import SwiftUI

struct SettingsView: View {
    @EnvironmentObject private var model: AppModel
    @State private var nickname = ""
    @State private var password = ""
    @State private var didLoad = false

    private var localIP: String { NetInfo.localIPv4 ?? "未知" }

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("身份")) {
                    TextField("昵称", text: $nickname)
                        .autocorrectionDisabled()
                    Text("此昵称会广播给局域网内其他设备。")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                Section(header: Text("远程控制")) {
                    SecureField("远程控制密码", text: $password)
                        .autocorrectionDisabled()
                    Text("留空表示对方请求控制你电脑时无需密码。此密码仅用于「iPhone 控制电脑」方向。")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                Section(header: Text("本机信息")) {
                    row("昵称", model.settings.nickname)
                    row("设备标识", model.settings.appId)
                    row("本机 IP", localIP)
                    row("TCP 端口", "\(model.settings.listenPort)")
                    row("App 版本", ProtocolSpec.appVersion)
                }
            }
            .navigationTitle("设置")
            .onAppear {
                guard !didLoad else { return }
                didLoad = true
                nickname = model.settings.nickname
                password = model.settings.remotePassword
            }
            .onDisappear(perform: save)
        }
        .navigationViewStyle(.stack)
    }

    private func save() {
        model.settings.nickname = nickname.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            ? model.settings.nickname
            : nickname.trimmingCharacters(in: .whitespacesAndNewlines)
        model.settings.remotePassword = password
    }

    private func row(_ title: String, _ value: String) -> some View {
        HStack {
            Text(title)
            Spacer()
            Text(value)
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
        }
    }
}
