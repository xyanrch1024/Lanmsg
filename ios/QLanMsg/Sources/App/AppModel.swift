import Foundation
import Combine

/// Central app coordinator. Owns the Discovery / Transport instances and routes
/// incoming frames to the right per-peer feature (chat, file transfer, remote
/// control). All @Published state is mutated on the main thread; Core callbacks
/// arrive on background queues and are hopped over.
final class AppModel: ObservableObject {

    static let shared = AppModel()

    @Published private(set) var peers: [Peer] = []
    @Published private(set) var messages: [String: [ChatMessage]] = [:] // by peer ip
    @Published private(set) var transfers: [TransferRecord] = []
    @Published private(set) var currentPeer: Peer?

    /// Set when a peer offers a file; the UI presents an accept/decline alert.
    @Published var pendingFileOffer: IncomingFileOffer?

    /// The active remote-control session, if any. Present it as a full-screen cover.
    @Published var remoteSession: RemoteController?

    /// Human-readable one-shot notifications surfaced in the device list.
    @Published var notice: String? {
        didSet { if notice != nil { scheduleNoticeClear() } }
    }

    let settings = SettingsStore.shared

    private let discovery: Discovery
    private let transport: Transport

    private var senders: [String: FileSender] = [:]          // by token
    private var receivers: [String: FileReceiver] = [:]      // by token
    private var waitingSenders: [String: FileSender] = [:]   // by peer ip, until session ready
    private var receiversByIp: [String: String] = [:]        // ip -> token of active receive

    private init() {
        let localIP = NetInfo.localIPv4 ?? "127.0.0.1"
        let identity = Identity(appId: settings.appId, name: settings.nickname, initIp: localIP)
        transport = Transport(identity: identity, listenerPort: settings.listenPort)
        discovery = Discovery(settings: settings, tcpPort: settings.listenPort)

        discovery.onPeerAdded = { [weak self] p in self?.mainAsync { self?.upsertPeer(p) } }
        discovery.onPeerUpdated = { [weak self] p in self?.mainAsync { self?.upsertPeer(p) } }
        discovery.onPeerRemoved = { [weak self] id in
            self?.mainAsync { self?.peers.removeAll { $0.id == id } }
        }

        transport.onSessionReady = { [weak self] ip, name in
            self?.mainAsync { self?.onSessionReady(ip: ip, name: name) }
        }
        transport.onSessionClosed = { [weak self] ip in
            self?.mainAsync { self?.onSessionClosed(ip: ip) }
        }
        transport.onFrame = { [weak self] ip, type, json, body in
            self?.mainAsync { self?.handleFrame(ip: ip, type: type, json: json, body: body) }
        }

        discovery.start()
        transport.startListening()
    }

    private func mainAsync(_ block: @escaping () -> Void) {
        DispatchQueue.main.async(execute: block)
    }

