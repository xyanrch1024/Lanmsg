import Foundation

/// Maps hardware keyboard HID usage codes (`UIPress.key.keyCode.rawValue`) to
/// Qt::Key integers understood by the desktop receiver. This is the iOS
/// analogue of `InputSinkWindows::keyToVk` / `InputSinkX11::keyToKeysym`.
public enum KeyMapping {

    /// Returns a Qt::Key integer, or 0 for unknown/unprintable usages.
    public static func qtKey(forHID usage: UInt32) -> Int {
        switch usage {
        // A-Z
        case 0x04...0x1D: return Int(0x41 + (usage - 0x04))
        // 1-9, 0
        case 0x1E...0x26: return Int(0x31 + (usage - 0x1E))
        case 0x27: return Int(0x30) // '0'
        case 0x28: return QtKey.return // Enter
        case 0x29: return QtKey.escape
        case 0x2A: return QtKey.backspace
        case 0x2B: return QtKey.tab
        case 0x2C: return 0x20 // Space
        case 0x2D: return 0x2D // '-'
        case 0x2E: return 0x3D // '='
        case 0x2F: return 0x5B // '['
        case 0x30: return 0x5D // ']'
        case 0x31: return 0x5C // '\\'
        case 0x33: return 0x3B // ';'
        case 0x34: return 0x27 // '\''
        case 0x35: return 0x60 // '`'
        case 0x36: return 0x2C // ','
        case 0x37: return 0x2E // '.'
        case 0x38: return 0x2F // '/'
        case 0x39: return QtKey.capsLock
        case 0x3A...0x45: return QtKey.f1 + Int(usage - 0x3A) // F1-F12
        case 0x46: return QtKey.print // PrintScreen
        case 0x47: return QtKey.scrollLock
        case 0x48: return QtKey.pause
        case 0x49: return QtKey.insert
        case 0x4A: return QtKey.home
        case 0x4B: return QtKey.pageUp
        case 0x4C: return QtKey.delete
        case 0x4D: return QtKey.end
        case 0x4E: return QtKey.pageDown
        case 0x4F: return QtKey.right
        case 0x50: return QtKey.left
        case 0x51: return QtKey.down
        case 0x52: return QtKey.up
        case 0x53: return QtKey.numLock
        case 0x64: return QtKey.menu // Application
        case 0x67: return QtKey.enter // Keypad Enter
        case 0x68...0x73: return QtKey.f1 + Int(usage - 0x68 + 12) // F13-F24
        case 0xE0, 0xE4: return QtKey.control // Left/Right Control
        case 0xE1, 0xE5: return QtKey.shift // Left/Right Shift
        case 0xE2, 0xE6: return QtKey.alt // Left/Right Alt
        case 0xE3, 0xE7: return QtKey.meta // Left/Right GUI
        default: return 0
        }
    }
}
