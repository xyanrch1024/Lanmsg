#include "net/PeerSession.h"

#include "common/Config.h"

#include <QTimer>

PeerSession::PeerSession(QTcpSocket *socket, Role role, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_role(role) {
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::connected, this, &PeerSession::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &PeerSession::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &PeerSession::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &PeerSession::onError);
    connect(m_socket, &QTcpSocket::bytesWritten, this,
            [this](qint64) { emit bytesWrittenToSocket(); });

    // Outgoing connections may hang forever if a firewall silently drops the SYN
    // (common across the WSL2 NAT boundary). Abort so the slot is freed and the
    // UI can reconnect instead of freezing with a never-ready session.
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(12000);
    connect(m_connectTimer, &QTimer::timeout, this, [this] {
        if (!m_ready) {
            if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                qInfo() << "[session] connect timeout, aborting" << ip();
            if (m_socket)
                m_socket->abort();
        }
    });
}

namespace {
QString normalizeIp(QString ip) {
    if (ip.startsWith(QLatin1String("::ffff:")))
        ip.remove(0, 7);
    return ip;
}
} // namespace

QString PeerSession::ip() const {
    if (!m_keyIp.isEmpty())
        return m_keyIp;
    if (m_socket)
        return normalizeIp(m_socket->peerAddress().toString());
    return QString();
}

void PeerSession::setReady(bool r) {
    if (m_ready == r)
        return;
    m_ready = r;
    if (m_ready) {
        if (m_connectTimer)
            m_connectTimer->stop();
        flushQueue();
    }
    emit readyChanged(m_ready);
}

void PeerSession::onConnected() {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[session] connected" << ip();
    if (m_connectTimer)
        m_connectTimer->start();
    // Outgoing: we are the initiator. Send Hello immediately.
    QJsonObject hello;
    hello["id"] = QString::fromUtf8(Config::instance().appId());
    hello["name"] = Config::instance().nickname();
    hello["seq"] = static_cast<qint64>(m_connSeq);
    hello["initIp"] = m_initIp;
    hello["appId"] = QString::fromUtf8(Config::instance().appId());
    sendMessage(MsgType::Hello, hello);
    setReady(true);
}

void PeerSession::sendMessage(MsgType type, const QJsonObject &json, const QByteArray &body) {
    QByteArray frame = qlm::makeFrame(type, json, body);
    if (!m_ready || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        m_queue.append(frame);
        return;
    }
    m_socket->write(frame);
}

void PeerSession::flushQueue() {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    for (const QByteArray &frame : std::as_const(m_queue))
        m_socket->write(frame);
    m_queue.clear();
}

void PeerSession::onReadyRead() {
    if (!m_socket)
        return;
    m_buffer.append(m_socket->readAll());
    qlm::consumeFrames(m_buffer, [this](MsgType type, const QJsonObject &json, const QByteArray &body) {
        if (type == MsgType::Hello) {
            m_peerName = json["name"].toString();
            m_connSeq = static_cast<quint64>(json["seq"].toDouble());
            m_initIp = json["initIp"].toString();
            m_initAppId = json["appId"].toString();
            setReady(true);
            emit helloReceived(m_peerName, m_connSeq, m_initIp, m_initAppId);
            return;
        }
        emit frame(type, json, body);
    });
}

void PeerSession::onDisconnected() {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[session] disconnected" << ip() << "ready=" << m_ready;
    emit closed(this);
}

void PeerSession::onError(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[session] error" << ip() << m_socket->errorString();
    if (m_socket)
        m_socket->abort();
}
