import Foundation

/// Wire-protocol constants mirroring the desktop app (`src/common/Protocol.h`).
/// Byte order is little-endian (both platforms are little-endian).
public enum ProtocolSpec {
    public static let udpPort: UInt16 = 24260
    public static let tcpPort: UInt16 = 24261
    public static let multicastGroup = "239.255.77.77"
    public static let discoveryInterval: TimeInterval = 3.0
    public static let peerTimeout: TimeInterval = 10.0
    public static let frameHeaderSize = 12 // totalLen(4) + msgType(4) + jsonLen(4)
    public static let fileChunkSize = 64 * 1024
    public static let remoteFrameMaxBuffered = 768 * 1024
    public static let defaultRemoteFps = 10
    public static let defaultJpegQuality = 65
    public static let magic = "QLMSG"
    public static let appVersion = "0.1.0"
    public static let broadcastAddress = "255.255.255.255"
}

/// Frame message types. Keep the numeric values identical to the desktop.
public enum MsgType: UInt32 {
    case hello = 1
    case chat = 2
    case fileOffer = 3
    case fileAccept = 4
    case fileDecline = 5
    case fileChunk = 6
    case fileDone = 7
    case fileCancel = 8
    case rcRequest = 9
    case rcAccept = 10
    case rcDecline = 11
    case rcFrame = 12
    case rcInput = 13
    case rcStop = 14
    case rcPing = 15
    case rcPong = 16
}

public struct ChatMessage: Identifiable, Equatable {
    public let id = UUID()
    public let who: String
    public let text: String
    public let ts: Double
    public let isMe: Bool

    public init(who: String, text: String, ts: Double, isMe: Bool) {
        self.who = who
        self.text = text
        self.ts = ts
        self.isMe = isMe
    }
}
