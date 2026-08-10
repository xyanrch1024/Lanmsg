#include "ui/MainWindow.h"

#include "common/Config.h"
#include "discovery/PeerDiscovery.h"
#include "net/FileReceiver.h"
#include "net/FileSender.h"
#include "net/NetworkService.h"
#include "remote/RemoteServer.h"
#include "ui/ChatWidget.h"
#include "ui/RemoteDialog.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkInterface>
#include <QPainter>
#include <QPixmap>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#ifdef QLANMSG_HAS_MULTIMEDIA
#include <QSoundEffect>
#endif

#include <QDebug>

namespace {
bool isLocalAddress(const QString &ip) {
    if (ip == QStringLiteral("127.0.0.1") || ip == QStringLiteral("::1"))
        return true;
    const auto addrs = QNetworkInterface::allAddresses();
    for (const auto &a : addrs)
        if (a.toString() == ip)
            return true;
    return false;
}

QString fileSizeText(qint64 size) {
    if (size >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(size / 1024.0 / 1024.0, 0, 'f', 1);
    if (size >= 1024)
        return QStringLiteral("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(size);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_discovery(new PeerDiscovery(this))
    , m_net(new NetworkService(this))
    , m_remoteServer(new RemoteServer(m_net, this)) {
    buildUi();

    connect(m_discovery, &PeerDiscovery::peerAdded, this, &MainWindow::updatePeerItem);
    connect(m_discovery, &PeerDiscovery::peerUpdated, this, &MainWindow::updatePeerItem);
    connect(m_discovery, &PeerDiscovery::peerRemoved, this, &MainWindow::removePeerItem);

    connect(m_peerList, &QListWidget::currentItemChanged, this, &MainWindow::onPeerSelected);
    m_peerList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_peerList, &QWidget::customContextMenuRequested, this, &MainWindow::onPeerMenuRequested);

    connect(m_chat, &ChatWidget::sendRequested, this, &MainWindow::onChatSend);
    connect(m_chat, &ChatWidget::attachRequested, this, &MainWindow::onAttachRequested);
    connect(m_chat, &ChatWidget::cancelFileRequested, this, &MainWindow::onCancelFileRequested);
    connect(m_chat, &ChatWidget::openFileRequested, this, &MainWindow::onOpenFileRequested);
    connect(m_net, &NetworkService::chatReceived, this, &MainWindow::onChatReceived);

    connect(m_net, &NetworkService::fileOfferReceived, this, &MainWindow::onFileOffer);
    connect(m_net, &NetworkService::fileChunkReceived, this, &MainWindow::onFileChunk);
    connect(m_net, &NetworkService::fileDoneReceived, this, &MainWindow::onFileDone);
    connect(m_net, &NetworkService::fileCancelReceived, this, &MainWindow::onFileCancel);

    connect(m_remoteServer, &RemoteServer::requestIncoming, this, &MainWindow::onRcRequest);
    connect(m_net, &NetworkService::rcAcceptReceived, this, &MainWindow::onRcAccept);
    connect(m_net, &NetworkService::rcDeclineReceived, this, &MainWindow::onRcDecline);
    connect(m_net, &NetworkService::rcFrameReceived, this, &MainWindow::onRcFrame);
    connect(m_net, &NetworkService::rcStopReceived, this, &MainWindow::onRcStop);
    connect(m_net, &NetworkService::sessionClosed, this, &MainWindow::onSessionClosed);
    connect(m_remoteServer, &RemoteServer::activeChanged, this,
            [this](const QString &, bool) {
                statusBar()->showMessage(QStringLiteral("远程控制会话: %1")
                                             .arg(m_remoteServer->active() ? QStringLiteral("进行中") : QStringLiteral("空闲")),
                                           3000);
            });
    connect(m_remoteServer, &RemoteServer::error, this, [this](const QString &msg) {
        if (!msg.isEmpty())
            statusBar()->showMessage(msg, 5000);
    });

    // Drag & drop a file from the OS file manager to send it.
    for (QWidget *dropTarget : {static_cast<QWidget *>(m_peerList),
                                static_cast<QWidget *>(m_chat)}) {
        dropTarget->setAcceptDrops(true);
        dropTarget->installEventFilter(this);
    }
}

MainWindow::~MainWindow() {
    if (m_discovery)
        m_discovery->goodbye();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Closing hides to the tray (if one is available) instead of quitting;
    // use the tray menu "退出" to really quit.
    if (m_tray) {
        hide();
        m_tray->showMessage(QStringLiteral("QLanMsg"),
                            QStringLiteral("已最小化到托盘，点击托盘图标可恢复。"),
                            QSystemTrayIcon::Information, 2000);
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("QLanMsg - 局域网聊天与远程控制"));
    resize(1000, 640);

    auto *toolbar = addToolBar(QStringLiteral("main"));
    toolbar->setMovable(false);
    QAction *settingsAction = toolbar->addAction(QStringLiteral("设置"));
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    m_peerDelegate = new PeerListDelegate(this);
    m_peerList = new QListWidget(this);
    m_peerList->setObjectName(QStringLiteral("peerList"));
    m_peerList->setItemDelegate(m_peerDelegate);
    m_peerList->setMouseTracking(true);
    m_peerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_peerList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_peerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_peerList->setContextMenuPolicy(Qt::CustomContextMenu);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("searchBox"));
    m_search->setPlaceholderText(QStringLiteral("搜索设备…"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedHeight(32);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &s) {
        for (int i = 0; i < m_peerList->count(); ++i) {
            QListWidgetItem *it = m_peerList->item(i);
            const bool match = s.isEmpty()
                || it->data(Qt::UserRole + 2).toString().contains(s, Qt::CaseInsensitive)
                || it->data(Qt::UserRole + 1).toString().contains(s, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
    });

    auto *leftPanel = new QWidget(this);
    auto *leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(8, 8, 0, 0);
    leftLay->setSpacing(6);
    leftLay->addWidget(m_search);
    leftLay->addWidget(m_peerList, 1);

    m_chat = new ChatWidget(this);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_chat);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    splitter->setSizes({300, 700});
    setCentralWidget(splitter);

    // System tray icon: lets the window hide to tray and shows balloon
    // notifications for incoming messages while hidden.
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QPixmap pm(64, 64);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x3b, 0x82, 0xf6));
        p.drawRoundedRect(4, 4, 56, 56, 14, 14);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSize(26);
        p.setFont(f);
        p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("Q"));
        p.end();

