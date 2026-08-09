#include "ui/RemoteDialog.h"

#include "common/Config.h"
#include "net/NetworkService.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>

RemoteDialog::RemoteDialog(const Peer &peer, NetworkService *svc, QWidget *parent)
    : QDialog(parent)
    , m_peer(peer)
    , m_svc(svc)
    , m_peerIp(peer.ip)
    , m_token(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_peerName(peer.name) {
    setWindowTitle(QStringLiteral("远程控制 - %1 (%2)").arg(m_peerName, m_peerIp));
    resize(960, 640);

    m_status = new QLabel(QStringLiteral("正在请求控制…"), this);
    m_screen = new QLabel(this);
    m_screen->setMinimumSize(320, 200);
    m_screen->setAlignment(Qt::AlignCenter);
    m_screen->setStyleSheet("background:#1c1c1c; color:#aaa;");
    m_screen->setText(QStringLiteral("等待对方接受控制请求…"));
    m_screen->setFocusPolicy(Qt::StrongFocus);
    // Once the preview is clicked/focused, the remote desktop takes over ALL
    // keyboard input: disable the local IME so raw key events reach
    // keyPressEvent/keyReleaseEvent instead of being swallowed by local
    // composition. Widget-local only, other apps unaffected.
    m_screen->setAttribute(Qt::WA_InputMethodEnabled, false);

    m_stop = new QPushButton(QStringLiteral("断开"), this);
    m_ctrlAltDel = new QPushButton(QStringLiteral("Ctrl+Alt+Del"), this);
    m_esc = new QPushButton(QStringLiteral("ESC"), this);
    installEventFilter(this);

    auto *bar = new QHBoxLayout;
    bar->addWidget(m_status, 1);
    bar->addWidget(m_ctrlAltDel);
    bar->addWidget(m_esc);
    bar->addWidget(m_stop);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(bar);
    lay->addWidget(m_screen, 1);

    connect(m_stop, &QPushButton::clicked, this, &RemoteDialog::close);
    connect(m_ctrlAltDel, &QPushButton::clicked, this, [this] {
        InputEvent ev;
        ev.type = InputEvent::KeyDown;
        ev.key = Qt::Key_Control;
        sendInput(ev);
        ev.key = Qt::Key_Alt;
        sendInput(ev);
        ev.key = Qt::Key_Delete;
        sendInput(ev);
        ev.type = InputEvent::KeyUp;
        ev.key = Qt::Key_Delete;
        sendInput(ev);
        ev.key = Qt::Key_Alt;
        sendInput(ev);
        ev.key = Qt::Key_Control;
        sendInput(ev);
    });
    m_screen->setFocus();
    connect(m_esc, &QPushButton::clicked, this, [this] {
        InputEvent ev;
        ev.type = InputEvent::KeyDown;
        ev.key = Qt::Key_Escape;
        sendInput(ev);
        ev.type = InputEvent::KeyUp;
        ev.key = Qt::Key_Escape;
        sendInput(ev);
    });
    m_screen->setFocus();

    m_svc->requestRemote(m_peerIp, m_token, Config::instance().remotePassword());
    m_screen->setFocus();
}

RemoteDialog::~RemoteDialog() {
    if (m_connected) {
        releaseAllMods();
        InputEvent stop;
        stop.type = InputEvent::KeyUp;
        m_svc->stopRemote(m_peerIp, m_token);
    }
    emit closed(m_token);
}

void RemoteDialog::onAccepted() {
    m_connected = true;
    m_controlling = true;
    m_status->setText(QStringLiteral("正在控制 (点击画面获取焦点)"));
    m_screen->setText(QString());
}

void RemoteDialog::onDeclined(const QString &reason) {
    releaseAllMods();
    m_status->setText(reason.isEmpty() ? QStringLiteral("已拒绝控制请求") : reason);
    m_controlling = false;
    QTimer::singleShot(3000, this, &RemoteDialog::close);
}

void RemoteDialog::onStoppedByPeer() {
    if (!m_connected)
        return;
    m_connected = false;
    m_controlling = false;
    releaseAllMods();
    m_status->setText(QStringLiteral("对方已断开"));
    m_screen->setText(QStringLiteral("对方已断开远程控制"));
    QTimer::singleShot(2500, this, &RemoteDialog::close);
}

void RemoteDialog::onSessionClosed(const QString &ip) {
    if (ip != m_peerIp)
        return;
    if (m_connected) {
        m_connected = false;
        m_controlling = false;
        releaseAllMods();
        m_status->setText(QStringLiteral("连接已断开"));
        m_screen->setText(QStringLiteral("连接已断开"));
    }
    QTimer::singleShot(1500, this, &RemoteDialog::close);
}

void RemoteDialog::onFrame(const QByteArray &jpeg, int w, int h, qint64) {
    m_screenW = w;
    m_screenH = h;
    if (!m_pixmap.loadFromData(jpeg, "JPG"))
        return;
    QPixmap scaled = m_pixmap.scaled(m_screen->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_screen->setPixmap(scaled);
    m_screen->setText(QString());
}

void RemoteDialog::sendInput(const InputEvent &ev) {
    if (m_controlling && m_connected)
        m_svc->sendRemoteInput(m_peerIp, m_token, ev);
}

void RemoteDialog::mapPos(const QPoint &pos, int &x, int &y) const {
    if (m_pixmap.isNull() || m_screenW <= 0 || m_screenH <= 0) {
        x = -1;
        y = -1;
        return;
    }
    const QSize disp = m_pixmap.size();
    if (disp.width() <= 0 || disp.height() <= 0) {
        x = -1;
        y = -1;
        return;
    }
    const QPoint local = pos - m_screen->pos();
    x = static_cast<int>(static_cast<double>(local.x()) * m_screenW / disp.width());
    y = static_cast<int>(static_cast<double>(local.y()) * m_screenH / disp.height());
}

void RemoteDialog::mousePressEvent(QMouseEvent *e) {
    InputEvent ev;
    ev.type = InputEvent::MouseDown;
    ev.button = e->button() == Qt::RightButton ? 3 : (e->button() == Qt::MiddleButton ? 2 : 1);
    mapPos(e->pos(), ev.x, ev.y);
    sendInput(ev);
    m_screen->setFocus();
    QDialog::mousePressEvent(e);
}

void RemoteDialog::mouseReleaseEvent(QMouseEvent *e) {
    InputEvent ev;
    ev.type = InputEvent::MouseUp;
    ev.button = e->button() == Qt::RightButton ? 3 : (e->button() == Qt::MiddleButton ? 2 : 1);
    mapPos(e->pos(), ev.x, ev.y);
    sendInput(ev);
    QDialog::mouseReleaseEvent(e);
}

void RemoteDialog::mouseMoveEvent(QMouseEvent *e) {
    InputEvent ev;
    ev.type = InputEvent::MouseMove;
    mapPos(e->pos(), ev.x, ev.y);
    sendInput(ev);
    QDialog::mouseMoveEvent(e);
}

void RemoteDialog::wheelEvent(QWheelEvent *e) {
    InputEvent ev;
    ev.type = InputEvent::Wheel;
    const QPoint delta = e->angleDelta();
    ev.dy = delta.y() / 120;
    ev.dx = delta.x() / 120;
    sendInput(ev);
    QDialog::wheelEvent(e);
}

int RemoteDialog::modifierKey(Qt::KeyboardModifiers mod) {
    switch (mod) {
    case Qt::ShiftModifier: return Qt::Key_Shift;
    case Qt::ControlModifier: return Qt::Key_Control;
    case Qt::AltModifier: return Qt::Key_Alt;
    case Qt::MetaModifier: return Qt::Key_Meta;
    default: return 0;
    }
}

// Push the local modifier state to the remote as explicit KeyDown/KeyUp events,
// so the remote IME / applications see a real modifier press (Shift+letter
// types uppercase, Ctrl+Space switches the remote IME, ...).
void RemoteDialog::sendModifierDelta(Qt::KeyboardModifiers next) {
    const Qt::KeyboardModifiers mask =
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    next &= mask;
    const Qt::KeyboardModifiers pressed = next & ~m_heldMods;
    const Qt::KeyboardModifiers released = m_heldMods & ~next;
    const Qt::KeyboardModifiers order[] = {Qt::ControlModifier, Qt::AltModifier,
                                           Qt::ShiftModifier, Qt::MetaModifier};
    for (Qt::KeyboardModifiers m : order) {
        if (pressed & m) {
            InputEvent ev;
            ev.type = InputEvent::KeyDown;
            ev.key = modifierKey(m);
            sendInput(ev);
        }
    }
    for (int i = 3; i >= 0; --i) {
        const Qt::KeyboardModifiers m = order[i];
        if (released & m) {
            InputEvent ev;
            ev.type = InputEvent::KeyUp;
            ev.key = modifierKey(m);
            sendInput(ev);
        }
    }
    m_heldMods = next;
}

void RemoteDialog::releaseAllMods() {
    sendModifierDelta(Qt::NoModifier);
}

bool RemoteDialog::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::FocusOut && m_controlling)
        releaseAllMods(); // don't leave modifiers stuck on the remote
    return QDialog::eventFilter(watched, event);
}

