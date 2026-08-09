#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTcpSocket>

#include "common/Protocol.h"

using qlm::MsgType;

class QTimer;

// One bidirectional TCP connection to a peer.
class PeerSession : public QObject {
    Q_OBJECT
public:
    enum Role { Outgoing, Incoming };

    PeerSession(QTcpSocket *socket, Role role, QObject *parent = nullptr);

    // Canonical peer IP (IPv4-mapped ::ffff:... normalized to plain IPv4).
    // For outgoing sessions this is the destination IP (set explicitly), for
    // incoming ones it is the accepted socket's peer address.
    QString ip() const;
    void setKeyIp(const QString &ip) { m_keyIp = ip; }
    QString peerName() const { return m_peerName; }
    void setPeerName(const QString &n) { m_peerName = n; }
    Role role() const { return m_role; }
    quint64 connSeq() const { return m_connSeq; }
    void setConnSeq(quint64 v) { m_connSeq = v; }
    QString initIp() const { return m_initIp; }
    void setInitIp(const QString &v) { m_initIp = v; }
    QString initAppId() const { return m_initAppId; }
    void setInitAppId(const QString &v) { m_initAppId = v; }
    bool ready() const { return m_ready; }
    QTcpSocket *socket() { return m_socket; }
    void setDropped() { m_dropped = true; }
    bool dropped() const { return m_dropped; }

    // Queue-safe: messages sent before ready() are buffered and flushed on ready.
    void sendMessage(MsgType type, const QJsonObject &json = {}, const QByteArray &body = {});

signals:
    void helloReceived(const QString &peerName, quint64 seq, const QString &initIp, const QString &initAppId);
    void readyChanged(bool ready);
    void frame(const MsgType type, const QJsonObject &json, const QByteArray &body);
    void bytesWrittenToSocket();
    void closed(PeerSession *self);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError err);

private:
    void setReady(bool r);
    void flushQueue();

    QTcpSocket *m_socket = nullptr;
    Role m_role = Incoming;
    QTimer *m_connectTimer = nullptr;
    QByteArray m_buffer;
    QList<QByteArray> m_queue;
    QString m_peerName;
    quint64 m_connSeq = 0;
    QString m_initIp;
    QString m_initAppId;
    QString m_keyIp;
    bool m_ready = false;
    bool m_dropped = false;
    friend class NetworkService;
};
