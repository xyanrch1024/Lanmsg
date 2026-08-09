import Foundation
import Network

/// Identity we advertise in the TCP Hello handshake.
public struct Identity {
    public let appId: String
    public let name: String
    public let initIp: String

    public init(appId: String, name: String, initIp: String) {
        self.appId = appId
        self.name = name
        self.initIp = initIp
    }
}

/// A single TCP connection to one peer, wire-compatible with `PeerSession`
/// on the desktop. Frames are queued until the session is "ready":
///   - outgoing: ready right after we send our Hello
///   - incoming: ready once we receive the peer's Hello
/// The desktop never replies to a Hello, so only the initiating side sends one.
public final class Session {
    public let ip: String
    public let isIncoming: Bool
    public private(set) var ready = false
    public private(set) var peerName = ""
    /// "initIp|seq|appId" used for connection dedupe: our own key for outgoing,
    /// the peer's key for incoming (set when its Hello arrives).
    public private(set) var helloKey = ""
    /// Set when this session lost a dedupe race; its close is not surfaced.
    public var dropped = false

    public var onReady: (() -> Void)?
    public var onFrame: ((MsgType, [String: Any], Data) -> Void)?
    public var onClosed: (() -> Void)?
    /// (peerName, peerHelloKey)
    public var onHello: ((String, String) -> Void)?

    private let conn: NWConnection
    private let queue: DispatchQueue
    private let identity: Identity
    private let mySeq: UInt64
    private var buffer: [UInt8] = []
    private var pending: [Data] = []
    private var pendingCompletions: [(() -> Void)?] = []

    public init(connection: NWConnection, ip: String, isIncoming: Bool,
                identity: Identity, mySeq: UInt64, queue: DispatchQueue) {
        self.conn = connection
        self.ip = ip
        self.isIncoming = isIncoming
        self.identity = identity
        self.mySeq = mySeq
        self.queue = queue
    }

    public func start() {
        conn.stateUpdateHandler = { [weak self] state in self?.handleState(state) }
        conn.start(queue: queue)
        startReceive()
    }

    public func close() {
        conn.cancel()
    }

    public func send(_ type: MsgType, json: [String: Any] = [:], body: Data = Data()) {
        let frame = Framing.makeFrame(type, json: json, body: body)
        if ready {
            sendFrame(frame)
        } else {
            pending.append(frame)
            pendingCompletions.append(nil)
        }
    }

    /// Sends a frame and invokes `completion` once the socket has accepted the
    /// data, giving the file-sender a way to apply backpressure without relying
    /// on internal TCP buffer heuristics.
    public func sendChunk(_ type: MsgType, json: [String: Any] = [:], body: Data = Data(),
                          completion: (() -> Void)? = nil) {
        let frame = Framing.makeFrame(type, json: json, body: body)
        if ready {
            if let completion {
                conn.send(content: frame, completion: .contentProcessed { _ in completion() })
            } else {
                sendFrame(frame)
            }
        } else {
            pending.append(frame)
            pendingCompletions.append(completion)
        }
    }

    // MARK: - internals

    private func handleState(_ state: NWConnection.State) {
        switch state {
        case .ready:
            if !isIncoming {
                sendHello()
            }
            setReady(true)
        case .failed, .cancelled:
            if !dropped {
                onClosed?()
            }
        default:
            break
        }
    }

    private func sendHello() {
        let hello: [String: Any] = [
            "id": identity.appId,
            "name": identity.name,
            "seq": NSNumber(value: mySeq),
            "initIp": identity.initIp,
            "appId": identity.appId
        ]
        helloKey = "\(identity.initIp)|\(mySeq)|\(identity.appId)"
        send(.hello, json: hello)
    }

    private func setReady(_ r: Bool) {
        guard !ready else { return }
        ready = r
        if r {
            for (i, f) in pending.enumerated() {
                if i < pendingCompletions.count, let c = pendingCompletions[i] {
                    conn.send(content: f, completion: .contentProcessed { _ in c() })
                } else {
                    sendFrame(f)
                }
            }
            pending.removeAll()
            pendingCompletions.removeAll()
            onReady?()
        }
    }

    private func sendFrame(_ frame: Data) {
        conn.send(content: frame, completion: .contentProcessed { _ in })
    }

    private func startReceive() {
        conn.receive(minimumIncompleteLength: 1, maximumLength: 256 * 1024) { [weak self] data, _, isComplete, _ in
            guard let self else { return }
            if let data, !data.isEmpty {
                self.buffer.append(contentsOf: data)
                _ = Framing.consumeFrames(from: &self.buffer) { type, json, body in
                    self.dispatch(type, json, body)
                }
            }
            if isComplete {
                self.handleState(.cancelled)
            } else {
                self.startReceive()
            }
        }
    }

    private func dispatch(_ type: MsgType, _ json: [String: Any], _ body: Data) {
        if type == .hello {
            peerName = json["name"] as? String ?? ""
            let seq = (json["seq"] as? NSNumber)?.uint64Value ?? 0
            let initIp = json["initIp"] as? String ?? ""
            let appId = json["appId"] as? String ?? ""
            helloKey = "\(initIp)|\(seq)|\(appId)"
            if isIncoming { setReady(true) }
            onHello?(peerName, helloKey)
            return
        }
        onFrame?(type, json, body)
    }
}

