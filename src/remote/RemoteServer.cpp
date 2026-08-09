#include "remote/RemoteServer.h"

#include "common/Config.h"
#include "net/NetworkService.h"
#include "net/PeerSession.h"
#include "remote/IScreenSource.h"
#include "remote/IInputSink.h"

#if defined(Q_OS_LINUX)
#include "remote/InputSinkX11.h"
#include "remote/ScreenSourceX11.h"
#elif defined(Q_OS_WIN)
#include "remote/InputSinkWindows.h"
#include "remote/ScreenSourceWindows.h"
#endif

#include <QBuffer>
#include <QDateTime>
#include <QImage>
#include <QImageWriter>
#include <QThread>
#include <QTimer>

#include <QDebug>

CaptureWorker::CaptureWorker(QObject *parent)
    : QObject(parent) {}

CaptureWorker::~CaptureWorker() {
    if (m_timer)
        m_timer->stop();
    if (m_source) {
        m_source->shutdown();
        delete m_source;
    }
}

void CaptureWorker::start(int intervalMs, int quality) {
    m_quality = quality;
    m_nullFrames = 0;
    m_blankFrames = 0;
    if (!m_source) {
#if defined(Q_OS_LINUX)
        m_source = new ScreenSourceX11;
#elif defined(Q_OS_WIN)
        m_source = new ScreenSourceWindows;
#else
        emit failed(QStringLiteral("当前平台不支持截屏"));
        return;
#endif
    }
    if (!m_source->initialize()) {
        emit failed(QStringLiteral("无法初始化屏幕采集"));
        delete m_source;
        m_source = nullptr;
        return;
    }
    m_timer = new QTimer(this);
    m_timer->setInterval(intervalMs);
    connect(m_timer, &QTimer::timeout, this, &CaptureWorker::tick);
    m_timer->start();
}

void CaptureWorker::stop() {
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    if (m_source) {
        m_source->shutdown();
        delete m_source;
        m_source = nullptr;
    }
}

bool CaptureWorker::isBlank(const QImage &img) {
    if (img.isNull())
        return true;
    const uchar *bits = img.constBits();
    const qint64 total = static_cast<qint64>(img.bytesPerLine()) * img.height();
    qint64 sum = 0;
    qint64 count = 0;
    const qint64 step = 16;
    for (qint64 i = 0; i < total; i += step) {
        sum += bits[i];
        ++count;
    }
    if (count > 0 && qEnvironmentVariableIsSet("QLANMSG_LOG") && (sum / count) < 16)
        qInfo() << "[rc] blank level mean=" << (sum / count);
    return count > 0 && sum / count < 8; // near-black frame
}

void CaptureWorker::tick() {
    if (!m_source)
        return;
    QImage img = m_source->grab();
    if (img.isNull()) {
        // silent null frames (e.g. XGetImage BadMatch in WSLg) mean no capture
        if (++m_nullFrames >= 10) {
            m_nullFrames = 0;
            emit failed(QStringLiteral("无法采集屏幕画面(显示服务拒绝返回图像)"));
            if (m_timer)
                m_timer->stop();
        }
        return;
    }
    m_nullFrames = 0;
    if (isBlank(img)) {
        // a fully black capture means the real desktop is not readable
        // (WSLg composites the desktop host-side; Xwayland root is empty)
        if (++m_blankFrames >= 10) {
            m_blankFrames = 0;
            emit failed(QStringLiteral("桌面画面为空白:WSLg/无 X11 桌面环境下无法采集真实屏幕"));
            if (m_timer)
                m_timer->stop();
            return;
        }
    } else {
        m_blankFrames = 0;
    }
    QByteArray jpeg;
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    QImageWriter w(&buf, "JPG");
    w.setQuality(m_quality);
    if (w.write(img))
        emit frameReady(jpeg, img.width(), img.height(), QDateTime::currentMSecsSinceEpoch());
}

RemoteServer::RemoteServer(NetworkService *svc, QObject *parent)
    : QObject(parent)
    , m_svc(svc) {
    connect(m_svc, &NetworkService::rcRequestReceived, this, &RemoteServer::onRcRequest);
    connect(m_svc, &NetworkService::rcInputReceived, this, &RemoteServer::onRcInput);
    connect(m_svc, &NetworkService::rcStopReceived, this, &RemoteServer::onRcStop);
    connect(m_svc, &NetworkService::sessionClosed, this, &RemoteServer::onSessionClosed);
}

RemoteServer::~RemoteServer() {
    end(QStringLiteral("退出"));
}