void RemoteDialog::keyPressEvent(QKeyEvent *e) {
    if (!(m_controlling && m_connected)) {
        QDialog::keyPressEvent(e);
        return;
    }
    // Auto-repeat (held key) is forwarded as-is; modifiers are only re-sent on
    // state changes, so repeats naturally produce a single modifier press.
    sendModifierDelta(e->modifiers());
    const int key = e->key();
    if (key != Qt::Key_Shift && key != Qt::Key_Control && key != Qt::Key_Alt && key != Qt::Key_Meta) {
        InputEvent ev;
        ev.type = InputEvent::KeyDown;
        ev.key = key;
        sendInput(ev);
    }
    // Consume the event (do NOT call QDialog::keyPressEvent): Escape would
    // otherwise reject/close the dialog instead of reaching the remote.
    e->accept();
}

void RemoteDialog::keyReleaseEvent(QKeyEvent *e) {
    if (!(m_controlling && m_connected)) {
        QDialog::keyReleaseEvent(e);
        return;
    }
    const int key = e->key();
    if (key != Qt::Key_Shift && key != Qt::Key_Control && key != Qt::Key_Alt && key != Qt::Key_Meta) {
        InputEvent ev;
        ev.type = InputEvent::KeyUp;
        ev.key = key;
        sendInput(ev);
    }
    sendModifierDelta(e->modifiers());
    e->accept();
}

void RemoteDialog::closeEvent(QCloseEvent *e) {
    releaseAllMods();
    if (m_connected)
        m_svc->stopRemote(m_peerIp, m_token);
    m_connected = false;
    QDialog::closeEvent(e);
}
