#include "net/NetworkService.h"

#include "common/Config.h"
#include "net/PeerSession.h"

#include <QDateTime>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>

NetworkService::NetworkService(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this)) {
    m_initIp = localIpv4();

    if (!m_server->listen(QHostAddress::Any, Config::instance().tcpPort()))
        qWarning() << "TCP listen failed:" << m_server->errorString();
    connect(m_server, &QTcpServer::newConnection, this, &NetworkService::onNewConnection);
}

NetworkService::~NetworkService() {
    const auto keys = m_sessions.keys();
    for (const QString &k : keys)
        if (m_sessions.value(k))
            m_sessions.value(k)->setDropped();
}

QString NetworkService::localIpv4() const {
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry &e : entries) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && !e.ip().isLoopback()) {
                const QString ip = e.ip().toString();
                if (!ip.startsWith("169.254."))
                    return ip;
            }
        }
    }
    return QStringLiteral("127.0.0.1");
}

PeerSession *NetworkService::sessionFor(const QString &ip) const {
    PeerSession *s = m_sessions.value(ip);
    if (s && s->socket() && s->socket()->state() == QAbstractSocket::ConnectedState)
        return s;
    return nullptr;
}

bool NetworkService::isReady(const QString &ip) const {
    PeerSession *s = m_sessions.value(ip);
    return s && s->ready();
}

QList<PeerSession *> NetworkService::sessions() const {
    return m_sessions.values();
}

void NetworkService::ensureSession(const Peer &peer) {
    if (sessionAny(peer.ip))
        return; // reuse any existing session (connecting or connected)
    QTcpSocket *sock = new QTcpSocket;
    PeerSession *s = new PeerSession(sock, PeerSession::Outgoing, this);
    s->setConnSeq(++m_outSeq);
    s->setInitIp(m_initIp);
    s->setKeyIp(peer.ip);
    registerSession(s);
    sock->connectToHost(peer.ip, peer.tcpPort);
}

PeerSession *NetworkService::registerSession(PeerSession *s) {
    const QString ip = s->ip();
    if (ip.isEmpty())
        return s;
    m_sessions.insert(ip, s);
    connect(s, &PeerSession::frame, this,
            [this, s](MsgType t, const QJsonObject &j, const QByteArray &b) {
                onSessionFrame(s, t, j, b);
            });
    connect(s, &PeerSession::helloReceived, this,
            [this, s](const QString &n, quint64 seq, const QString &initIp, const QString &initAppId) {
                onSessionHello(s, n, seq, initIp, initAppId);
            });
    connect(s, &PeerSession::closed, this,
            [this, s](PeerSession *) { onSessionClosed(s); });
    connect(s, &PeerSession::readyChanged, this, [this, s](bool ready) {
        // Outgoing sessions never receive a Hello, so announce readiness here.
        if (ready && s->role() == PeerSession::Outgoing && m_sessions.value(s->ip()) == s)
            emit sessionReady(s->ip(), s->peerName());
    });
    return s;
}

void NetworkService::unregister(PeerSession *s) {
    const QString ip = s->ip();
    if (m_sessions.value(ip) == s)
        m_sessions.remove(ip);
}

void NetworkService::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        PeerSession *s = new PeerSession(sock, PeerSession::Incoming, this);
        registerSession(s);
        if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
            qInfo() << "[net] accepted connection from" << s->ip();
    }
}

void NetworkService::onSessionHello(PeerSession *s, const QString &peerName, quint64 seq, const QString &initIp,
                                    const QString &initAppId) {
    dedupe(s, initIp.isEmpty() ? s->ip() : initIp, seq, initAppId);
    if (m_sessions.value(s->ip()) != s)
        return;
    s->setPeerName(peerName);
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[net] session ready with" << peerName << s->ip();
    emit sessionReady(s->ip(), peerName);
}

void NetworkService::dedupe(PeerSession *s, const QString &initIp, quint64 seq, const QString &initAppId) {
    const QString ip = s->ip();
    PeerSession *other = m_sessions.value(ip);
    PeerSession *winner = s;

    if (other && other != s) {
        // keep the session with the smaller (initIp, seq, initAppId) key
        const QString thisKey = initIp + QLatin1Char('|') + QString::number(seq) + QLatin1Char('|') + initAppId;
        const QString otherKey = other->initIp() + QLatin1Char('|') + QString::number(other->connSeq()) +
                                 QLatin1Char('|') + other->initAppId();
        if (thisKey < otherKey)
            winner = s;
        else
            winner = other;
    }

    PeerSession *loser = (winner == s) ? other : s;
    if (loser && loser != winner) {
        if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
            qInfo() << "[net] dedupe: drop" << loser->ip() << "keep" << winner->ip()
                    << "role=" << (winner->role() == PeerSession::Outgoing ? "out" : "in");
        loser->setDropped();
        if (loser->socket())
            loser->socket()->abort();
        loser->deleteLater();
    }
    // always ensure the surviving session is the registered one (multiple sessions
    // to the same ip can overwrite the map before dedupe runs)
    m_sessions.insert(ip, winner);
}

void NetworkService::onSessionClosed(PeerSession *s) {
    const QString ip = s->ip();
    unregister(s);
    emit sessionClosed(ip);
    s->deleteLater();
}

