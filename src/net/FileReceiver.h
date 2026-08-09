#pragma once

#include <QFile>
#include <QFileInfo>
#include <QObject>

#include "common/Protocol.h"

class NetworkService;

// Receives one file from a peer, writing chunks to disk in order.
class FileReceiver : public QObject {
    Q_OBJECT
public:
    FileReceiver(const QString &peerIp, const QString &token, const QString &savePath,
                 qint64 totalSize, NetworkService *svc, QObject *parent = nullptr);

    QString token() const { return m_token; }
    QString peerIp() const { return m_peerIp; }
    QString fileName() const { return QFileInfo(m_file).fileName(); }
    qint64 totalBytes() const { return m_total; }
    qint64 receivedBytes() const { return m_received; }
    bool active() const { return m_active; }

public slots:
    void onChunk(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk);
    void onCancel(const QString &ip, const QString &token, const QString &reason);
    void onDone(const QString &ip, const QString &token);
    void cancel();

signals:
    void progress(const QString &token, qint64 received, qint64 total);
    void finished(const QString &token, bool ok, const QString &info);

private:
    void finish(bool ok, const QString &info);

    QString m_peerIp;
    QString m_token;
    QFile m_file;
    NetworkService *m_svc = nullptr;
    qint64 m_total = 0;
    qint64 m_received = 0;
    bool m_active = false;
};
