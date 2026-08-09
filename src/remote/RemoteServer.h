#pragma once

#include <QObject>
#include <QString>

#include "common/Protocol.h"

using qlm::InputEvent;

class NetworkService;
class QThread;
class QTimer;
class IScreenSource;
class IInputSink;

// Capture + JPEG encode worker, lives in its own thread.
class CaptureWorker : public QObject {
    Q_OBJECT
public:
    explicit CaptureWorker(QObject *parent = nullptr);
    ~CaptureWorker() override;

public slots:
    void start(int intervalMs, int quality);
    void stop();

signals:
    void frameReady(const QByteArray &jpeg, int w, int h, qint64 ts);
    void failed(const QString &msg);

private slots:
    void tick();

private:
    QTimer *m_timer = nullptr;
    IScreenSource *m_source = nullptr;
    int m_quality = 65;
};

// Receives remote-control requests: asks for permission, then streams the screen
// and injects incoming input on the controlled machine.
class RemoteServer : public QObject {
    Q_OBJECT
public:
    explicit RemoteServer(NetworkService *svc, QObject *parent = nullptr);
    ~RemoteServer() override;

    bool active() const { return !m_peerIp.isEmpty(); }
    QString peerIp() const { return m_peerIp; }

    void setJpegQuality(int q);
    void setFps(int fps);

signals:
    void requestIncoming(const QString &ip, const QString &token, const QString &requestName, bool hasPassword);
    void activeChanged(const QString &ip, bool active);
    void error(const QString &msg);

public slots:
    void respond(const QString &ip, const QString &token, bool accept);
    void stopCurrent();
    void onSessionClosed(const QString &ip);

private slots:
    void onRcRequest(const QString &ip, const QString &token, bool hasPassword);
    void onRcInput(const QString &ip, const QString &token, const InputEvent &ev);
    void onRcStop(const QString &ip, const QString &token);
    void onFrameReady(const QByteArray &jpeg, int w, int h, qint64 ts);

private:
    void begin(const QString &ip, const QString &token);
    void end(const QString &reason);

    NetworkService *m_svc = nullptr;
    QThread *m_thread = nullptr;
    CaptureWorker *m_worker = nullptr;
    IInputSink *m_input = nullptr;
    QString m_peerIp;
    QString m_token;
    int m_jpegQuality = 65;
    int m_frameIntervalMs = 100;
};