void NetworkService::onSessionFrame(PeerSession *s, MsgType type, const QJsonObject &json, const QByteArray &body) {
    const QString ip = s->ip();
    switch (type) {
    case MsgType::Chat:
        if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
            qInfo() << "[net] chat frame from" << ip;
        emit chatReceived(ip, json["text"].toString(), static_cast<qint64>(json["ts"].toDouble()));
        break;
    case MsgType::FileOffer: {
        FileOffer o;
        o.token = json["token"].toString();
        o.name = json["name"].toString();
        o.size = static_cast<qint64>(json["size"].toDouble());
        emit fileOfferReceived(ip, o);
        break;
    }
    case MsgType::FileAccept:
        emit fileAcceptReceived(ip, json["token"].toString());
        break;
    case MsgType::FileDecline:
        emit fileDeclineReceived(ip, json["token"].toString(), json["reason"].toString());
        break;
    case MsgType::FileDone:
        emit fileDoneReceived(ip, json["token"].toString());
        break;
    case MsgType::FileCancel:
        emit fileCancelReceived(ip, json["token"].toString(), json["reason"].toString());
        break;
    case MsgType::FileChunk:
        emit fileChunkReceived(ip, json["token"].toString(), static_cast<qint64>(json["seq"].toDouble()), body);
        break;
    case MsgType::RcRequest:
        emit rcRequestReceived(ip, json["token"].toString(), json["password"].toString().size() > 0);
        break;
    case MsgType::RcAccept:
        emit rcAcceptReceived(ip, json["token"].toString());
        break;
    case MsgType::RcDecline:
        emit rcDeclineReceived(ip, json["token"].toString(), json["reason"].toString());
        break;
    case MsgType::RcFrame:
        emit rcFrameReceived(ip, json["token"].toString(), body, json["w"].toInt(), json["h"].toInt(),
                             static_cast<qint64>(json["ts"].toDouble()));
        break;
    case MsgType::RcInput:
        emit rcInputReceived(ip, json["token"].toString(), qlm::jsonToInput(json));
        break;
    case MsgType::RcStop:
        emit rcStopReceived(ip, json["token"].toString());
        break;
    default:
        break;
    }
}

bool NetworkService::sendChat(const QString &ip, const QString &text, qint64 ts) {
    PeerSession *s = sessionAny(ip);
    if (!s)
        return false;
    QJsonObject j;
    j["text"] = text;
    j["ts"] = static_cast<double>(ts);
    s->sendMessage(MsgType::Chat, j);
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[net] send chat to" << ip << "ready=" << s->ready() << "role="
                << (s->role() == PeerSession::Outgoing ? "out" : "in");
    return true;
}

bool NetworkService::sendFileOffer(const QString &ip, const QString &token, const QString &name, qint64 size) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j;
    j["token"] = token;
    j["name"] = name;
    j["size"] = static_cast<double>(size);
    s->sendMessage(MsgType::FileOffer, j);
    return true;
}

bool NetworkService::sendFileAccept(const QString &ip, const QString &token) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}};
    s->sendMessage(MsgType::FileAccept, j);
    return true;
}

bool NetworkService::sendFileDecline(const QString &ip, const QString &token, const QString &reason) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}, {"reason", reason}};
    s->sendMessage(MsgType::FileDecline, j);
    return true;
}

bool NetworkService::sendFileDone(const QString &ip, const QString &token) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}};
    s->sendMessage(MsgType::FileDone, j);
    return true;
}

bool NetworkService::sendFileCancel(const QString &ip, const QString &token, const QString &reason) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}, {"reason", reason}};
    s->sendMessage(MsgType::FileCancel, j);
    return true;
}

bool NetworkService::sendFileChunk(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}, {"seq", static_cast<double>(seq)}};
    s->sendMessage(MsgType::FileChunk, j, chunk);
    return true;
}

bool NetworkService::requestRemote(const QString &ip, const QString &token, const QString &password) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}, {"password", password}};
    s->sendMessage(MsgType::RcRequest, j);
    return true;
}

bool NetworkService::acceptRemote(const QString &ip, const QString &token) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}};
    s->sendMessage(MsgType::RcAccept, j);
    return true;
}

bool NetworkService::declineRemote(const QString &ip, const QString &token, const QString &reason) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}, {"reason", reason}};
    s->sendMessage(MsgType::RcDecline, j);
    return true;
}

bool NetworkService::sendRemoteFrame(const QString &ip, const QString &token, const QByteArray &jpeg, int w, int h, qint64 ts) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    // Adaptive: drop frame when the socket buffer is already full.
    if (s->socket()->bytesToWrite() > qlm::kRemoteFrameMaxBuffered)
        return false;
    QJsonObject j{{"token", token}, {"w", w}, {"h", h}, {"ts", static_cast<double>(ts)}};
    s->sendMessage(MsgType::RcFrame, j, jpeg);
    return true;
}

bool NetworkService::sendRemoteInput(const QString &ip, const QString &token, const InputEvent &ev) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j = qlm::inputToJson(ev);
    j["token"] = token;
    s->sendMessage(MsgType::RcInput, j);
    return true;
}

bool NetworkService::stopRemote(const QString &ip, const QString &token) {
    PeerSession *s = sessionFor(ip);
    if (!s)
        return false;
    QJsonObject j{{"token", token}};
    s->sendMessage(MsgType::RcStop, j);
    return true;
}
