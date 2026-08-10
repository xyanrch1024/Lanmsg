import SwiftUI

@main
struct QLanMsgApp: App {
    @StateObject private var model = AppModel.shared
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(model)
        }
        .onChange(of: scenePhase) { newPhase in
            switch newPhase {
            case .background, .inactive:
                model.sendGoodbye()
            case .active:
                model.sendAnnounce()
            @unknown default:
                break
            }
        }
    }
}
