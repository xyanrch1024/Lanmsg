import SwiftUI
import QLanMsgCore

struct ChatView: View {
    @EnvironmentObject private var model: AppModel
    @Environment(\.dismiss) private var dismiss

    let peer: Peer
    @State private var draft = ""

    private var log: [ChatMessage] { model.messages[peer.ip] ?? [] }

    var body: some View {
        NavigationView {
            VStack(spacing: 0) {
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(spacing: 8) {
                            ForEach(log) { msg in
                                BubbleRow(msg: msg)
                                    .id(msg.id)
                            }
                        }
                        .padding(.horizontal, 12)
                        .padding(.vertical, 8)
                    }
                    .onChange(of: log.count) { _ in
                        if let last = log.last {
                            withAnimation { proxy.scrollTo(last.id, anchor: .bottom) }
                        }
                    }
                }

                Divider()
                HStack(spacing: 10) {
                    TextField("消息", text: $draft)
                        .textFieldStyle(.roundedBorder)
                        .submitLabel(.send)
                        .onSubmit(send)
                    Button(action: send) {
                        Image(systemName: "paperplane.fill")
                    }
                    .disabled(draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
                .padding(10)
            }
            .navigationTitle(peer.name)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button {
                        dismiss()
                    } label: {
                        Text("完成")
                    }
                }
            }
        }
        .navigationViewStyle(.stack)
    }

    private func send() {
        let text = draft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        model.sendChat(text, to: peer)
        draft = ""
    }
}

private struct BubbleRow: View {
    let msg: ChatMessage

    var body: some View {
        HStack {
            if msg.isMe { Spacer(minLength: 60) }
            VStack(alignment: .leading, spacing: 2) {
                if !msg.isMe {
                    Text(msg.who)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
                Text(msg.text)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 8)
                    .background(msg.isMe ? Color.accentColor : Color(.systemGray5))
                    .foregroundStyle(msg.isMe ? .white : .primary)
                    .clipShape(RoundedRectangle(cornerRadius: 16))
                    .textSelection(.enabled)
            }
            if !msg.isMe { Spacer(minLength: 60) }
        }
    }
}