        m_tray = new QSystemTrayIcon(QIcon(pm), this);
        m_tray->setToolTip(QStringLiteral("QLanMsg - 局域网聊天与远程控制"));
        auto *trayMenu = new QMenu(this);
        QAction *showAct = trayMenu->addAction(QStringLiteral("显示主窗口"));
        connect(showAct, &QAction::triggered, this, [this] {
            showNormal();
            raise();
            activateWindow();
        });
        QAction *quitAct = trayMenu->addAction(QStringLiteral("退出"));
        connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
        m_tray->setContextMenu(trayMenu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                showNormal();
                raise();
                activateWindow();
            }
        });
        m_tray->show();
    }

#ifdef QLANMSG_HAS_MULTIMEDIA
    m_sound = new QSoundEffect(this);
    m_sound->setSource(QUrl(QStringLiteral("qrc:/msgnotify.wav")));
    m_sound->setVolume(0.8f);
#endif

    statusBar()->showMessage(QStringLiteral("本机 IP: %1   端口: %2")
                                 .arg(m_net->localIpv4())
                                 .arg(Config::instance().tcpPort()));

    // auto-clean finished transfer rows after a while
    auto *cleanTimer = new QTimer(this);
    cleanTimer->setInterval(30000);
    connect(cleanTimer, &QTimer::timeout, this, &MainWindow::clearFinishedCards);
    cleanTimer->start();

    // test-only: QLANMSG_TEST_CHAT="ip1,ip2" sends a chat to those peers after startup
    const QString testChat = qEnvironmentVariable("QLANMSG_TEST_CHAT");
    if (!testChat.isEmpty()) {
        const auto ips = testChat.split(QLatin1Char(','), Qt::SkipEmptyParts);
        auto *trySend = new QTimer(this);
        connect(trySend, &QTimer::timeout, this, [this, trySend, ips] {
            const int attempts = trySend->property("attempts").toInt() + 1;
            trySend->setProperty("attempts", attempts);
            bool sentAny = false;
            for (const QString &ip : ips) {
                const Peer p = m_discovery->peerByIp(ip);
                if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                    qInfo() << "[test] try chat to" << ip << "found=" << !p.id.isEmpty();
                if (p.id.isEmpty())
                    continue;
                m_net->ensureSession(p);
                QTimer::singleShot(300, this, [this, ip] {
                    m_net->sendChat(ip, QStringLiteral("hello-from-%1").arg(QString::fromUtf8(Config::instance().appId())),
                                    QDateTime::currentMSecsSinceEpoch());
                });
                sentAny = true;
            }
            if (sentAny || attempts > 30)
                trySend->stop();
        });
        trySend->start(500);
    }

    // test-only: QLANMSG_TEST_FILE="ip:/abs/path" sends a file to that peer
    const QString testFile = qEnvironmentVariable("QLANMSG_TEST_FILE");
    if (!testFile.isEmpty()) {
        const int sep = testFile.indexOf(QLatin1Char(':'));
        if (sep > 0) {
            const QString fIp = testFile.left(sep);
            const QString fPath = testFile.mid(sep + 1);
            auto *tryFile = new QTimer(this);
            connect(tryFile, &QTimer::timeout, this, [this, tryFile, fIp, fPath] {
                const Peer p = m_discovery->peerByIp(fIp);
                if (p.id.isEmpty())
                    return;
                tryFile->stop();
                if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                    qInfo() << "[test] sending file to" << fIp << fPath;
                m_net->ensureSession(p);
                const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
                startFileSend(p, fPath, token);
            });
            tryFile->start(500);
        }
    }

    // test-only: QLANMSG_TEST_RC="ip" requests remote control of that peer
    const QString testRc = qEnvironmentVariable("QLANMSG_TEST_RC");
    if (!testRc.isEmpty()) {
        auto *tryRc = new QTimer(this);
        connect(tryRc, &QTimer::timeout, this, [this, tryRc, testRc] {
            const Peer p = m_discovery->peerByIp(testRc);
            if (p.id.isEmpty())
                return;
            tryRc->stop();
            m_net->ensureSession(p);
            auto *whenReady = new QTimer(this);
            connect(m_net, &NetworkService::sessionReady, this, [this, whenReady, testRc](const QString &ip, const QString &) {
                if (ip != testRc)
                    return;
                whenReady->stop();
                if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                    qInfo() << "[test] requesting remote control of" << testRc;
                m_net->requestRemote(testRc, QStringLiteral("testtoken"), Config::instance().remotePassword());
            });
            if (m_net->isReady(testRc)) {
                if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
                    qInfo() << "[test] requesting remote control of" << testRc;
                m_net->requestRemote(testRc, QStringLiteral("testtoken"), Config::instance().remotePassword());
            }
            whenReady->start(8000);
        });
        tryRc->start(500);
    }
}

