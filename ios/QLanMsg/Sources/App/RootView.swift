import SwiftUI

/// Top-level tab container plus the global presentation layer: the file-offer
/// alert and the full-screen remote-control cover.
struct RootView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        TabView {
            PeerListView()
                .tabItem { Label("设备", systemImage: "list.bullet") }
            TransferListView()
                .tabItem { Label("传输", systemImage: "arrow.up.arrow.down") }
            SettingsView()
                .tabItem { Label("设置", systemImage: "gearshape") }
        }
        .alert(item: $model.pendingFileOffer) { offer in
            Alert(
                title: Text("接收文件"),
                message: Text("\(offer.peerName) 想向你发送文件:\n\n\(offer.name)\n大小: \(Self.sizeText(offer.size))\n\n是否接收?"),
                primaryButton: .default(Text("接收")) {
                    model.respond(to: offer, accept: true)
                },
                secondaryButton: .cancel(Text("拒绝")) {
                    model.respond(to: offer, accept: false)
                }
            )
        }
        .overlay(alignment: .top) {
            if let notice = model.notice {
                Text(notice)
                    .font(.footnote)
                    .padding(.horizontal, 14)
                    .padding(.vertical, 8)
                    .background(.thinMaterial, in: Capsule())
                    .padding(.top, 6)
                    .transition(.move(edge: .top).combined(with: .opacity))
            }
        }
        .animation(.easeInOut(duration: 0.2), value: model.notice)
        .fullScreenCover(item: $model.remoteSession) { controller in
            RemoteControlView(controller: controller)
        }
    }

    private static func sizeText(_ size: Int64) -> String {
        if size >= 1024 * 1024 {
            return String(format: "%.1f MB", Double(size) / 1024.0 / 1024.0)
        }
        return String(format: "%.1f KB", Double(size) / 1024.0)
    }
}
