#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include "discovery/Peer.h"

class QUdpSocket;
class QTimer;

// UDP based LAN peer discovery: announce on startup (+ when the local IP
// changes), reply once to brand-new peers, and explicit "bye" on exit. No
// periodic keep-alive broadcast; peers are removed only when a "bye" arrives.
class PeerDiscovery : public QObject {
    Q_OBJECT
public:
    explicit PeerDiscovery(QObject *parent = nullptr);

    QList<Peer> peers() const;
    Peer peerByIp(const QString &ip) const;
    Peer peerById(const QString &id) const;

    // Re-announce our presence (startup, settings change, network change).
    void announce();
    // Tell everyone we are going offline so they drop us immediately.
    void goodbye();

signals:
    void peerAdded(const Peer &peer);
    void peerUpdated(const Peer &peer);
    void peerRemoved(const QString &id);

private slots:
    void readPending();
    void checkNetwork();

private:
    QByteArray packet(const QByteArray &cmd) const;
    void upsert(const Peer &p, bool notify);
    QString localIPv4() const;

    QUdpSocket *m_socket = nullptr;
    QTimer *m_netTimer = nullptr;
    QHash<QString, Peer> m_peers; // key: id
    QString m_lastLocalIp;
};