Peer MainWindow::currentPeer() const {
    return m_discovery->peerByIp(m_currentIp);
}

void MainWindow::onPeerSelected(QListWidgetItem *current, QListWidgetItem *previous) {
    Q_UNUSED(previous);
    if (!current)
        return;
    const QString id = current->data(Qt::UserRole).toString();
    const Peer p = m_discovery->peerById(id);
    if (p.id.isEmpty())
        return;
    m_currentIp = p.ip;
    m_unread.remove(p.ip);
    current->setData(Qt::UserRole + 5, 0);
    m_peerList->viewport()->update();

    const QString os = p.os.isEmpty() ? p.host : p.os;
    m_chat->setPeerInfo(p.name, QStringLiteral("%1 · %2").arg(p.ip, os));
    m_chat->clearLog();
    const auto entries = m_history.value(p.ip);
    for (const ChatEntry &e : entries)
        m_chat->appendMessage(e.who, e.text, e.ts, e.who == QStringLiteral("我"));
    const auto cards = m_fileCards.value(p.ip);
    for (auto it = cards.begin(); it != cards.end(); ++it)
        renderCard(p.ip, it.key());
    m_chat->setEnabled(true);
}

void MainWindow::updatePeerItem(const Peer &p) {
    m_peers.insert(p.id, p);
    QListWidgetItem *it = nullptr;
    for (int i = 0; i < m_peerList->count(); ++i) {
        if (m_peerList->item(i)->data(Qt::UserRole).toString() == p.id) {
            it = m_peerList->item(i);
            break;
        }
    }
    if (!it) {
        it = new QListWidgetItem;
        it->setData(Qt::UserRole, p.id);
        it->setSizeHint(QSize(200, 64));
        m_peerList->addItem(it);
    }
    it->setData(Qt::UserRole, p.id);
    it->setData(Qt::UserRole + 1, p.ip);
    it->setData(Qt::UserRole + 2, p.name);

    const auto &hist = m_history.value(p.ip);
    QString subtitle;
    QString time;
    if (!hist.isEmpty()) {
        const ChatEntry &last = hist.last();
        subtitle = last.text;
        subtitle.replace(QLatin1Char('\n'), QLatin1Char(' '));
        if (subtitle.size() > 40)
            subtitle = subtitle.left(40) + QStringLiteral("…");
        time = QDateTime::fromMSecsSinceEpoch(last.ts).toString(QStringLiteral("HH:mm"));
    } else {
        subtitle = QStringLiteral("%1  ·  %2").arg(p.ip, p.os.isEmpty() ? p.host : p.os);
    }
    it->setData(Qt::UserRole + 3, subtitle);
    it->setData(Qt::UserRole + 4, time);
    it->setData(Qt::UserRole + 5, m_unread.value(p.ip, 0));
    it->setToolTip(QStringLiteral("%1 (%2)\n主机: %3\n系统: %4").arg(p.name, p.ip, p.host, p.os));
    m_peerList->viewport()->update();
}

