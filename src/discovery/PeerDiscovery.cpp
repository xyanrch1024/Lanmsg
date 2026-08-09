#include "discovery/PeerDiscovery.h"

#include "common/Config.h"
#include "common/Protocol.h"

#include <QDateTime>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QProcessEnvironment>
#include <QUdpSocket>
#include <QSysInfo>
#include <QTimer>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

PeerDiscovery::PeerDiscovery(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_bcastTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this)) {
    m_socket->bind(QHostAddress::AnyIPv4, qlm::kUdpPort,
                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    // Multicast lets multiple instances on the same host (which share the UDP
    // port via SO_REUSEADDR) all receive discovery. On Windows, unicast and
    // broadcast to a shared port are delivered to a single socket only,
    // multicast is delivered to every joined member. Failure is non-fatal:
    // the LAN broadcast path still works across machines.
    if (m_socket->state() == QAbstractSocket::BoundState) {
        const QHostAddress group{QLatin1String(qlm::kMulticastGroup)};
        if (!m_socket->joinMulticastGroup(group))
            qWarning() << "joinMulticastGroup failed:" << m_socket->errorString();
    } else {
        qWarning() << "discovery UDP bind failed:" << m_socket->errorString()
                   << "(state:" << m_socket->state() << "), multicast disabled";
    }
    const int fd = static_cast<int>(m_socket->socketDescriptor());
    if (fd >= 0) {
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&one), sizeof(one));
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &PeerDiscovery::readPending);

    m_bcastTimer->setInterval(qlm::kDiscoveryIntervalMs);
    connect(m_bcastTimer, &QTimer::timeout, this, &PeerDiscovery::broadcast);
    m_bcastTimer->start();
    broadcast();

    m_cleanupTimer->setInterval(2000);
    connect(m_cleanupTimer, &QTimer::timeout, this, &PeerDiscovery::cleanup);
    m_cleanupTimer->start();
}

QList<Peer> PeerDiscovery::peers() const {
    return m_peers.values();
}

Peer PeerDiscovery::peerByIp(const QString &ip) const {
    for (const Peer &p : m_peers)
        if (p.ip == ip)
            return p;
    return Peer{};
}

Peer PeerDiscovery::peerById(const QString &id) const {
    return m_peers.value(id);
}

QByteArray PeerDiscovery::packet() const {
    QJsonObject o;
    o["magic"] = qlm::kMagic;
    o["ver"] = 1;
    o["id"] = QString::fromUtf8(Config::instance().appId());
    o["name"] = Config::instance().nickname();
    o["host"] = QHostInfo::localHostName();
    o["os"] = QSysInfo::prettyProductName();
    o["app"] = qlm::kAppVersion;
    o["port"] = Config::instance().tcpPort();
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void PeerDiscovery::broadcast() {
    QByteArray data = packet();
    m_socket->writeDatagram(data, QHostAddress(QStringLiteral("255.255.255.255")), qlm::kUdpPort);
    // multicast: required for same-host multi-instance (shared UDP port), also
    // covers broadcast-isolated networks
    m_socket->writeDatagram(data, QHostAddress(QLatin1String(qlm::kMulticastGroup)), qlm::kUdpPort);

    // unicast to known peers too: helps with NAT'd / isolated broadcast domains (e.g. WSL2)
    const auto keys = m_peers.keys();
    for (const QString &id : keys) {
        const Peer &p = m_peers.value(id);
        if (p.ip.isEmpty())
            continue;
        m_socket->writeDatagram(data, QHostAddress(p.ip), qlm::kUdpPort);
    }
}

void PeerDiscovery::readPending() {
    while (m_socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket->readDatagram(data.data(), data.size(), &sender, &senderPort);

        QJsonParseError err;
        QJsonObject o = QJsonDocument::fromJson(data, &err).object();
        if (err.error != QJsonParseError::NoError)
            continue;
        if (o["magic"].toString() != QString::fromUtf8(qlm::kMagic))
            continue;
        const QByteArray id = o["id"].toString().toUtf8();
        if (id.isEmpty() || id == Config::instance().appId())
            continue; // ignore ourselves

        Peer p;
        p.id = QString::fromUtf8(id);
        p.name = o["name"].toString();
        p.host = o["host"].toString();
        p.os = o["os"].toString();
        p.ver = o["app"].toString();
        p.ip = sender.toString();
        p.tcpPort = static_cast<quint16>(o["port"].toInt(qlm::kTcpPort));
        p.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
        const bool isNew = !m_peers.contains(p.id);
        upsert(p, true);

        // Reply only to a brand-new peer so it learns about us immediately.
        // Replying to every datagram makes two clients echo each other's
        // replies forever, saturating the UI thread and freezing the app.
        if (isNew)
            m_socket->writeDatagram(packet(), sender, senderPort ? senderPort : qlm::kUdpPort);
    }
}

void PeerDiscovery::upsert(const Peer &p, bool notify) {
    if (m_peers.contains(p.id)) {
        Peer old = m_peers.value(p.id);
        m_peers[p.id] = p;
        if (notify && (old.ip != p.ip || old.name != p.name)) {
            emit peerUpdated(p);
            if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                qInfo() << "[discovery] updated peer" << p.name << p.ip;
        }
    } else {
        m_peers.insert(p.id, p);
        if (notify) {
            emit peerAdded(p);
            if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                qInfo() << "[discovery] found peer" << p.name << p.ip << "os=" << p.os;
        }
    }
}

void PeerDiscovery::cleanup() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto ids = m_peers.keys();
    for (const QString &id : ids) {
        const Peer &p = m_peers.value(id);
        if (now - p.lastSeenMs > qlm::kPeerTimeoutMs) {
            m_peers.remove(id);
            emit peerRemoved(id);
        }
    }
}
