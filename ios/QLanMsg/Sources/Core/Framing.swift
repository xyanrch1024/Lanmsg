import Foundation

/// Frame building / parsing, wire-compatible with the desktop `makeFrame` /
/// `consumeFrames` in `src/common/Protocol.h`.
///
/// Frame layout (all integers little-endian):
///   [total u32][msgType u32][jsonLen u32][json bytes][body bytes]
public enum Framing {

    public static func makeFrame(_ type: MsgType, json: [String: Any] = [:], body: Data = Data()) -> Data {
        let payload: Data
        if json.isEmpty {
            payload = Data()
        } else {
            payload = (try? JSONSerialization.data(withJSONObject: json, options: [])) ?? Data()
        }
        let total = UInt32(ProtocolSpec.frameHeaderSize + payload.count + body.count)
        var data = Data(capacity: Int(total))
        appendUInt32LE(total, to: &data)
        appendUInt32LE(type.rawValue, to: &data)
        appendUInt32LE(UInt32(payload.count), to: &data)
        data.append(payload)
        data.append(body)
        return data
    }

    /// Consumes as many complete frames as possible from `buffer`.
    /// Returns false if malformed data was dropped (and the buffer is cleared).
    @discardableResult
    public static func consumeFrames(from buffer: inout [UInt8],
                                     _ handler: (MsgType, [String: Any], Data) -> Void) -> Bool {
        var ok = true
        while buffer.count >= ProtocolSpec.frameHeaderSize {
            let total = Int(readUInt32LE(buffer, 0))
            let typeRaw = readUInt32LE(buffer, 4)
            let jsonLen = Int(readUInt32LE(buffer, 8))
            if total < ProtocolSpec.frameHeaderSize {
                buffer.removeAll()
                ok = false
                break
            }
            if buffer.count < total {
                break // wait for more data
            }
            let frame = Array(buffer[0..<total])
            buffer.removeFirst(total)
            guard let type = MsgType(rawValue: typeRaw) else { continue } // unknown, skip
            let jsonEnd = ProtocolSpec.frameHeaderSize + jsonLen
            var json: [String: Any] = [:]
            if jsonLen > 0 {
                let jsonData = Data(frame[ProtocolSpec.frameHeaderSize..<jsonEnd])
                if let obj = try? JSONSerialization.jsonObject(with: jsonData), let dict = obj as? [String: Any] {
                    json = dict
                }
            }
            let body = Data(frame[jsonEnd..<frame.count])
            handler(type, json, body)
        }
        return ok
    }

    // MARK: - helpers

    private static func appendUInt32LE(_ value: UInt32, to data: inout Data) {
        var v = value.littleEndian
        withUnsafeBytes(of: &v) { data.append(contentsOf: $0) }
    }

    private static func readUInt32LE(_ bytes: [UInt8], _ offset: Int) -> UInt32 {
        UInt32(bytes[offset]) |
        (UInt32(bytes[offset + 1]) << 8) |
        (UInt32(bytes[offset + 2]) << 16) |
        (UInt32(bytes[offset + 3]) << 24)
    }
}