void MainWindow::removePeerItem(const QString &id) {
    m_peers.remove(id);
    for (int i = 0; i < m_peerList->count(); ++i) {
        if (m_peerList->item(i)->data(Qt::UserRole).toString() == id) {
            delete m_peerList->takeItem(i);
            break;
        }
    }
}

void MainWindow::onPeerMenuRequested(const QPoint &pos) {
    QListWidgetItem *it = m_peerList->itemAt(pos);
    if (!it)
        return;
    const Peer p = m_discovery->peerById(it->data(Qt::UserRole).toString());
    if (p.id.isEmpty())
        return;

    QMenu menu(this);
    QAction *fileAct = menu.addAction(QStringLiteral("发送文件"));
    QAction *rcAct = menu.addAction(QStringLiteral("远程控制"));
    QAction *chatAct = menu.addAction(QStringLiteral("打开聊天"));

    QAction *chosen = menu.exec(m_peerList->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == fileAct)
        sendFileTo(p);
    else if (chosen == rcAct)
        startRemoteControl(p);
    else if (chosen == chatAct)
        selectPeer(p.ip);
}

void MainWindow::selectPeer(const QString &ip) {
    const Peer p = m_discovery->peerByIp(ip);
    if (p.id.isEmpty())
        return;
    const QString id = p.id;
    for (int i = 0; i < m_peerList->count(); ++i) {
        QListWidgetItem *it = m_peerList->item(i);
        if (it->data(Qt::UserRole).toString() == id) {
            m_peerList->setCurrentItem(it);
            break;
        }
    }
}

void MainWindow::onChatSend(const QString &text) {
    const Peer p = currentPeer();
    if (p.id.isEmpty()) {
        m_chat->appendMessage(QStringLiteral("系统"), QStringLiteral("请先在左侧选择设备"), QDateTime::currentMSecsSinceEpoch(), false);
        return;
    }
    ChatEntry e{QStringLiteral("我"), text, QDateTime::currentMSecsSinceEpoch()};
    m_history[p.ip].append(e);
    m_chat->appendMessage(e.who, e.text, e.ts, true);
    m_net->ensureSession(p);
    if (!m_net->sendChat(p.ip, text, e.ts))
        m_chat->appendMessage(QStringLiteral("系统"), QStringLiteral("连接失败，对方可能已离线"), QDateTime::currentMSecsSinceEpoch(), false);
}

