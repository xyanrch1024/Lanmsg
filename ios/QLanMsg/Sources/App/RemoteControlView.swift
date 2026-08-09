import SwiftUI
import UIKit
import QLanMsgCore

/// Full-screen remote control: shows the desktop's JPEG frames, translates
/// touches into mouse/wheel input, and forwards hardware-keyboard presses.
struct RemoteControlView: View {
    @EnvironmentObject private var model: AppModel
    @ObservedObject var controller: RemoteController

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            if let image = controller.frameImage {
                Image(uiImage: image)
                    .resizable()
                    .scaledToFit()
                    .ignoresSafeArea()
            } else {
                ProgressView()
                    .tint(.white)
            }

            if controller.phase == .active {
                RemoteSurface(controller: controller)
                    .ignoresSafeArea()
            }

            VStack {
                Spacer()
                if controller.phase == .ended {
                    endedBar
                } else if controller.phase == .requesting {
                    Text("正在请求控制 \(controller.peer.name)…")
                        .font(.footnote)
                        .foregroundStyle(.white)
                        .padding(.horizontal, 16)
                        .padding(.vertical, 8)
                        .background(.black.opacity(0.6), in: Capsule())
                }
                if controller.phase == .active {
                    controlBar
                }
            }
            .padding(.bottom, 8)
        }
        .onDisappear {
            if controller.phase == .active {
                controller.stop()
            }
            if model.remoteSession === controller {
                model.remoteSession = nil
            }
        }
    }

    private var controlBar: some View {
        HStack(spacing: 12) {
            ToolButton(icon: "escape", label: "Esc") {
                controller.key(QtKey.escape, down: true)
                controller.key(QtKey.escape, down: false)
            }
            ToolButton(icon: "command", label: "Ctrl+Alt+Del") {
                controller.sendCtrlAltDel()
            }
            Spacer()
            Button {
                model.endRemoteControl()
            } label: {
                Label("结束", systemImage: "stop.fill")
                    .font(.subheadline.weight(.semibold))
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                    .background(.red, in: Capsule())
                    .foregroundStyle(.white)
            }
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 16))
        .padding(.horizontal, 8)
    }

    private var endedBar: some View {
        VStack(spacing: 10) {
            Text(controller.statusMessage.isEmpty ? "远程控制已结束" : controller.statusMessage)
                .font(.subheadline)
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
            Button {
                model.endRemoteControl()
            } label: {
                Text("返回")
                    .font(.subheadline.weight(.semibold))
                    .padding(.horizontal, 32)
                    .padding(.vertical, 10)
                    .background(.white, in: Capsule())
                    .foregroundStyle(.black)
            }
        }
        .padding(20)
        .background(.black.opacity(0.7), in: RoundedRectangle(cornerRadius: 16))
        .padding(.horizontal, 40)
    }
}

private struct ToolButton: View {
    let icon: String
    let label: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Label(label, systemImage: icon)
                .font(.subheadline)
                .padding(.horizontal, 14)
                .padding(.vertical, 10)
                .background(.white.opacity(0.15), in: Capsule())
                .foregroundStyle(.white)
        }
    }
}

// MARK: - Touch + keyboard surface

/// Transparent overlay that turns touches and hardware-keyboard presses into
/// RcInput events. Coordinates are mapped from the local (aspect-fit) frame to
/// absolute remote screen coordinates.
private struct RemoteSurface: UIViewRepresentable {
    let controller: RemoteController

    func makeCoordinator() -> Coordinator {
        Coordinator(controller: controller)
    }

    func makeUIView(context: Context) -> RemoteSurfaceView {
        let view = RemoteSurfaceView()
        view.coordinator = context.coordinator
        return view
    }

    func updateUIView(_ uiView: RemoteSurfaceView, context: Context) {
        uiView.coordinator = context.coordinator
        context.coordinator.controller = controller
        if controller.phase == .active, !uiView.isFirstResponder {
            DispatchQueue.main.async {
                if !uiView.isFirstResponder { uiView.becomeFirstResponder() }
            }
        }
    }

    final class Coordinator {
        weak var controller: RemoteController?
        init(controller: RemoteController) { self.controller = controller }
    }
}

