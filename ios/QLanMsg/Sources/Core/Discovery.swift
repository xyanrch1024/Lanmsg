import Foundation
import Darwin

/// Local network identity helpers (no UIKit dependency).
public enum NetInfo {
    /// First non-loopback IPv4 of a Wi-Fi / ethernet interface.
    public static var localIPv4: String? {
        var ifaddr: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&ifaddr) == 0, let first = ifaddr else { return nil }
        defer { freeifaddrs(first) }
        var ptr: UnsafeMutablePointer<ifaddrs>? = first
        while let p = ptr {
            if let addr = p.pointee.ifa_addr,
               addr.pointee.sa_family == sa_family_t(AF_INET) {
                let name = String(cString: p.pointee.ifa_name)
                if name.hasPrefix("en") || name.hasPrefix("wl") {
                    let ip = addr.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { sa -> String in
                        var buffer = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
                        inet_ntop(AF_INET, &sa.pointee.sin_addr, &buffer, socklen_t(buffer.count))
                        return String(cString: buffer)
                    }
                    if !ip.isEmpty && !ip.hasPrefix("127.") {
                        return ip
                    }
                }
            }
            ptr = p.pointee.ifa_next
        }
        return nil
    }

    public static var hostName: String {
        ProcessInfo.processInfo.hostName
    }

    public static var osString: String {
        let v = ProcessInfo.processInfo.operatingSystemVersion
        return "iOS \(v.majorVersion).\(v.minorVersion).\(v.patchVersion)"
    }
}

/// LAN peer discovery over a raw POSIX UDP socket bound to the shared port
/// 24260. Mirrors `PeerDiscovery` on the desktop:
///   - broadcast to 255.255.255.255 every 3s
///   - unicast to known peers every 3s
///   - reply to a sender immediately
///   - drop peers not seen for 10s
/// Multicast joining is intentionally skipped on iOS (requires the App Store
/// "multicast-networking" entitlement and is unnecessary for a single
/// instance per device).
public final class Discovery {
    public var onPeerAdded: ((Peer) -> Void)?
    public var onPeerUpdated: ((Peer) -> Void)?
    public var onPeerRemoved: ((String) -> Void)?

    private let fd: Int32
    private let queue = DispatchQueue(label: "qlanmsg.discovery")
    private let settings: SettingsStore
    private let tcpPort: UInt16
    private var peers: [String: Peer] = [:]
    private var added: Set<String> = []
    private var broadcastTimer: DispatchSourceTimer?
    private var cleanupTimer: DispatchSourceTimer?
    private var running = false