void MainWindow::onChatReceived(const QString &ip, const QString &text, qint64 ts) {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[chat] received from" << ip << ":" << text;
    const Peer p = m_discovery->peerByIp(ip);
    const QString who = p.name.isEmpty() ? ip : p.name;
    ChatEntry e{who, text, ts};
    m_history[ip].append(e);
    if (ip == m_currentIp) {
        m_chat->appendMessage(e.who, e.text, e.ts, false);
    } else {
        m_unread[ip] = m_unread.value(ip) + 1;
        setWindowTitle(QStringLiteral("QLanMsg - %1 发来新消息").arg(who));
    }
    if (!p.id.isEmpty())
        updatePeerItem(p); // refresh list preview / time / unread badge
    notifyNewMessage(who, text);
    playNotificationSound();
}

void MainWindow::notifyNewMessage(const QString &who, const QString &text) {
    // Only pop a balloon when the window is minimized or hidden to the tray;
    // when it's visible the chat is already on screen.
    if (m_tray && (isMinimized() || !isVisible())) {
        QString t = text;
        t.replace(QLatin1Char('\n'), QLatin1Char(' '));
        if (t.size() > 80)
            t = t.left(80) + QStringLiteral("…");
        m_tray->showMessage(who, t, QSystemTrayIcon::Information, 3000);
    }
}

void MainWindow::playNotificationSound() {
#ifdef QLANMSG_HAS_MULTIMEDIA
    if (m_sound && m_sound->status() == QSoundEffect::Ready)
        m_sound->play();
    else
        QApplication::beep();
#else
    QApplication::beep();
#endif
}

void MainWindow::sendFileTo(const Peer &peer) {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择要发送的文件"));
    if (path.isEmpty())
        return;
    sendFileTo(peer, path);
}

void MainWindow::sendFileTo(const Peer &peer, const QString &path) {
    if (path.isEmpty())
        return;
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    startFileSend(peer, path, token);
}

void MainWindow::startFileSend(const Peer &peer, const QString &path, const QString &token) {
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    const QString name = info.fileName();
    const qint64 size = info.size();

    m_net->ensureSession(peer);
    auto *sender = new FileSender(peer.ip, token, path, m_net, this);
    m_senders.insert(token, sender);

    FileCardState &card = m_fileCards[peer.ip][token];
    card.token = token;
    card.isSend = true;
    card.peerName = peer.name;
    card.name = name;
    card.total = size;
    card.path = path;
    if (peer.ip == m_currentIp)
        m_chat->addFileCard(token, true, name, size, path);

    connect(sender, &FileSender::progress, this,
            [this, ip = peer.ip](const QString &t, qint64 sent, qint64 total) {
                FileCardState &c = m_fileCards[ip][t];
                c.done = sent;
                c.total = total;
                if (ip == m_currentIp)
                    m_chat->updateFileCard(t, sent, total);
            });
    connect(sender, &FileSender::finished, this,
            [this, ip = peer.ip, name](const QString &t, bool ok, const QString &info) {
                FileCardState &c = m_fileCards[ip][t];
                c.finished = true;
                c.ok = ok;
                c.statusText = ok ? QStringLiteral("完成 · %1").arg(fileSizeText(c.total)) : info;
                if (ip == m_currentIp)
                    m_chat->setFileCardStatus(t, ok, c.statusText);
                const QString text = ok ? QStringLiteral("文件「%1」发送完成").arg(name)
                                        : QStringLiteral("文件「%1」发送失败: %2").arg(name, info);
                appendChatEntry(ip, QStringLiteral("我"), text, QDateTime::currentMSecsSinceEpoch(), true);
                if (FileSender *s = m_senders.take(t))
                    s->deleteLater();
            });
    sender->start();

    appendChatEntry(peer.ip, QStringLiteral("我"),
                    QStringLiteral("发送文件: %1 (%2)").arg(name, fileSizeText(size)),
                    QDateTime::currentMSecsSinceEpoch(), true);
}

