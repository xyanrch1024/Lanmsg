#include "net/FileReceiver.h"

#include "net/NetworkService.h"

#include <QDir>
#include <QFileInfo>

FileReceiver::FileReceiver(const QString &peerIp, const QString &token, const QString &savePath,
                           qint64 totalSize, NetworkService *svc, QObject *parent)
    : QObject(parent)
    , m_peerIp(peerIp)
    , m_token(token)
    , m_file(savePath)
    , m_svc(svc)
    , m_total(totalSize)
    , m_active(true) {
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finish(false, QStringLiteral("无法创建文件: %1").arg(m_file.errorString()));
        return;
    }
    connect(m_svc, &NetworkService::fileCancelReceived, this, &FileReceiver::onCancel);
}

void FileReceiver::onChunk(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk) {
    if (!m_active || ip != m_peerIp || token != m_token)
        return;
    const qint64 pos = seq * qlm::kFileChunkSize;
    if (pos != m_file.pos())
        m_file.seek(pos);
    if (m_file.write(chunk) != chunk.size()) {
        finish(false, QStringLiteral("写入失败: %1").arg(m_file.errorString()));
        return;
    }
    m_received += chunk.size();
    emit progress(m_token, m_received, m_total);
    if (m_received >= m_total) {
        m_svc->sendFileDone(m_peerIp, m_token);
        finish(true, QStringLiteral("接收完成"));
    }
}

void FileReceiver::onCancel(const QString &ip, const QString &token, const QString &reason) {
    if (!m_active || ip != m_peerIp || token != m_token)
        return;
    finish(false, reason.isEmpty() ? QStringLiteral("传输被取消") : reason);
}

void FileReceiver::onDone(const QString &ip, const QString &token) {
    if (!m_active || ip != m_peerIp || token != m_token)
        return;
    m_svc->sendFileDone(m_peerIp, m_token);
    finish(true, QStringLiteral("接收完成"));
}

void FileReceiver::cancel() {
    if (!m_active)
        return;
    m_svc->sendFileCancel(m_peerIp, m_token, QStringLiteral("接收方取消"));
    finish(false, QStringLiteral("已取消"));
}

void FileReceiver::finish(bool ok, const QString &info) {
    m_active = false;
    m_file.close();
    if (!ok)
        m_file.remove();
    emit finished(m_token, ok, info);
}
