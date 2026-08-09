#include "net/FileSender.h"

#include "net/NetworkService.h"
#include "net/PeerSession.h"

#include <QFileInfo>
#include <QTimer>

FileSender::FileSender(const QString &peerIp, const QString &token, const QString &path,
                       NetworkService *svc, QObject *parent)
    : QObject(parent)
    , m_peerIp(peerIp)
    , m_token(token)
    , m_file(path)
    , m_svc(svc) {
}

FileSender::~FileSender() {
    if (m_active)
        m_svc->sendFileCancel(m_peerIp, m_token, QStringLiteral("sender closed"));
}

void FileSender::start() {
    connect(m_svc, &NetworkService::sessionReady, this, &FileSender::onSessionReady);
    // If the session is already up, offer immediately; otherwise wait for sessionReady.
    PeerSession *s = m_svc->sessionAny(m_peerIp);
    if (s && s->ready())
        onSessionReady(m_peerIp, QString());
}

void FileSender::onSessionReady(const QString &ip, const QString &) {
    if (ip != m_peerIp || m_active)
        return;
    if (!m_file.open(QIODevice::ReadOnly))
        return finish(false, QStringLiteral("无法打开文件: %1").arg(m_file.errorString()));
    m_total = m_file.size();
    m_sent = 0;
    m_seq = 0;
    m_doneSent = false;
    m_active = true;

    connect(m_svc, &NetworkService::fileAcceptReceived, this, &FileSender::onAccept);
    connect(m_svc, &NetworkService::fileDeclineReceived, this, &FileSender::onDecline);
    connect(m_svc, &NetworkService::fileCancelReceived, this, &FileSender::onCancel);

    if (!m_svc->sendFileOffer(m_peerIp, m_token, QFileInfo(m_file).fileName(), m_total))
        return finish(false, QStringLiteral("连接不可用"));
}

void FileSender::onAccept(const QString &ip, const QString &token) {
    if (ip != m_peerIp || token != m_token)
        return;
    PeerSession *s = m_svc->sessionFor(m_peerIp);
    if (s)
        connect(s, &PeerSession::bytesWrittenToSocket, this, &FileSender::pump);
    pump();
}

void FileSender::onDecline(const QString &ip, const QString &token, const QString &reason) {
    if (ip != m_peerIp || token != m_token)
        return;
    finish(false, reason.isEmpty() ? QStringLiteral("对方拒绝了文件传输") : reason);
}

void FileSender::onCancel(const QString &ip, const QString &token, const QString &reason) {
    if (ip != m_peerIp || token != m_token)
        return;
    finish(false, reason.isEmpty() ? QStringLiteral("传输被取消") : reason);
}

void FileSender::pump() {
    if (!m_active)
        return;
    PeerSession *s = m_svc->sessionFor(m_peerIp);
    if (!s)
        return;
    const qint64 maxQueued = 512 * 1024;
    while (s->socket()->bytesToWrite() < maxQueued) {
        if (m_file.atEnd()) {
            if (!m_doneSent) {
                m_doneSent = true;
                m_svc->sendFileDone(m_peerIp, m_token);
                finish(true, QStringLiteral("发送完成"));
            }
            return;
        }
        QByteArray chunk = m_file.read(qlm::kFileChunkSize);
        if (chunk.isEmpty()) {
            m_svc->sendFileDone(m_peerIp, m_token);
            finish(true, QStringLiteral("发送完成"));
            return;
        }
        m_svc->sendFileChunk(m_peerIp, m_token, m_seq++, chunk);
        m_sent += chunk.size();
        emit progress(m_token, m_sent, m_total);
    }
}

void FileSender::cancel() {
    if (!m_active)
        return;
    m_svc->sendFileCancel(m_peerIp, m_token, QStringLiteral("发送方取消"));
    finish(false, QStringLiteral("已取消"));
}

void FileSender::finish(bool ok, const QString &info) {
    m_active = false;
    m_file.close();
    emit finished(m_token, ok, info);
}