void MainWindow::onFileOffer(const QString &ip, const FileOffer &offer) {
    const Peer p = m_discovery->peerByIp(ip);
    const QString who = p.name.isEmpty() ? ip : p.name;
    const QString sizeText = offer.size >= 1024 * 1024
                                 ? QStringLiteral("%1 MB").arg(offer.size / 1024.0 / 1024.0, 0, 'f', 1)
                                 : QStringLiteral("%1 KB").arg(offer.size / 1024.0, 0, 'f', 1);

    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[file] offer from" << ip << offer.name << "size=" << offer.size;

    if (!qEnvironmentVariableIsSet("QLANMSG_AUTO_ACCEPT")) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("接收文件"),
            QStringLiteral("%1 想向你发送文件:\n\n%2\n大小: %3\n\n是否接收?").arg(who, offer.name, sizeText),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes) {
            m_net->sendFileDecline(ip, offer.token, QStringLiteral("用户拒绝接收"));
            return;
        }
    }

    QString dir = Config::instance().downloadDir();
    QDir d(dir);
    QString name = offer.name;
    if (d.exists(name))
        name = QStringLiteral("%1_%2%3").arg(QFileInfo(name).completeBaseName())
                    .arg(QDateTime::currentMSecsSinceEpoch())
                    .arg(QFileInfo(name).completeSuffix().isEmpty() ? QString() : "." + QFileInfo(name).completeSuffix());
    QString savePath;
    if (qEnvironmentVariableIsSet("QLANMSG_AUTO_ACCEPT"))
        savePath = d.absoluteFilePath(name);
    else
        savePath = QFileDialog::getSaveFileName(this, QStringLiteral("保存文件到"), d.absoluteFilePath(name));
    if (savePath.isEmpty()) {
        m_net->sendFileDecline(ip, offer.token, QStringLiteral("未选择保存路径"));
        return;
    }

    auto *receiver = new FileReceiver(ip, offer.token, savePath, offer.size, m_net, this);
    m_receivers.insert(offer.token, receiver);

    FileCardState &card = m_fileCards[ip][offer.token];
    card.token = offer.token;
    card.isSend = false;
    card.peerName = who;
    card.name = offer.name;
    card.total = offer.size;
    card.path = savePath;
    if (ip == m_currentIp)
        m_chat->addFileCard(offer.token, false, offer.name, offer.size, savePath);

    connect(receiver, &FileReceiver::progress, this,
            [this, ip](const QString &t, qint64 received, qint64 total) {
                FileCardState &c = m_fileCards[ip][t];
                c.done = received;
                c.total = total;
                if (ip == m_currentIp)
                    m_chat->updateFileCard(t, received, total);
            });
    connect(receiver, &FileReceiver::finished, this, [this, ip, who, fileName = offer.name](const QString &t, bool ok, const QString &info) {
        if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
            qInfo() << "[test] file receive finished ok=" << ok << info;
        FileCardState &c = m_fileCards[ip][t];
        c.finished = true;
        c.ok = ok;
        c.statusText = ok ? QStringLiteral("完成 · %1").arg(fileSizeText(c.total)) : info;
        if (ip == m_currentIp)
            m_chat->setFileCardStatus(t, ok, c.statusText);
        const QString text = ok ? QStringLiteral("文件「%1」接收完成").arg(fileName)
                                : QStringLiteral("文件「%1」接收失败: %2").arg(fileName, info);
        appendChatEntry(ip, who, text, QDateTime::currentMSecsSinceEpoch(), false);
        if (FileReceiver *r = m_receivers.take(t))
            r->deleteLater();
    });
    connect(m_net, &NetworkService::fileDoneReceived, receiver, &FileReceiver::onDone);
    if (receiver->active())
        m_net->sendFileAccept(ip, offer.token);

    appendChatEntry(ip, who,
                    QStringLiteral("发送文件: %1 (%2)").arg(offer.name, fileSizeText(offer.size)),
                    QDateTime::currentMSecsSinceEpoch(), false);
}

void MainWindow::onFileChunk(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk) {
    if (FileReceiver *r = m_receivers.value(token))
        r->onChunk(ip, token, seq, chunk);
}

void MainWindow::onFileDone(const QString &ip, const QString &token) {
    if (FileReceiver *r = m_receivers.value(token))
        r->onDone(ip, token);
}

void MainWindow::onFileCancel(const QString &ip, const QString &token, const QString &reason) {
    if (FileReceiver *r = m_receivers.value(token))
        r->onCancel(ip, token, reason);
    if (FileSender *s = m_senders.value(token))
        s->onCancel(ip, token, reason);
}