void RemoteServer::setJpegQuality(int q) {
    m_jpegQuality = q;
}

void RemoteServer::setFps(int fps) {
    m_frameIntervalMs = fps > 0 ? 1000 / qMin(fps, 60) : 100;
}

void RemoteServer::onRcRequest(const QString &ip, const QString &token, bool hasPassword) {
    if (active()) {
        m_svc->declineRemote(ip, token, QStringLiteral("正在被另一台电脑控制"));
        return;
    }
    emit requestIncoming(ip, token, m_svc->sessionFor(ip) ? m_svc->sessionFor(ip)->peerName() : ip, hasPassword);
}

void RemoteServer::respond(const QString &ip, const QString &token, bool accept) {
    if (accept) {
        const QString configured = Config::instance().remotePassword();
        // Password policy is enforced by MainWindow before calling respond().
        begin(ip, token);
        m_svc->acceptRemote(ip, token);
    } else {
        m_svc->declineRemote(ip, token, QStringLiteral("对方拒绝了控制请求"));
    }
}

void RemoteServer::begin(const QString &ip, const QString &token) {
    m_peerIp = ip;
    m_token = token;
    m_thread = new QThread(this);
    m_worker = new CaptureWorker;
    m_worker->moveToThread(m_thread);
    connect(m_worker, &CaptureWorker::frameReady, this, &RemoteServer::onFrameReady, Qt::QueuedConnection);
    connect(m_worker, &CaptureWorker::failed, this, [this, ip, token](const QString &msg) {
        // tell the controller why the stream is unusable instead of a silent black screen
        m_svc->declineRemote(ip, token, msg);
        emit error(msg);
        end(msg);
    });
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
    QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection,
                              Q_ARG(int, m_frameIntervalMs), Q_ARG(int, m_jpegQuality));
    emit activeChanged(ip, true);
}

void RemoteServer::onFrameReady(const QByteArray &jpeg, int w, int h, qint64 ts) {
    if (!active())
        return;
    m_svc->sendRemoteFrame(m_peerIp, m_token, jpeg, w, h, ts);
}

void RemoteServer::onRcInput(const QString &ip, const QString &token, const InputEvent &ev) {
    if (ip != m_peerIp || token != m_token)
        return;
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[rc] input from" << ip << "type=" << int(ev.type) << ev.x << ev.y << "key=" << ev.key;
    if (!m_input) {
#if defined(Q_OS_LINUX)
        m_input = new InputSinkX11;
#elif defined(Q_OS_WIN)
        m_input = new InputSinkWindows;
#else
        emit error(QStringLiteral("当前平台不支持输入注入"));
        return;
#endif
        if (!m_input->initialize()) {
            emit error(QStringLiteral("无法初始化输入注入"));
            delete m_input;
            m_input = nullptr;
            return;
        }
    }
    switch (ev.type) {
    case InputEvent::MouseMove:
        m_input->mouseMove(ev.x, ev.y);
        break;
    case InputEvent::MouseDown:
        m_input->mouseButton(ev.button, true, ev.x, ev.y);
        break;
    case InputEvent::MouseUp:
        m_input->mouseButton(ev.button, false, ev.x, ev.y);
        break;
    case InputEvent::Wheel:
        m_input->wheel(ev.dx, ev.dy);
        break;
    case InputEvent::KeyDown:
        m_input->key(ev.key, true);
        break;
    case InputEvent::KeyUp:
        m_input->key(ev.key, false);
        break;
    }
}

void RemoteServer::onRcStop(const QString &ip, const QString &token) {
    if (ip != m_peerIp || token != m_token)
        return;
    end(QStringLiteral("对方已断开远程控制"));
}

void RemoteServer::onSessionClosed(const QString &ip) {
    if (ip == m_peerIp)
        end(QStringLiteral("连接已断开"));
}

void RemoteServer::stopCurrent() {
    if (!active())
        return;
    m_svc->stopRemote(m_peerIp, m_token);
    end(QStringLiteral("已停止远程控制"));
}

void RemoteServer::end(const QString &reason) {
    if (!active())
        return;
    const QString ip = m_peerIp;
    m_peerIp.clear();
    m_token.clear();
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
        if (m_thread) {
            m_thread->quit();
            m_thread->wait(2000);
        }
    }
    m_worker = nullptr;
    m_thread = nullptr;
    if (m_input) {
        m_input->shutdown();
        delete m_input;
        m_input = nullptr;
    }
    if (!reason.isEmpty())
        qInfo() << "Remote control ended:" << reason;
    emit activeChanged(ip, false);
}
