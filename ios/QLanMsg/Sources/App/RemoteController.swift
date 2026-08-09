import Foundation
import UIKit

/// One remote-control session where the iPhone is the *controller*. Sends
/// RcRequest and forwards RcInput events; AppModel routes RcAccept / RcDecline /
/// RcFrame / RcStop here by matching token.
final class RemoteController: Identifiable, ObservableObject {

    enum Phase {
        case requesting
        case active
        case ended
    }

    let id = UUID()
    let peer: Peer
    let token: String
    private let transport: Transport
    private let password: String

    @Published private(set) var phase: Phase = .requesting
    @Published private(set) var frameImage: UIImage?
    @Published private(set) var frameSize: CGSize = .zero
    @Published private(set) var statusMessage: String = ""

    private var stopping = false

    init(peer: Peer, transport: Transport, password: String) {
        self.peer = peer
        self.token = UUID().uuidString
        self.transport = transport
        self.password = password
    }

    // MARK: - lifecycle

    func request() {
        transport.send(peer.ip, .rcRequest, json: ["token": token, "password": password])
    }

    func onAccepted() {
        phase = .active
        statusMessage = ""
    }

    func onDeclined(reason: String) {
        phase = .ended
        statusMessage = reason
    }

    func onFrame(jpeg: Data, w: Int, h: Int) {
        guard w > 0, h > 0 else { return }
        frameSize = CGSize(width: w, height: h)
        frameImage = UIImage(data: jpeg)
    }

    func onStopped() {
        phase = .ended
        statusMessage = "对方已断开远程控制"
    }

    func onSessionLost() {
        phase = .ended
        statusMessage = "连接已断开"
    }

    /// User taps the end button.
    func stop() {
        guard !stopping else { return }
        stopping = true
        transport.send(peer.ip, .rcStop, json: ["token": token])
        phase = .ended
    }

    // MARK: - input

    func sendInput(_ ev: InputEvent) {
        guard phase == .active else { return }
        var json = ev.json
        json["token"] = token
        transport.send(peer.ip, .rcInput, json: json)
    }

    func mouseMove(x: Int, y: Int) {
        sendInput(InputEvent(type: .mouseMove, x: x, y: y))
    }

    func mouseButton(_ button: Int, down: Bool, x: Int, y: Int) {
        let kind: InputEvent.Kind = down ? .mouseDown : .mouseUp
        sendInput(InputEvent(type: kind, x: x, y: y, button: button))
    }

    func wheel(dx: Int, dy: Int) {
        sendInput(InputEvent(type: .wheel, dx: dx, dy: dy))
    }

    func key(_ qtKey: Int, down: Bool) {
        let kind: InputEvent.Kind = down ? .keyDown : .keyUp
        sendInput(InputEvent(type: kind, key: qtKey))
    }

    /// Sends a full press+release of a modifier key.
    func tapModifier(_ qtKey: Int) {
        guard phase == .active else { return }
        key(qtKey, down: true)
        key(qtKey, down: false)
    }

    /// Sends Ctrl+Alt+Del as a chord.
    func sendCtrlAltDel() {
        guard phase == .active else { return }
        key(QtKey.control, down: true)
        key(QtKey.alt, down: true)
        key(QtKey.delete, down: true)
        key(QtKey.delete, down: false)
        key(QtKey.alt, down: false)
        key(QtKey.control, down: false)
    }
}
