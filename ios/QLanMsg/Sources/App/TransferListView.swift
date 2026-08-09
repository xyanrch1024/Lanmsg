import SwiftUI

struct TransferListView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        NavigationView {
            Group {
                if model.transfers.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "arrow.up.arrow.down")
                            .font(.system(size: 44))
                            .foregroundStyle(.secondary)
                        Text("暂无传输记录")
                            .font(.headline)
                        Text("在设备列表中选择「发送文件」开始传输。")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                } else {
                    List {
                        ForEach(model.transfers) { record in
                            TransferRow(record: record)
                                .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                                    if !record.finished {
                                        Button {
                                            model.cancelTransfer(record)
                                        } label: {
                                            Label("取消", systemImage: "xmark")
                                        }
                                        .tint(.red)
                                    }
                                }
                        }
                    }
                }
            }
            .navigationTitle("传输")
        }
        .navigationViewStyle(.stack)
    }
}

private struct TransferRow: View {
    @ObservedObject var record: TransferRecord

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Image(systemName: record.direction == .send ? "arrow.up.circle" : "arrow.down.circle")
                    .foregroundStyle(record.ok ? .green : .secondary)
                VStack(alignment: .leading, spacing: 1) {
                    Text(record.fileName)
                        .font(.headline)
                        .lineLimit(1)
                    Text("\(record.direction.arrow) \(record.peerName) · \(Self.sizeText(record.done)) / \(Self.sizeText(record.total))")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if record.finished {
                    HStack(spacing: 8) {
                        if let path = record.filePath {
                            ShareButton(url: URL(fileURLWithPath: path))
                        }
                        Image(systemName: record.ok ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundStyle(record.ok ? .green : .red)
                    }
                }
            }

            if !record.finished {
                ProgressView(value: record.fraction)
                    .tint(record.direction == .send ? .accentColor : .green)
            } else {
                Text(record.status)
                    .font(.caption)
                    .foregroundStyle(record.ok ? .secondary : .red)
            }
        }
        .padding(.vertical, 4)
    }

    private static func sizeText(_ size: Int64) -> String {
        if size >= 1024 * 1024 {
            return String(format: "%.1f MB", Double(size) / 1024.0 / 1024.0)
        }
        if size >= 1024 {
            return String(format: "%.0f KB", Double(size) / 1024.0)
        }
        return "\(size) B"
    }
}

/// Minimal UIActivityViewController wrapper for sharing a received file.
private struct ShareButton: UIViewRepresentable {
    let url: URL

    func makeUIView(context: Context) -> UIButton {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: "square.and.arrow.up"), for: .normal)
        button.tintColor = .systemBlue
        button.addAction(UIAction { _ in present() }, for: .touchUpInside)
        return button
    }

    func updateUIView(_ uiView: UIButton, context: Context) {}

    private func present() {
        guard let root = UIApplication.shared.connectedScenes
            .compactMap({ ($0 as? UIWindowScene)?.keyWindow?.rootViewController })
            .first else { return }
        root.present(UIActivityViewController(activityItems: [url], applicationActivities: nil),
                     animated: true)
    }
}