void MainWindow::appendChatEntry(const QString &ip, const QString &who, const QString &text, qint64 ts, bool isSelf) {
    m_history[ip].append(ChatEntry{who, text, ts});
    if (ip == m_currentIp)
        m_chat->appendMessage(who, text, ts, isSelf);
}

QString MainWindow::ipByToken(const QString &token) const {
    for (auto it = m_fileCards.begin(); it != m_fileCards.end(); ++it)
        if (it.value().contains(token))
            return it.key();
    return QString();
}

void MainWindow::renderCard(const QString &ip, const QString &token) {
    if (ip != m_currentIp)
        return;
    const FileCardState &c = m_fileCards.value(ip).value(token);
    m_chat->addFileCard(token, c.isSend, c.name, c.total, c.path);
    if (c.finished)
        m_chat->setFileCardStatus(token, c.ok, c.statusText);
    else if (c.done > 0)
        m_chat->updateFileCard(token, c.done, c.total);
}

void MainWindow::onAttachRequested() {
    const Peer p = currentPeer();
    if (p.id.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("请先在左侧选择设备"), 3000);
        return;
    }
    sendFileTo(p);
}

void MainWindow::onCancelFileRequested(const QString &token) {
    if (FileSender *s = m_senders.value(token))
        s->cancel();
    else if (FileReceiver *r = m_receivers.value(token))
        r->cancel();
}

void MainWindow::onOpenFileRequested(const QString &token) {
    const QString ip = ipByToken(token);
    if (ip.isEmpty())
        return;
    const FileCardState &c = m_fileCards.value(ip).value(token);
    if (!c.finished || !c.ok || c.path.isEmpty())
        return;
    if (!QFileInfo::exists(c.path)) {
        statusBar()->showMessage(QStringLiteral("文件不存在: %1").arg(c.path), 4000);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(c.path));
}

void MainWindow::clearFinishedCards() {
    for (auto it = m_fileCards.begin(); it != m_fileCards.end(); ++it) {
        bool removedAny = false;
        const QStringList tokens = it.value().keys();
        for (const QString &token : tokens) {
            const FileCardState &c = it.value().value(token);
            if (c.finished) {
                it.value().remove(token);
                removedAny = true;
            }
        }
        if (removedAny && it.key() == m_currentIp)
            m_chat->removeFinishedCards();
    }
}

void MainWindow::startRemoteControl(const Peer &peer) {
    if (isLocalAddress(peer.ip)) {
        QMessageBox::warning(this, QStringLiteral("远程控制"),
                             QStringLiteral("不能远程控制本机:同机控制会抓取到控制窗口自身，产生无限画面嵌套。"));
        return;
    }
    m_net->ensureSession(peer);
    auto *dlg = new RemoteDialog(peer, m_net, this);
    m_remoteDialogs.insert(rcKey(peer.ip, dlg->token()), dlg);
    connect(dlg, &RemoteDialog::closed, this, [this, dlg](const QString &) {
        m_remoteDialogs.remove(rcKey(dlg->peerIp(), dlg->token()));
        dlg->deleteLater();
    });
    dlg->setAttribute(Qt::WA_DeleteOnClose, false);
    dlg->show();
    dlg->raise();
}

