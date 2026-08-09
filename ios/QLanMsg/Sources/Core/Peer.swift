import Foundation

/// A discovered device. Mirrors `discovery/Peer.h` on the desktop.
public struct Peer: Identifiable, Equatable {
    public let id: String
    public var name: String
    public var host: String
    public var os: String
    public var ver: String
    public var ip: String
    public var tcpPort: UInt16
    public var lastSeen: Date

    public init(id: String, name: String, host: String, os: String, ver: String,
                ip: String, tcpPort: UInt16, lastSeen: Date = Date()) {
        self.id = id
        self.name = name
        self.host = host
        self.os = os
        self.ver = ver
        self.ip = ip
        self.tcpPort = tcpPort
        self.lastSeen = lastSeen
    }
}