private final class RemoteSurfaceView: UIView {

    weak var coordinator: RemoteSurface.Coordinator?

    private var activeTouches: [UITouch: CGPoint] = [:]
    private var touchStart: [UITouch: CGPoint] = [:]
    private var isLeftDown = false
    private var isRightDrag = false
    private var twoFingerTap = false
    private var lastCentroid: CGPoint?
    private var scrollAccum = CGPoint.zero
    private var longPressTimer: Timer?
    private var longPressFired = false
    private var heldModifiers: Set<UInt32> = []

    override var canBecomeFirstResponder: Bool { true }

    private var controller: RemoteController? { coordinator?.controller }

    // MARK: touches

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            let loc = t.location(in: self)
            activeTouches[t] = loc
            touchStart[t] = loc
        }
        guard let controller else { return }

        if activeTouches.count == 1 {
            guard let point = mappedPoint(for: activeTouches.values.first!) else { return }
            controller.mouseMove(x: point.x, y: point.y)
            controller.mouseButton(1, down: true, x: point.x, y: point.y)
            isLeftDown = true
            longPressFired = false
            scheduleLongPress()
            lastCentroid = nil
            scrollAccum = .zero
        } else if activeTouches.count == 2 {
            longPressTimer?.invalidate()
            // If we were mid-left-drag, cancel it so the button isn't stuck down.
            if isLeftDown {
                controller.mouseButton(1, down: false, x: -1, y: -1)
                isLeftDown = false
            }
            lastCentroid = centroid()
            scrollAccum = .zero
            twoFingerTap = true
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches { activeTouches[t] = t.location(in: self) }
        guard let controller else { return }

        if activeTouches.count == 1 {
            if isRightDrag {
                guard let point = mappedPoint(for: activeTouches.values.first!) else { return }
                controller.mouseMove(x: point.x, y: point.y)
            } else {
                if let t = touches.first, let start = touchStart[t] {
                    let current = t.location(in: self)
                    let dx = start.x - current.x
                    let dy = start.y - current.y
                    if dx * dx + dy * dy > 36 { // moved > 6pt
                        longPressTimer?.invalidate()
                    }
                }
                guard let point = mappedPoint(for: activeTouches.values.first!) else { return }
                controller.mouseMove(x: point.x, y: point.y)
            }
        } else if activeTouches.count == 2 {
            twoFingerTap = false
            guard let current = centroid() else { return }
            if let prev = lastCentroid {
                let dx = current.x - prev.x
                let dy = current.y - prev.y
                if abs(dy) > 0.5 || abs(dx) > 0.5 {
                    twoFingerTap = false
                }
                scrollAccum.x += dx
                scrollAccum.y += dy
                let step: CGFloat = 12
                let unitsY = Int(scrollAccum.y / step)
                let unitsX = Int(scrollAccum.x / step)
                if unitsY != 0 || unitsX != 0 {
                    controller.wheel(dx: unitsX, dy: unitsY)
                    scrollAccum.x -= CGFloat(unitsX) * step
                    scrollAccum.y -= CGFloat(unitsY) * step
                }
            }
            lastCentroid = current
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        touchesEndedOrCancelled(touches)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        touchesEndedOrCancelled(touches)
    }

    private func touchesEndedOrCancelled(_ touches: Set<UITouch>) {
        guard let controller else { return }
        let wasTwo = activeTouches.count == 2
        for t in touches {
            activeTouches.removeValue(forKey: t)
            touchStart.removeValue(forKey: t)
        }
        longPressTimer?.invalidate()

        if wasTwo && activeTouches.count == 0 && twoFingerTap {
            // two-finger tap -> middle click
            if let touch = touches.first, let point = mappedPoint(for: touch.location(in: self)) {
                controller.mouseButton(2, down: true, x: point.x, y: point.y)
                controller.mouseButton(2, down: false, x: point.x, y: point.y)
            }
        } else if activeTouches.count == 0 {
            if isRightDrag {
                controller.mouseButton(3, down: false, x: -1, y: -1)
                isRightDrag = false
            } else if longPressFired {
                controller.mouseButton(3, down: false, x: -1, y: -1)
            } else if isLeftDown {
                if let touch = touches.first, let point = mappedPoint(for: touch.location(in: self)) {
                    controller.mouseButton(1, down: false, x: point.x, y: point.y)
                }
            }
            isLeftDown = false
        }
        twoFingerTap = false
        lastCentroid = nil
        scrollAccum = .zero
    }

    private func centroid() -> CGPoint? {
        let values = Array(activeTouches.values)
        guard !values.isEmpty else { return nil }
        let sum = values.reduce(CGPoint.zero) { CGPoint(x: $0.x + $1.x, y: $0.y + $1.y) }
        return CGPoint(x: sum.x / CGFloat(values.count), y: sum.y / CGFloat(values.count))
    }

    private func scheduleLongPress() {
        longPressTimer?.invalidate()
        longPressTimer = Timer.scheduledTimer(withTimeInterval: 0.55, repeats: false) { [weak self] _ in
            self?.fireLongPress()
        }
    }

    private func fireLongPress() {
        guard let controller, activeTouches.count == 1,
              let point = mappedPoint(for: activeTouches.values.first!) else { return }
        controller.mouseButton(1, down: false, x: point.x, y: point.y) // cancel left
        controller.mouseButton(3, down: true, x: point.x, y: point.y)  // right down
        isLeftDown = false
        isRightDrag = true
        longPressFired = true
    }

    /// Maps a local point (aspect-fit) to absolute remote coordinates.
    private func mappedPoint(for point: CGPoint) -> (x: Int, y: Int)? {
        guard let controller else { return nil }
        let size = controller.frameSize
        guard size.width > 0, size.height > 0, bounds.width > 0, bounds.height > 0 else { return nil }
        let scale = min(bounds.width / size.width, bounds.height / size.height)
        let rectW = size.width * scale
        let rectH = size.height * scale
        let rectX = (bounds.width - rectW) / 2
        let rectY = (bounds.height - rectH) / 2
        let x = (point.x - rectX) / rectW
        let y = (point.y - rectY) / rectH
        guard x >= 0, y >= 0, x <= 1, y <= 1 else { return nil }
        return (Int(x * size.width), Int(y * size.height))
    }

    // MARK: hardware keyboard

    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let flags = event?.modifierFlags ?? []
        for press in presses {
            let hid = UInt32(press.key?.keyCode.rawValue ?? 0)
            handleKey(hid: hid, flags: flags, down: true)
        }
    }

    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let flags = event?.modifierFlags ?? []
        for press in presses {
            let hid = UInt32(press.key?.keyCode.rawValue ?? 0)
            handleKey(hid: hid, flags: flags, down: false)
        }
    }

    override func pressesCancelled(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        pressesEnded(presses, with: event)
    }

    private func handleKey(hid: UInt32, flags: UIKeyModifierFlags, down: Bool) {
        guard let controller, controller.phase == .active else { return }

        // Physical modifier keys (0xE0...0xE7) map directly.
        let isModifier = (hid >= 0xE0 && hid <= 0xE7)
        let qt = KeyMapping.qtKey(forHID: hid)
        guard qt != 0 else { return }

        if isModifier {
            controller.key(qt, down: down)
            if down { heldModifiers.insert(hid) } else { heldModifiers.remove(hid) }
            return
        }

        if down {
            ensureModifiers(flags)
            controller.key(qt, down: true)
        } else {
            controller.key(qt, down: false)
        }
    }

    /// Synthesizes modifier key-downs (e.g. Cmd for Cmd+C) that are present in
    /// `flags` but not yet reported as held. Release is handled by the physical
    /// modifier key's own press events.
    private func ensureModifiers(_ flags: UIKeyModifierFlags) {
        guard let controller else { return }
        let pairs: [(UIKeyModifierFlags, Int, UInt32)] = [
            (.shift, KeyMods.shift, 0xE1),
            (.control, KeyMods.control, 0xE0),
            (.command, KeyMods.meta, 0xE3),
            (.alternate, KeyMods.alt, 0xE2),
        ]
        for (flag, modMask, hid) in pairs where flags.contains(flag) {
            guard !heldModifiers.contains(hid) else { continue }
            heldModifiers.insert(hid)
            controller.key(KeyMods.qtKey(modMask), down: true)
        }
    }
}
