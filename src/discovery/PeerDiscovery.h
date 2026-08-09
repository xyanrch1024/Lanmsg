#pragma once

#include <QHash>
#include <QObject>

#include "discovery/Peer.h"

class QUdpSocket;
class QTimer;

// UDP based LAN peer discovery: periodic broadcast + unicast reply + expiry.
class PeerDiscovery : public QObject {
    Q_OBJECT
public:
    explicit PeerDiscovery(QObject *parent = nullptr);

    QList<Peer> peers() const;
    Peer peerByIp(const QString &ip) const;
    Peer peerById(const QString &id) const;

signals:
    void peerAdded(const Peer &peer);
    void peerUpdated(const Peer &peer);
    void peerRemoved(const QString &id);

public slots:
    void broadcast();

private slots:
    void readPending();
    void cleanup();

private:
    QByteArray packet() const;
    void upsert(const Peer &p, bool notify);

    QUdpSocket *m_socket = nullptr;
    QTimer *m_bcastTimer = nullptr;
    QTimer *m_cleanupTimer = nullptr;
    QHash<QString, Peer> m_peers; // key: id
};
