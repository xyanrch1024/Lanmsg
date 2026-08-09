import XCTest
@testable import QLanMsgCore

final class ProtocolTests: XCTestCase {

    func testFrameLayoutMatchesDesktop() {
        // Desktop: makeFrame(Chat, {"text":"hi","ts":123}, {}) emits a 12-byte
        // little-endian header + compact JSON.
        let frame = Framing.makeFrame(.chat, json: ["text": "hi", "ts": 123])
        XCTAssertGreaterThanOrEqual(frame.count, 12)

        // total
        XCTAssertEqual(UInt32(frame[0]) | (UInt32(frame[1]) << 8) |
                       (UInt32(frame[2]) << 16) | (UInt32(frame[3]) << 24),
                       UInt32(frame.count))
        // msgType == 2 (Chat)
        let type = UInt32(frame[4]) | (UInt32(frame[5]) << 8) |
                   (UInt32(frame[6]) << 16) | (UInt32(frame[7]) << 24)
        XCTAssertEqual(type, 2)
    }

    func testConsumeFramesHandlesStickyBuffer() {
        var buffer: [UInt8] = []
        let a = Framing.makeFrame(.chat, json: ["text": "one"])
        let b = Framing.makeFrame(.hello, json: ["id": "x"])
        buffer.append(contentsOf: a)
        buffer.append(contentsOf: Array(b.prefix(5))) // partial second frame

        var received: [(MsgType, [String: Any])] = []
        _ = Framing.consumeFrames(from: &buffer) { type, json, _ in
            received.append((type, json))
        }
        XCTAssertEqual(received.count, 1)
        XCTAssertEqual(received.first?.0, .chat)
        XCTAssertEqual(received.first?.1["text"] as? String, "one")

        // Feed the rest; now the hello must come through.
        buffer.append(contentsOf: Array(b.dropFirst(5)))
        _ = Framing.consumeFrames(from: &buffer) { type, json, _ in
            received.append((type, json))
        }
        XCTAssertEqual(received.count, 2)
        XCTAssertEqual(received[1].0, .hello)
        XCTAssertTrue(buffer.isEmpty)
    }

    func testConsumeFramesDropsMalformedFrame() {
        var buffer: [UInt8] = [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
        var seen = 0
        let ok = Framing.consumeFrames(from: &buffer) { _, _, _ in seen += 1 }
        XCTAssertFalse(ok) // total(1) < header(12) -> malformed
        XCTAssertTrue(buffer.isEmpty)
        XCTAssertEqual(seen, 0)
    }

    func testUnknownMsgTypeSkipped() {
        var bytes = [UInt8]()
        bytes.append(contentsOf: [0x0C, 0x00, 0x00, 0x00]) // total 12
        bytes.append(contentsOf: [0xFF, 0xFF, 0xFF, 0xFF]) // unknown type
        bytes.append(contentsOf: [0x00, 0x00, 0x00, 0x00]) // jsonLen 0
        var buffer = bytes
        var seen = 0
        let ok = Framing.consumeFrames(from: &buffer) { _, _, _ in seen += 1 }
        XCTAssertTrue(ok)
        XCTAssertEqual(seen, 0)
        XCTAssertTrue(buffer.isEmpty)
    }

    func testChatJSONFields() {
        let frame = Framing.makeFrame(.chat, json: ["text": "你好", "ts": 1234.5])
        var buffer = Array(frame)
        var result: [String: Any]?
        _ = Framing.consumeFrames(from: &buffer) { type, json, _ in
            if type == .chat { result = json }
        }
        XCTAssertEqual(result?["text"] as? String, "你好")
        XCTAssertEqual((result?["ts"] as? NSNumber)?.doubleValue, 1234.5)
    }

    func testInputEventJSONUsesExpectedKeys() {
        let ev = InputEvent(type: .mouseDown, x: 100, y: 200, button: 3)
        let json = ev.json
        XCTAssertEqual(json["t"] as? Int, 1) // mouseDown
        XCTAssertEqual(json["x"] as? Int, 100)
        XCTAssertEqual(json["y"] as? Int, 200)
        XCTAssertEqual(json["b"] as? Int, 3)
        XCTAssertEqual(json["k"] as? Int, 0)

        let round = InputEvent.fromJSON(json)
        XCTAssertEqual(round.type, .mouseDown)
        XCTAssertEqual(round.button, 3)
    }

    func testKeyMappingHIDToQt() {
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x04), 0x41)       // A
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x1E), 0x31)       // '1'
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x28), QtKey.return)
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x29), QtKey.escape)
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0xE0), QtKey.control)
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0xE1), QtKey.shift)
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x52), QtKey.up)
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x3A), QtKey.f1)   // F1
        XCTAssertEqual(KeyMapping.qtKey(forHID: 0x99), 0)          // unknown
    }

    func testKeyModsMapping() {
        XCTAssertEqual(KeyMods.qtKey(KeyMods.shift), QtKey.shift)
        XCTAssertEqual(KeyMods.qtKey(KeyMods.control), QtKey.control)
        XCTAssertEqual(KeyMods.qtKey(KeyMods.alt), QtKey.alt)
        XCTAssertEqual(KeyMods.qtKey(KeyMods.meta), QtKey.meta)
    }
}