    private var noticeClearWork: DispatchWorkItem?
    private func scheduleNoticeClear() {
        noticeClearWork?.cancel()
        let work = DispatchWorkItem { [weak self] in self?.notice = nil }
        noticeClearWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 4, execute: work)
    }

    // MARK: - peers

    private func upsertPeer(_ p: Peer) {
        if let idx = peers.firstIndex(where: { $0.id == p.id }) {
            peers[idx] = p
        } else {
            peers.append(p)
        }
        peers.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    func peer(byIp ip: String) -> Peer? {
        peers.first { $0.ip == ip }
    }

    func select(peer: Peer) {
        currentPeer = peer
        transport.connect(to: peer)
    }

    // MARK: - chat

    func sendChat(_ text: String, to peer: Peer) {
        transport.connect(to: peer)
        let ts = Date().timeIntervalSince1970 * 1000
        let msg = ChatMessage(who: settings.nickname, text: text, ts: ts, isMe: true)
        messages[peer.ip, default: []].append(msg)
        transport.send(peer.ip, .chat, json: ["text": text, "ts": ts])
    }

    // MARK: - file transfer (send)

    func sendFile(url: URL, to peer: Peer) {
        let token = UUID().uuidString
        let record = TransferRecord(token: token, direction: .send, peerName: peer.name,
                                    fileName: url.lastPathComponent, total: 0, status: "等待连接")
        transfers.insert(record, at: 0)

        let sender = FileSender(ip: peer.ip, token: token, url: url, transport: transport)
        senders[token] = sender
        sender.onProgress = { [weak self] sent, total in
            self?.mainAsync { record.progress(sent: sent, total: total) }
        }
        sender.onFinish = { [weak self] ok, info in
            self?.mainAsync {
                record.finish(ok: ok, info: info)
                self?.senders[token] = nil
            }
        }

        if transport.hasSession(peer.ip) {
            sender.begin(peerName: peer.name)
        } else {
            waitingSenders[peer.ip] = sender
            transport.connect(to: peer)
        }
    }

    private func onSessionReady(ip: String, name: String) {
        if let sender = waitingSenders.removeValue(forKey: ip) {
            sender.begin(peerName: name.isEmpty ? ip : name)
        }
    }

    // MARK: - file transfer (receive)

    func respond(to offer: IncomingFileOffer, accept: Bool) {
        pendingFileOffer = nil
        let ip = offer.peerIp
        let token = offer.token
        if !accept {
            transport.send(ip, .fileDecline, json: ["token": token, "reason": "用户拒绝接收"])
            return
        }

        let record = TransferRecord(token: token, direction: .receive, peerName: offer.peerName,
                                    fileName: offer.name, total: offer.size, status: "接收中")
        transfers.insert(record, at: 0)

        let receiver = FileReceiver(ip: ip, token: token, name: offer.name, size: offer.size,
                                    transport: transport)
        receivers[token] = receiver
        receiversByIp[ip] = token
        receiver.onProgress = { [weak self] got in
            self?.mainAsync { record.progress(sent: got, total: offer.size) }
        }
        receiver.onFinish = { [weak self] ok, info in
            self?.mainAsync {
                record.finish(ok: ok, info: info)
                record.filePath = receiver.destinationPath
                if self?.receivers[token] === receiver { self?.receivers[token] = nil }
                self?.receiversByIp[ip] = nil
            }
        }
        receiver.start()
        transport.send(ip, .fileAccept, json: ["token": token])
    }

    // MARK: - remote control (controller side, iPhone -> desktop)

    func requestRemoteControl(of peer: Peer) {
        guard remoteSession == nil else {
            notice = "已有远程控制会话进行中"
            return
        }
        transport.connect(to: peer)
        let controller = RemoteController(peer: peer, transport: transport,
                                          password: settings.remotePassword)
        remoteSession = controller
        controller.request()
    }

    func endRemoteControl() {
        remoteSession?.stop()
        remoteSession = nil
    }

    // MARK: - transfer management

    func cancelTransfer(_ record: TransferRecord) {
        senders[record.token]?.cancel()
        receivers[record.token]?.cancel()
    }

    // MARK: - frame routing

    private func handleFrame(ip: String, type: MsgType, json: [String: Any], body: Data) {
        switch type {
        case .chat:
            let text = json["text"] as? String ?? ""
            let ts = (json["ts"] as? NSNumber)?.doubleValue ?? 0
            let who = peer(byIp: ip)?.name ?? ip
            messages[ip, default: []].append(ChatMessage(who: who, text: text, ts: ts, isMe: false))

        case .fileOffer:
            guard let token = json["token"] as? String,
                  let name = json["name"] as? String,
                  pendingFileOffer == nil else { return }
            let size = (json["size"] as? NSNumber)?.int64Value ?? 0
            let who = peer(byIp: ip)?.name ?? ip
            pendingFileOffer = IncomingFileOffer(peerIp: ip, peerName: who,
                                                 token: token, name: name, size: size)

        case .fileAccept:
            if let token = json["token"] as? String, let s = senders[token] {
                s.onAccepted()
            }

        case .fileDecline:
            if let token = json["token"] as? String, let s = senders[token] {
                s.onDeclined(reason: json["reason"] as? String ?? "对方拒绝了文件传输")
            }

        case .fileChunk:
            guard let token = json["token"] as? String,
                  let seq = (json["seq"] as? NSNumber)?.int64Value else { return }
            if let r = receivers[token] {
                r.onChunk(seq: seq, data: body)
            }

        case .fileDone:
            if let token = json["token"] as? String, let r = receivers[token] {
                r.onDone()
            }

        case .fileCancel:
            if let token = json["token"] as? String {
                let reason = json["reason"] as? String ?? "传输被取消"
                senders[token]?.onCanceled(reason: reason)
                receivers[token]?.onCanceled(reason: reason)
            }

        case .rcRequest:
            // iOS can be the controller, not the controlled: reject incoming requests.
            if let token = json["token"] as? String {
                transport.send(ip, .rcDecline, json: ["token": token, "reason": "iOS 设备不支持被远程控制"])
            }

        case .rcAccept:
            if let token = json["token"] as? String, remoteSession?.token == token {
                remoteSession?.onAccepted()
            }

        case .rcDecline:
            if let token = json["token"] as? String, remoteSession?.token == token {
                remoteSession?.onDeclined(reason: json["reason"] as? String ?? "对方拒绝了控制请求")
            }

        case .rcFrame:
            guard let token = json["token"] as? String, remoteSession?.token == token else { return }
            let w = json["w"] as? Int ?? 0
            let h = json["h"] as? Int ?? 0
            remoteSession?.onFrame(jpeg: body, w: w, h: h)

        case .rcStop:
            if let token = json["token"] as? String, remoteSession?.token == token {
                remoteSession?.onStopped()
            }

        default:
            break
        }
    }

    private func onSessionClosed(ip: String) {
        if remoteSession?.peer.ip == ip {
            remoteSession?.onSessionLost()
        }
        if let token = receiversByIp[ip], let r = receivers[token] {
            r.onCanceled(reason: "连接已断开")
        }
    }
}