void MainWindow::onRcRequest(const QString &ip, const QString &token, const QString &requestName, bool hasPassword) {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[rc] incoming request from" << ip << requestName << "token=" << token;

    if (isLocalAddress(ip)) {
        if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
            qInfo() << "[rc] rejecting same-host request from" << ip;
        m_remoteServer->respond(ip, token, false);
        return;
    }

    if (qEnvironmentVariableIsSet("QLANMSG_AUTO_ACCEPT")) {
        m_remoteServer->respond(ip, token, true);
        return;
    }

    const QString configured = Config::instance().remotePassword();
    bool accept = false;

    if (!configured.isEmpty() || hasPassword) {
        bool ok = false;
        const QString pw = QInputDialog::getText(
            this, QStringLiteral("远程控制请求"),
            QStringLiteral("%1 请求控制你的电脑。\n请输入远程控制密码:").arg(requestName),
            QLineEdit::Password, QString(), &ok);
        if (ok)
            accept = (pw == configured);
        if (ok && !accept)
            QMessageBox::warning(this, QStringLiteral("远程控制"), QStringLiteral("密码错误，已拒绝。"));
    } else {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("远程控制请求"),
            QStringLiteral("%1 请求控制你的电脑。\n\n是否允许?").arg(requestName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        accept = (answer == QMessageBox::Yes);
    }
    m_remoteServer->respond(ip, token, accept);
}

void MainWindow::onRcAccept(const QString &ip, const QString &token) {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[rc] accepted by" << ip;
    if (RemoteDialog *d = m_remoteDialogs.value(rcKey(ip, token)))
        d->onAccepted();
    if (qEnvironmentVariable("QLANMSG_TEST_RCINPUT") == ip) {
        if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
            qInfo() << "[test] injecting mouse move via" << ip;
        qlm::InputEvent ev;
        ev.type = qlm::InputEvent::MouseMove;
        ev.x = 8;
        ev.y = 8;
        m_net->sendRemoteInput(ip, token, ev);
    }
}

void MainWindow::onRcDecline(const QString &ip, const QString &token, const QString &reason) {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[rc] declined by" << ip << reason;
    if (RemoteDialog *d = m_remoteDialogs.value(rcKey(ip, token)))
        d->onDeclined(reason);
}

void MainWindow::onRcFrame(const QString &ip, const QString &token, const QByteArray &jpeg, int w, int h, qint64 ts) {
    if (qEnvironmentVariableIsSet("QLANMSG_LOG"))
        qInfo() << "[rc] frame from" << ip << w << "x" << h << "bytes=" << jpeg.size();
    if (RemoteDialog *d = m_remoteDialogs.value(rcKey(ip, token)))
        d->onFrame(jpeg, w, h, ts);
}

void MainWindow::onRcStop(const QString &ip, const QString &token) {
    if (RemoteDialog *d = m_remoteDialogs.value(rcKey(ip, token)))
        d->onStoppedByPeer();
}

void MainWindow::onSessionClosed(const QString &ip) {
    m_remoteServer->onSessionClosed(ip);
    const auto keys = m_remoteDialogs.keys();
    for (const QString &k : keys) {
        if (k.startsWith(ip + QLatin1Char('|')))
            m_remoteDialogs.value(k)->onSessionClosed(ip);
    }
}

void MainWindow::openSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // nickname change takes effect on next broadcast
        m_discovery->announce();
        statusBar()->showMessage(QStringLiteral("设置已保存"), 2000);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_peerList && watched != m_chat)
        return QMainWindow::eventFilter(watched, event);

    if (event->type() == QEvent::DragEnter) {
        auto *de = static_cast<QDragEnterEvent *>(event);
        if (de->mimeData()->hasUrls()) {
            de->acceptProposedAction();
            return true;
        }
    } else if (event->type() == QEvent::DragMove) {
        auto *dm = static_cast<QDragMoveEvent *>(event);
        if (watched == m_peerList) {
            // Highlight the peer row under the cursor as the drop target.
            QListWidgetItem *it = m_peerList->itemAt(dm->position().toPoint());
            if (it)
                m_peerList->setCurrentItem(it);
        }
        if (dm->mimeData()->hasUrls())
            dm->acceptProposedAction();
        return true;
    } else if (event->type() == QEvent::Drop) {
        auto *de = static_cast<QDropEvent *>(event);
        QStringList paths;
        const QList<QUrl> urls = de->mimeData()->urls();
        for (const QUrl &u : urls) {
            if (u.isLocalFile() && QFileInfo(u.toLocalFile()).isFile())
                paths << u.toLocalFile();
        }
        if (paths.isEmpty())
            return true;

        Peer target = currentPeer();
        if (watched == m_peerList) {
            QListWidgetItem *it = m_peerList->itemAt(de->position().toPoint());
            if (it) {
                const Peer p = m_discovery->peerById(it->data(Qt::UserRole).toString());
                if (!p.id.isEmpty())
                    target = p;
            }
        }
        if (target.id.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("请先选择要发送文件的设备"), 3000);
        } else {
            for (const QString &path : paths)
                sendFileTo(target, path);
        }
        de->acceptProposedAction();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}
