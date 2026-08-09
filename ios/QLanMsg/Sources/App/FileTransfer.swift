import Foundation

// MARK: - UI-facing models

enum TransferDirection: String {
    case send
    case receive

    var arrow: String { self == .send ? "↑" : "↓" }
}

/// A row in the transfer list. Mutated on the main thread by AppModel.
final class TransferRecord: Identifiable, ObservableObject {
    let token: String
    let direction: TransferDirection
    let peerName: String
    let fileName: String
    let total: Int64

    @Published var done: Int64 = 0
    @Published var status: String
    @Published var finished = false
    @Published var ok = false
    @Published var filePath: String?

    init(token: String, direction: TransferDirection, peerName: String, fileName: String,
         total: Int64, status: String) {
        self.token = token
        self.direction = direction
        self.peerName = peerName
        self.fileName = fileName
        self.total = total
        self.status = status
    }

    func progress(sent: Int64, total: Int64) {
        done = sent
    }

    func finish(ok: Bool, info: String) {
        finished = true
        self.ok = ok
        status = ok ? "完成" : info
        done = total
    }

    var fraction: Double { total > 0 ? Double(done) / Double(total) : 0 }
}

/// An inbound file offer awaiting user consent.
struct IncomingFileOffer: Identifiable {
    let id = UUID()
    let peerIp: String
    let peerName: String
    let token: String
    let name: String
    let size: Int64
}

// MARK: - Sender

/// Streams a local file to a peer in 64 KB chunks, applying backpressure via
/// `Transport.sendChunk`'s completion callback (the iOS analogue of the
/// desktop `FileSender`).
final class FileSender {
    let ip: String
    let token: String

    var onProgress: ((Int64, Int64) -> Void)?
    var onFinish: ((Bool, String) -> Void)?

    private let url: URL
    private let transport: Transport
    private var handle: FileHandle?
    private var total: Int64 = 0
    private var sent: Int64 = 0
    private var seq: Int64 = 0
    private var active = false
    private var doneSent = false
    private var scoped = false
    private var inFlight = 0
    private let maxInFlight = 4

    init(ip: String, token: String, url: URL, transport: Transport) {
        self.ip = ip
        self.token = token
        self.url = url
        self.transport = transport
    }

    /// Called once the TCP session to the peer is ready.
    func begin(peerName: String) {
        guard !active else { return }
        scoped = url.startAccessingSecurityScopedResource()
        do {
            let values = try url.resourceValues(forKeys: [.fileSizeKey])
            total = Int64(values.fileSize ?? 0)
            let fh = try FileHandle(forReadingFrom: url)
            handle = fh
        } catch {
            finish(false, "无法打开文件: \(error.localizedDescription)")
            return
        }
        active = true
        transport.send(ip, .fileOffer,
                       json: ["token": token, "name": url.lastPathComponent, "size": total])
    }

    func onAccepted() {
        pump()
    }

    func onDeclined(reason: String) {
        finish(false, reason)
    }

    func onCanceled(reason: String) {
        finish(false, reason)
    }

    /// Reads and sends up to `maxInFlight` 64 KB chunks at a time, re-pumping as
    /// each chunk is accepted by the socket (completion-based backpressure).
    private func pump() {
        guard active, let fh = handle else { return }
        while inFlight < maxInFlight {
            let data: Data
            do {
                data = try fh.read(upToCount: ProtocolSpec.fileChunkSize) ?? Data()
            } catch {
                finish(false, "读取失败: \(error.localizedDescription)")
                return
            }
            if data.isEmpty {
                guard !doneSent else { return }
                doneSent = true
                transport.send(ip, .fileDone, json: ["token": token])
                finish(true, "发送完成")
                return
            }
            inFlight += 1
            sent += Int64(data.count)
            let frameSeq = seq
            seq += 1
            onProgress?(sent, total)
            transport.sendChunk(ip, .fileChunk, json: ["token": token, "seq": frameSeq], body: data) {
                [weak self] in
                self?.inFlight -= 1
                self?.pump()
            }
        }
    }

    func cancel() {
        guard active else { return }
        transport.send(ip, .fileCancel, json: ["token": token, "reason": "发送方取消"])
        finish(false, "已取消")
    }

    private func finish(_ ok: Bool, _ info: String) {
        active = false
        handle?.closeFile()
        handle = nil
        if scoped {
            url.stopAccessingSecurityScopedResource()
            scoped = false
        }
        onFinish?(ok, info)
    }
}

// MARK: - Receiver

/// Writes inbound file chunks to the app's Documents/Downloads folder at the
/// correct 64 KB-aligned offsets (the iOS analogue of the desktop
/// `FileReceiver`).
final class FileReceiver {
    let ip: String
    let token: String
    let name: String
    let total: Int64

    var onProgress: ((Int64) -> Void)?
    var onFinish: ((Bool, String) -> Void)?

    private let transport: Transport
    private var handle: FileHandle?
    private var received: Int64 = 0
    private var active = true
    private var destURL: URL?

    init(ip: String, token: String, name: String, size: Int64, transport: Transport) {
        self.ip = ip
        self.token = token
        self.name = name
        self.total = size
        self.transport = transport
    }

    var destinationPath: String { destURL?.path ?? "" }

    /// Called right after the user accepted the offer; must run before chunks
    /// arrive so the file handle is ready.
    func start() {
        guard active else { return }
        do {
            let url = try createFile()
            destURL = url
            handle = try FileHandle(forWritingTo: url)
        } catch {
            finish(false, "无法创建文件: \(error.localizedDescription)")
        }
    }

    private func createFile() throws -> URL {
        let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Downloads", isDirectory: true)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        var url = dir.appendingPathComponent(name)
        if FileManager.default.fileExists(atPath: url.path) {
            let base = (name as NSString).deletingPathExtension
            let ext = (name as NSString).pathExtension
            let stamp = Int64(Date().timeIntervalSince1970 * 1000)
            let newName = ext.isEmpty ? "\(base)_\(stamp)" : "\(base)_\(stamp).\(ext)"
            url = dir.appendingPathComponent(newName)
        }
        FileManager.default.createFile(atPath: url.path, contents: nil)
        return url
    }

    func onChunk(seq: Int64, data: Data) {
        guard active, let fh = handle else { return }
        let pos = seq * Int64(ProtocolSpec.fileChunkSize)
        do {
            try fh.seek(toOffset: UInt64(pos))
            try fh.write(contentsOf: data)
        } catch {
            finish(false, "写入失败: \(error.localizedDescription)")
            return
        }
        received += Int64(data.count)
        onProgress?(received)
        if received >= total {
            transport.send(ip, .fileDone, json: ["token": token])
            finish(true, "接收完成")
        }
    }

    func onDone() {
        guard active else { return }
        transport.send(ip, .fileDone, json: ["token": token])
        finish(true, "接收完成")
    }

    func onCanceled(reason: String) {
        finish(false, reason)
    }

    func cancel() {
        guard active else { return }
        transport.send(ip, .fileCancel, json: ["token": token, "reason": "接收方取消"])
        finish(false, "已取消")
    }

    private func finish(_ ok: Bool, _ info: String) {
        guard active else { return }
        active = false
        handle?.closeFile()
        handle = nil
        if !ok, let destURL {
            try? FileManager.default.removeItem(at: destURL)
        }
        onFinish?(ok, info)
    }
}