/// Owns all peer sessions: outgoing connections, the TCP listener, and the
/// connection dedupe that keeps the session with the smallest
/// (initIp|seq|appId) key when both sides connect simultaneously.
public final class Transport {
    public var onSessionReady: ((String, String) -> Void)? // ip, peerName
    public var onSessionClosed: ((String) -> Void)?
    public var onFrame: ((String, MsgType, [String: Any], Data) -> Void)?

    private let queue = DispatchQueue(label: "qlanmsg.transport")
    private let identity: Identity
    private let listenerPort: UInt16
    private var sessions: [String: Session] = [:]
    private var listener: NWListener?
    private var nextSeq: UInt64 = 0

    public init(identity: Identity, listenerPort: UInt16) {
        self.identity = identity
        self.listenerPort = listenerPort
    }

    public func startListening() {
        queue.async { [weak self] in self?.startListener() }
    }

    public func connect(to peer: Peer) {
        queue.async { [weak self] in self?.startOutgoing(ip: peer.ip, port: peer.tcpPort) }
    }

    /// True when a (live or connecting) session for `ip` is registered.
    public func hasSession(_ ip: String) -> Bool {
        var ok = false
        queue.sync { ok = sessions[ip] != nil }
        return ok
    }

    public func close(_ ip: String) {
        queue.async { [weak self] in self?.sessions[ip]?.close() }
    }

    public func send(_ ip: String, _ type: MsgType, json: [String: Any] = [:], body: Data = Data()) {
        queue.async { [weak self] in
            self?.sessions[ip]?.send(type, json: json, body: body)
        }
    }

    /// Sends a frame to `ip`, invoking `completion` on the transport queue once
    /// the socket has accepted it (used for backpressured file chunk streaming).
    public func sendChunk(_ ip: String, _ type: MsgType, json: [String: Any] = [:], body: Data = Data(),
                          completion: (() -> Void)? = nil) {
        queue.async { [weak self] in
            self?.sessions[ip]?.sendChunk(type, json: json, body: body, completion: completion)
        }
    }

    // MARK: - internals

    private func startListener() {
        guard let port = NWEndpoint.Port(rawValue: listenerPort) else { return }
        let params = NWParameters.tcp
        params.preferNoProxies = true
        do {
            let l = try NWListener(using: params, on: port)
            l.newConnectionHandler = { [weak self] conn in self?.accept(conn) }
            l.stateUpdateHandler = { _ in }
            l.start(queue: queue)
            listener = l
        } catch {
            // Port in use (e.g. second instance): inbound is degraded, outbound still works.
        }
    }

    private func accept(_ conn: NWConnection) {
        guard let ip = remoteIP(of: conn) else {
            conn.cancel()
            return
        }
        let session = Session(connection: conn, ip: ip, isIncoming: true,
                              identity: identity, mySeq: 0, queue: queue)
        register(session)
    }

    private func startOutgoing(ip: String, port: UInt16) {
        guard sessions[ip] == nil, let nwPort = NWEndpoint.Port(rawValue: port) else { return }
        let params = NWParameters.tcp
        params.preferNoProxies = true
        let conn = NWConnection(host: NWEndpoint.Host(ip), port: nwPort, using: params)
        nextSeq += 1
        let session = Session(connection: conn, ip: normalizeIp(ip), isIncoming: false,
                              identity: identity, mySeq: nextSeq, queue: queue)
        register(session)
    }

    private func register(_ s: Session) {
        s.onReady = { [weak self, weak s] in
            guard let self, let s else { return }
            self.onSessionReady?(s.ip, s.peerName)
        }
        s.onFrame = { [weak self, weak s] type, json, body in
            guard let self, let s else { return }
            self.onFrame?(s.ip, type, json, body)
        }
        s.onClosed = { [weak self, weak s] in
            guard let self, let s else { return }
            if self.sessions[s.ip] === s {
                self.sessions.removeValue(forKey: s.ip)
            }
            if !s.dropped {
                self.onSessionClosed?(s.ip)
            }
        }
        s.onHello = { [weak self, weak s] _, peerKey in
            guard let self, let s else { return }
            self.dedupe(s, peerKey: peerKey)
        }
        sessions[s.ip] = s
        s.start()
    }

    private func dedupe(_ s: Session, peerKey: String) {
        guard let other = sessions[s.ip], other !== s, !other.dropped else { return }
        let otherKey = other.helloKey
        let winner: Session = peerKey < otherKey ? s : other
        let loser: Session = winner === s ? other : s
        if winner !== loser {
            sessions[s.ip] = winner
            loser.dropped = true
            loser.close()
        }
    }

    private func remoteIP(of conn: NWConnection) -> String? {
        if case let .hostPort(host, _) = conn.endpoint {
            return normalizeIp(hostString(host))
        }
        if case let .hostPort(host, _)? = conn.currentPath?.remoteEndpoint {
            return normalizeIp(hostString(host))
        }
        return nil
    }

    private func hostString(_ host: NWEndpoint.Host) -> String {
        switch host {
        case .ipv4(let a): return a.debugDescription
        case .ipv6(let a): return a.debugDescription
        case .name(let n, _): return n
        default: return host.debugDescription
        }
    }

    private func normalizeIp(_ ip: String) -> String {
        if ip.hasPrefix("::ffff:") { return String(ip.dropFirst(7)) }
        return ip
    }
}
