import Foundation
import Combine

/// Persisted local settings (UserDefaults), mirroring `Config` on the desktop.
/// `appId` is generated once as the first 12 hex chars of a UUID.
public final class SettingsStore: ObservableObject {
    public static let shared = SettingsStore()

    public let appId: String

    @Published public var nickname: String {
        didSet { defaults.set(nickname, forKey: "nickname") }
    }

    @Published public var remotePassword: String {
        didSet { defaults.set(remotePassword, forKey: "remotePassword") }
    }

    @Published public var listenPort: UInt16 {
        didSet { defaults.set(Int(listenPort), forKey: "listenPort") }
    }

    private let defaults: UserDefaults

    public init(defaults: UserDefaults = .standard) {
        self.defaults = defaults

        if let id = defaults.string(forKey: "appId"), !id.isEmpty {
            appId = id
        } else {
            let generated = String(UUID().uuidString.replacingOccurrences(of: "-", with: "").prefix(12))
            defaults.set(generated, forKey: "appId")
            appId = generated
        }

        let storedPort = UInt16(defaults.integer(forKey: "listenPort"))
        listenPort = storedPort == 0 ? ProtocolSpec.tcpPort : storedPort
        nickname = defaults.string(forKey: "nickname") ?? "iPhone"
        remotePassword = defaults.string(forKey: "remotePassword") ?? ""
    }
}
