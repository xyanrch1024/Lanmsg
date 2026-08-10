#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>

#include "common/Protocol.h"
#include "discovery/Peer.h"
#include "ui/PeerListDelegate.h"

using qlm::FileOffer;

#ifdef QLANMSG_HAS_MULTIMEDIA
class QSoundEffect;
#endif

class ChatWidget;
class FileReceiver;
class FileSender;
class NetworkService;
class PeerDiscovery;
class RemoteDialog;
class RemoteServer;
class QSystemTrayIcon;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;

struct ChatEntry {
    QString who;
    QString text;
    qint64 ts = 0;
};

struct FileCardState {
    QString token;
    bool isSend = false;
    QString peerName;
    QString name;
    QString path;   // local file (source for send, destination for receive)
    qint64 total = 0;
    qint64 done = 0;
    bool finished = false;
    bool ok = false;
    QString statusText;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onPeerSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onPeerMenuRequested(const QPoint &pos);
    void onChatSend(const QString &text);
    void onChatReceived(const QString &ip, const QString &text, qint64 ts);
    void onFileOffer(const QString &ip, const FileOffer &offer);
    void onFileChunk(const QString &ip, const QString &token, qint64 seq, const QByteArray &chunk);
    void onFileDone(const QString &ip, const QString &token);
    void onFileCancel(const QString &ip, const QString &token, const QString &reason);
    void onRcRequest(const QString &ip, const QString &token, const QString &requestName, bool hasPassword);
    void onRcAccept(const QString &ip, const QString &token);
    void onRcDecline(const QString &ip, const QString &token, const QString &reason);
    void onRcFrame(const QString &ip, const QString &token, const QByteArray &jpeg, int w, int h, qint64 ts);
    void onRcStop(const QString &ip, const QString &token);
    void onSessionClosed(const QString &ip);
    void onAttachRequested();
    void onCancelFileRequested(const QString &token);
    void onOpenFileRequested(const QString &token);
    void openSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void updatePeerItem(const Peer &p);
    void removePeerItem(const QString &id);
    Peer currentPeer() const;
    void selectPeer(const QString &ip);
    void sendFileTo(const Peer &peer);
    void sendFileTo(const Peer &peer, const QString &path);
    void startFileSend(const Peer &peer, const QString &path, const QString &token);
    void startRemoteControl(const Peer &peer);
    QString rcKey(const QString &ip, const QString &token) const { return ip + QLatin1Char('|') + token; }
    QString ipByToken(const QString &token) const;
    void appendChatEntry(const QString &ip, const QString &who, const QString &text, qint64 ts, bool isSelf);
    void renderCard(const QString &ip, const QString &token);
    void clearFinishedCards();
    void notifyNewMessage(const QString &who, const QString &text);
    void playNotificationSound();

    PeerDiscovery *m_discovery = nullptr;
    NetworkService *m_net = nullptr;
    RemoteServer *m_remoteServer = nullptr;

    QLineEdit *m_search = nullptr;
    QListWidget *m_peerList = nullptr;
    PeerListDelegate *m_peerDelegate = nullptr;
    ChatWidget *m_chat = nullptr;

    QHash<QString, QList<ChatEntry>> m_history;   // ip -> chat history
    QHash<QString, int> m_unread;                 // ip -> unseen incoming messages
    QHash<QString, Peer> m_peers;                 // id -> peer
    QHash<QString, FileSender *> m_senders;       // token -> sender
    QHash<QString, FileReceiver *> m_receivers;   // token -> receiver
    QHash<QString, QHash<QString, FileCardState>> m_fileCards; // ip -> token -> card
    QHash<QString, RemoteDialog *> m_remoteDialogs; // "ip|token" -> dialog

    QSystemTrayIcon *m_tray = nullptr;
#ifdef QLANMSG_HAS_MULTIMEDIA
    QSoundEffect *m_sound = nullptr;
#endif

    QString m_currentIp;
};
