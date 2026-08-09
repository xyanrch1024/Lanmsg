#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <cstring>

namespace qlm {

constexpr quint16 kUdpPort = 24260;
constexpr quint16 kTcpPort = 24261;
constexpr int kDiscoveryIntervalMs = 3000;
constexpr int kPeerTimeoutMs = 10000;
constexpr quint32 kFrameHeaderSize = 12; // totalLen(4) + msgType(4) + jsonLen(4)
constexpr qint64 kFileChunkSize = 64 * 1024;
constexpr qint64 kRemoteFrameMaxBuffered = 768 * 1024; // skip frames when TCP buffer above this
constexpr int kDefaultRemoteFps = 10;
constexpr int kDefaultJpegQuality = 65;

constexpr const char *kMagic = "QLMSG";
constexpr const char *kAppVersion = "0.1.0";

enum class MsgType : quint32 {
    Hello = 1,
    Chat = 2,
    FileOffer = 3,
    FileAccept = 4,
    FileDecline = 5,
    FileChunk = 6,
    FileDone = 7,
    FileCancel = 8,
    RcRequest = 9,
    RcAccept = 10,
    RcDecline = 11,
    RcFrame = 12,
    RcInput = 13,
    RcStop = 14,
    RcPing = 15,
    RcPong = 16,
};

struct InputEvent {
    enum Type {
        MouseMove,
        MouseDown,
        MouseUp,
        Wheel,
        KeyDown,
        KeyUp,
    };
    Type type = MouseMove;
    int x = 0;     // absolute screen coords
    int y = 0;
    int button = 0; // 1=left 2=middle 3=right
    int dx = 0;     // wheel horizontal
    int dy = 0;     // wheel vertical
    int key = 0;    // Qt::Key
};

struct FileOffer {
    QString token;
    QString name;
    qint64 size = 0;
};

inline QByteArray makeFrame(MsgType type, const QJsonObject &json = {}, const QByteArray &body = {}) {
    QByteArray payload = QJsonDocument(json).toJson(QJsonDocument::Compact);
    const quint32 total = static_cast<quint32>(kFrameHeaderSize + payload.size() + body.size());
    const quint32 typeRaw = static_cast<quint32>(type);
    const quint32 jsonLen = static_cast<quint32>(payload.size());
    QByteArray frame;
    frame.reserve(static_cast<int>(total));
    frame.append(reinterpret_cast<const char *>(&total), 4);
    frame.append(reinterpret_cast<const char *>(&typeRaw), 4);
    frame.append(reinterpret_cast<const char *>(&jsonLen), 4);
    frame.append(payload);
    frame.append(body);
    return frame;
}

// Consumes as many complete frames as possible from `buffer`.
// Emits one callback per complete frame. Returns false if malformed data was dropped.
template <typename Fn>
bool consumeFrames(QByteArray &buffer, Fn &&cb) {
    bool ok = true;
    while (buffer.size() >= static_cast<int>(kFrameHeaderSize)) {
        quint32 total = 0;
        quint32 typeRaw = 0;
        quint32 jsonLen = 0;
        memcpy(&total, buffer.constData(), 4);
        memcpy(&typeRaw, buffer.constData() + 4, 4);
        memcpy(&jsonLen, buffer.constData() + 8, 4);
        if (total < kFrameHeaderSize) {
            buffer.clear();
            ok = false;
            break;
        }
        if (static_cast<quint32>(buffer.size()) < total)
            break; // wait for more data
        QByteArray frame = buffer.left(static_cast<int>(total));
        buffer.remove(0, static_cast<int>(total));
        QJsonObject json;
        if (jsonLen > 0) {
            QJsonParseError err;
            json = QJsonDocument::fromJson(frame.mid(kFrameHeaderSize, static_cast<int>(jsonLen)), &err).object();
        }
        QByteArray body = frame.mid(kFrameHeaderSize + jsonLen);
        cb(static_cast<MsgType>(typeRaw), json, body);
    }
    return ok;
}

inline QJsonObject inputToJson(const InputEvent &ev) {
    QJsonObject o;
    o["t"] = static_cast<int>(ev.type);
    o["x"] = ev.x;
    o["y"] = ev.y;
    o["b"] = ev.button;
    o["dx"] = ev.dx;
    o["dy"] = ev.dy;
    o["k"] = ev.key;
    return o;
}

inline InputEvent jsonToInput(const QJsonObject &o) {
    InputEvent ev;
    ev.type = static_cast<InputEvent::Type>(o["t"].toInt(0));
    ev.x = o["x"].toInt(0);
    ev.y = o["y"].toInt(0);
    ev.button = o["b"].toInt(0);
    ev.dx = o["dx"].toInt(0);
    ev.dy = o["dy"].toInt(0);
    ev.key = o["k"].toInt(0);
    return ev;
}

} // namespace qlm
