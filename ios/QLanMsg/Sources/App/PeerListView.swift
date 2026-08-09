import SwiftUI
import UniformTypeIdentifiers

struct PeerListView: View {
    @EnvironmentObject private var model: AppModel
    @State private var selectedPeer: Peer?
    @State private var fileTarget: Peer?
    @State private var showImporter = false

    var body: some View {
        NavigationView {
            Group {
                if model.peers.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "wifi.slash")
                            .font(.system(size: 44))
                            .foregroundStyle(.secondary)
                        Text("未发现设备")
                            .font(.headline)
                        Text("请确保与电脑处于同一 Wi-Fi,并保持本应用在前台。")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 40)
                    }
                } else {
                    List(model.peers) { peer in
                        PeerRow(peer: peer)
                            .contentShape(Rectangle())
                            .onTapGesture { selectedPeer = peer }
                            .contextMenu {
                                Button {
                                    selectedPeer = peer
                                } label: {
                                    Label("打开聊天", systemImage: "message")
                                }
                                Button {
                                    fileTarget = peer
                                    showImporter = true
                                } label: {
                                    Label("发送文件", systemImage: "paperplane")
                                }
                                Button {
                                    model.requestRemoteControl(of: peer)
                                } label: {
                                    Label("远程控制", systemImage: "cursorarrow.rays")
                                }
                            }
                            .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                                Button {
                                    selectedPeer = peer
                                } label: {
                                    Label("聊天", systemImage: "message")
                                }
                                .tint(.blue)
                            }
                    }
                }
            }
            .navigationTitle("设备")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Text("本机: \(model.settings.nickname)")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .navigationViewStyle(.stack)
        .sheet(item: $selectedPeer) { peer in
            ChatView(peer: peer)
        }
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [.item],
                      allowsMultipleSelection: false) { result in
            if let peer = fileTarget, case .success(let urls) = result, let url = urls.first {
                model.sendFile(url: url, to: peer)
            }
            fileTarget = nil
        }
    }
}

private struct PeerRow: View {
    let peer: Peer

    private var osIcon: String {
        if peer.os.lowercased().contains("windows") { return "pc" }
        if peer.os.lowercased().contains("ios") { return "iphone" }
        if peer.os.lowercased().contains("linux") { return "desktopcomputer" }
        return "desktopcomputer"
    }

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: osIcon)
                .font(.title3)
                .foregroundStyle(.tint)
            VStack(alignment: .leading, spacing: 2) {
                Text(peer.name)
                    .font(.headline)
                Text("\(peer.ip) · \(peer.os.isEmpty ? peer.host : peer.os)")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            Image(systemName: "chevron.right")
                .font(.footnote)
                .foregroundStyle(.tertiary)
        }
        .padding(.vertical, 4)
    }
}
