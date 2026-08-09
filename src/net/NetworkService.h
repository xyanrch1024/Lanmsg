#pragma once

#include <QHash>
#include <QObject>

#include "common/Protocol.h"
#include "discovery/Peer.h"

using qlm::FileOffer;
using qlm::InputEvent;
using qlm::MsgType;

class QTcpServer;
class QTcpSocket;
class PeerSession;

// Owns the TCP listener and all peer sessions; routes messages between UI and sessions.
class NetworkService : public QObject {
    Q_OBJECT
public:
    explicit NetworkService(QObject *parent = nullptr);
    ~NetworkService() override;

    // Connect to peer if not already connected (async). Safe to call repeatedly.
    void ensureSession(const Peer &peer);
    PeerSession *sessionFor(const QString &ip) const;
    // Any registered session, regardless of connect state (messages queue inside).
    PeerSession *sessionAny(const QString &ip) const { return m_sessions.value(ip); }
    bool isReady(const QString &ip) const;
    QList<PeerSession *> sessions() const;
    QString localIpv4() const;

    // Sending helpers (queued by session until connected).
    bool sendChat(const QString &ip, const QString &text, qint64 ts);
    bool sendFileOffer(const QString &ip, const QString &token, const QString &name, qint64 size);
    bool sendFileAccept(const QString &ip, const QString &token);
    bool sendFileDecline(const QString &ip, const QString &token, const QString &reason);
    bool sendFileDone(const QString &ip, const QString &token);
    bool sendFileCancel(const QString &ip, const QString &token, const QString &reason);
    bool sendFileChunk(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk);

    bool requestRemote(const QString &ip, const QString &token, const QString &password);
    bool acceptRemote(const QString &ip, const QString &token);
    bool declineRemote(const QString &ip, const QString &token, const QString &reason);
    bool sendRemoteFrame(const QString &ip, const QString &token, const QByteArray &jpeg, int w, int h, qint64 ts);
    bool sendRemoteInput(const QString &ip, const QString &token, const InputEvent &ev);
    bool stopRemote(const QString &ip, const QString &token);

signals:
    void sessionReady(const QString &ip, const QString &peerName);
    void sessionClosed(const QString &ip);

    void chatReceived(const QString &ip, const QString &text, qint64 ts);
    void fileOfferReceived(const QString &ip, const FileOffer &offer);
    void fileAcceptReceived(const QString &ip, const QString &token);
    void fileDeclineReceived(const QString &ip, const QString &token, const QString &reason);
    void fileDoneReceived(const QString &ip, const QString &token);
    void fileCancelReceived(const QString &ip, const QString &token, const QString &reason);
    void fileChunkReceived(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk);

    void rcRequestReceived(const QString &ip, const QString &token, bool hasPassword);
    void rcAcceptReceived(const QString &ip, const QString &token);
    void rcDeclineReceived(const QString &ip, const QString &token, const QString &reason);
    void rcFrameReceived(const QString &ip, const QString &token, const QByteArray &jpeg, int w, int h, qint64 ts);
    void rcInputReceived(const QString &ip, const QString &token, const InputEvent &ev);
    void rcStopReceived(const QString &ip, const QString &token);

private slots:
    void onNewConnection();
    void onSessionFrame(PeerSession *s, MsgType type, const QJsonObject &json, const QByteArray &body);
    void onSessionHello(PeerSession *s, const QString &peerName, quint64 seq, const QString &initIp,
                        const QString &initAppId);
    void onSessionClosed(PeerSession *s);

private:
    PeerSession *registerSession(PeerSession *s);
    void unregister(PeerSession *s);
    // Keep the connection with the smaller (initIp, seq, initAppId) key so both sides agree.
    void dedupe(PeerSession *s, const QString &initIp, quint64 seq, const QString &initAppId);

    QTcpServer *m_server = nullptr;
    QHash<QString, PeerSession *> m_sessions; // peer ip -> session
    quint64 m_outSeq = 0;
    QString m_initIp;
};
