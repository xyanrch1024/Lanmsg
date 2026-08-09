#pragma once

#include <QFile>
#include <QObject>

#include "common/Protocol.h"

class NetworkService;

// Sends one file to a peer, chunked with TCP back-pressure pacing.
class FileSender : public QObject {
    Q_OBJECT
public:
    FileSender(const QString &peerIp, const QString &token, const QString &path,
               NetworkService *svc, QObject *parent = nullptr);
    ~FileSender() override;

    void start(); // waits for session ready, then offers the file

    QString token() const { return m_token; }
    QString peerIp() const { return m_peerIp; }
    QString fileName() const { return m_file.fileName(); }
    qint64 totalBytes() const { return m_total; }
    qint64 sentBytes() const { return m_sent; }
    bool active() const { return m_active; }

    void cancel();
    void onCancel(const QString &ip, const QString &token, const QString &reason);

signals:
    void progress(const QString &token, qint64 sent, qint64 total);
    void finished(const QString &token, bool ok, const QString &info);

private slots:
    void onSessionReady(const QString &ip, const QString &name);
    void onAccept(const QString &ip, const QString &token);
    void onDecline(const QString &ip, const QString &token, const QString &reason);
    void pump();

private:
    void finish(bool ok, const QString &info);

    QString m_peerIp;
    QString m_token;
    QFile m_file;
    NetworkService *m_svc = nullptr;
    qint64 m_total = 0;
    qint64 m_sent = 0;
    qint64 m_seq = 0;
    bool m_active = false;
    bool m_doneSent = false;
};