    public init(settings: SettingsStore, tcpPort: UInt16) {
        self.settings = settings
        self.tcpPort = tcpPort
        let s = socket(AF_INET, SOCK_DGRAM, 0)
        if s >= 0 {
            var reuse: Int32 = 1
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, socklen_t(MemoryLayout<Int32>.size))
            var bcast: Int32 = 1
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, &bcast, socklen_t(MemoryLayout<Int32>.size))
            var addr = sockaddr_in()
            addr.sin_family = sa_family_t(AF_INET)
            addr.sin_port = ProtocolSpec.udpPort.bigEndian
            addr.sin_addr.s_addr = inet_addr("0.0.0.0")
            _ = withUnsafePointer(to: &addr) {
                $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                    bind(s, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
        fd = s
    }

    deinit { stop() }

    public func start() {
        guard !running else { return }
        running = true
        queue.async { [weak self] in self?.readLoop() }

        let bt = DispatchSource.makeTimerSource(queue: queue)
        bt.schedule(deadline: .now() + ProtocolSpec.discoveryInterval,
                    repeating: ProtocolSpec.discoveryInterval)
        bt.setEventHandler { [weak self] in self?.broadcast() }
        broadcastTimer = bt
        bt.resume()

        let ct = DispatchSource.makeTimerSource(queue: queue)
        ct.schedule(deadline: .now() + 2.0, repeating: 2.0)
        ct.setEventHandler { [weak self] in self?.cleanup() }
        cleanupTimer = ct
        ct.resume()
    }

    public func stop() {
        running = false
        broadcastTimer?.cancel()
        broadcastTimer = nil
        cleanupTimer?.cancel()
        cleanupTimer = nil
        if fd >= 0 { close(fd) }
    }

    public func peer(byId id: String) -> Peer? { peers[id] }

    public func peer(byIp ip: String) -> Peer? {
        peers.values.first { $0.ip == ip }
    }

    public var peerList: [Peer] { Array(peers.values) }

    // MARK: - sending

    private func packet() -> Data {
        var o: [String: Any] = [:]
        o["magic"] = ProtocolSpec.magic
        o["ver"] = 1
        o["id"] = settings.appId
        o["name"] = settings.nickname
        o["host"] = NetInfo.hostName
        o["os"] = NetInfo.osString
        o["app"] = ProtocolSpec.appVersion
        o["port"] = Int(tcpPort)
        return (try? JSONSerialization.data(withJSONObject: o, options: [])) ?? Data()
    }

    private func broadcast() {
        let data = packet()
        sendPacket(data, to: ProtocolSpec.broadcastAddress, port: ProtocolSpec.udpPort)
        for p in peers.values where !p.ip.isEmpty {
            sendPacket(data, to: p.ip, port: ProtocolSpec.udpPort)
        }
    }

    private func sendPacket(_ data: Data, to ip: String, port: UInt16) {
        guard fd >= 0 else { return }
        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = port.bigEndian
        addr.sin_addr.s_addr = inet_addr(ip)
        data.withUnsafeBytes { raw in
            withUnsafePointer(to: &addr) {
                $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { sa in
                    sendto(fd, raw.baseAddress, raw.count, 0, sa,
                           socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
    }

    // MARK: - receiving

    private func readLoop() {
        var buf = [UInt8](repeating: 0, count: 65536)
        while running {
            var addr = sockaddr()
            var len = socklen_t(MemoryLayout<sockaddr>.size)
            let n = withUnsafeMutablePointer(to: &addr) { ptr -> Int in
                buf.withUnsafeMutableBytes { raw -> Int in
                    recvfrom(fd, raw.baseAddress, raw.count, 0, ptr, &len)
                }
            }
            if n < 0 { continue }
            let sender = withUnsafeMutablePointer(to: &addr) { ptr -> sockaddr_in in
                ptr.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { $0.pointee }
            }
            let ip = inetNtoA(sender.sin_addr)
            let port = sender.sin_port.bigEndian
            let data = Data(buf[0..<n])
            handleDatagram(data, from: ip, senderPort: port)
        }
    }

    private func handleDatagram(_ data: Data, from senderIP: String, senderPort: UInt16) {
        guard let obj = try? JSONSerialization.jsonObject(with: data),
              let o = obj as? [String: Any],
              o["magic"] as? String == ProtocolSpec.magic,
              let id = o["id"] as? String,
              !id.isEmpty,
              id != settings.appId else { return }

        let p = Peer(id: id,
                     name: o["name"] as? String ?? "",
                     host: o["host"] as? String ?? "",
                     os: o["os"] as? String ?? "",
                     ver: o["app"] as? String ?? "",
                     ip: senderIP,
                     tcpPort: UInt16(o["port"] as? Int ?? Int(ProtocolSpec.tcpPort)),
                     lastSeen: Date())
        let isNew = peers[p.id] == nil
        upsert(p, notify: true)

        // Reply only to a brand-new peer so it learns about us immediately.
        // Replying to every datagram makes two clients echo each other's
        // replies forever (same fix as the desktop PeerDiscovery.cpp).
        if isNew {
            sendPacket(packet(), to: senderIP, port: senderPort == 0 ? ProtocolSpec.udpPort : senderPort)
        }
    }

    private func upsert(_ p: Peer, notify: Bool) {
        if let old = peers[p.id] {
            peers[p.id] = p
            if notify && (old.ip != p.ip || old.name != p.name) {
                onPeerUpdated?(p)
            }
        } else {
            peers[p.id] = p
            if notify { onPeerAdded?(p) }
        }
    }

    private func cleanup() {
        let now = Date()
        let expired = peers.compactMap { id, p -> String? in
            now.timeIntervalSince(p.lastSeen) > ProtocolSpec.peerTimeout ? id : nil
        }
        for id in expired {
            peers.removeValue(forKey: id)
            onPeerRemoved?(id)
        }
    }

    private func inetNtoA(_ addr: in_addr) -> String {
        var a = addr
        var buffer = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
        inet_ntop(AF_INET, &a, &buffer, socklen_t(buffer.count))
        return String(cString: buffer)
    }
}
