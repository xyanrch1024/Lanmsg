import Foundation

/// An input event sent to the controlled machine. JSON keys (`t/x/y/b/dx/dy/k`)
/// and numeric encodings match `qlm::inputToJson` in `src/common/Protocol.h`.
public struct InputEvent {
    public enum Kind: Int {
        case mouseMove = 0
        case mouseDown = 1
        case mouseUp = 2
        case wheel = 3
        case keyDown = 4
        case keyUp = 5
    }

    public var type: Kind = .mouseMove
    public var x: Int = 0 // absolute screen coords
    public var y: Int = 0
    public var button: Int = 0 // 1=left 2=middle 3=right
    public var dx: Int = 0 // wheel horizontal
    public var dy: Int = 0 // wheel vertical
    public var key: Int = 0 // Qt::Key

    public init() {}

    public init(type: Kind, x: Int = 0, y: Int = 0, button: Int = 0,
                dx: Int = 0, dy: Int = 0, key: Int = 0) {
        self.type = type
        self.x = x
        self.y = y
        self.button = button
        self.dx = dx
        self.dy = dy
        self.key = key
    }

    public var json: [String: Any] {
        ["t": type.rawValue, "x": x, "y": y, "b": button, "dx": dx, "dy": dy, "k": key]
    }

    public static func fromJSON(_ o: [String: Any]) -> InputEvent {
        var ev = InputEvent()
        ev.type = Kind(rawValue: o["t"] as? Int ?? 0) ?? .mouseMove
        ev.x = o["x"] as? Int ?? 0
        ev.y = o["y"] as? Int ?? 0
        ev.button = o["b"] as? Int ?? 0
        ev.dx = o["dx"] as? Int ?? 0
        ev.dy = o["dy"] as? Int ?? 0
        ev.key = o["k"] as? Int ?? 0
        return ev
    }
}

/// Modifier bitmask used by the remote-control keyboard path.
public enum KeyMods {
    public static let shift = 1 << 0
    public static let control = 1 << 1
    public static let alt = 1 << 2
    public static let meta = 1 << 3
    public static let all = shift | control | alt | meta

    public static func qtKey(_ mod: Int) -> Int {
        switch mod {
        case shift: return QtKey.shift
        case control: return QtKey.control
        case alt: return QtKey.alt
        case meta: return QtKey.meta
        default: return 0
        }
    }
}

/// Qt::Key values referenced by the iOS keyboard path (see Qt sources).
public enum QtKey {
    public static let escape = 0x01000000
    public static let tab = 0x01000001
    public static let backtab = 0x01000002
    public static let backspace = 0x01000003
    public static let `return` = 0x01000004
    public static let enter = 0x01000005
    public static let insert = 0x01000006
    public static let delete = 0x01000007
    public static let pause = 0x01000008
    public static let print = 0x01000009
    public static let clear = 0x0100000B
    public static let home = 0x01000010
    public static let end = 0x01000011
    public static let left = 0x01000012
    public static let up = 0x01000013
    public static let right = 0x01000014
    public static let down = 0x01000015
    public static let pageUp = 0x01000016
    public static let pageDown = 0x01000017
    public static let shift = 0x01000020
    public static let control = 0x01000021
    public static let meta = 0x01000022
    public static let alt = 0x01000023
    public static let capsLock = 0x01000024
    public static let numLock = 0x01000025
    public static let scrollLock = 0x01000026
    public static let f1 = 0x01000030
    public static let menu = 0x01000055
    public static let help = 0x01000058
}
