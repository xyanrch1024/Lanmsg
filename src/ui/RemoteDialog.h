#pragma once

#include <QDialog>

#include "common/Protocol.h"
#include "discovery/Peer.h"

using qlm::InputEvent;

class NetworkService;
class QLabel;
class QPushButton;

// Controller side of a remote-control session: displays the streamed screen
// and forwards mouse / keyboard events to the controlled machine.
class RemoteDialog : public QDialog {
    Q_OBJECT
public:
    RemoteDialog(const Peer &peer, NetworkService *svc, QWidget *parent = nullptr);
    ~RemoteDialog() override;

    QString peerIp() const { return m_peerIp; }
    QString token() const { return m_token; }

public slots:
    void onAccepted();
    void onDeclined(const QString &reason);
    void onStoppedByPeer();
    void onFrame(const QByteArray &jpeg, int w, int h, qint64 ts);
    void onSessionClosed(const QString &ip);

signals:
    void closed(const QString &token);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    void sendInput(const InputEvent &ev);
    void mapPos(const QPoint &pos, int &x, int &y) const;

    Peer m_peer;
    NetworkService *m_svc = nullptr;
    QString m_peerIp;
    QString m_token;
    QString m_peerName;

    QLabel *m_status = nullptr;
    QLabel *m_screen = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_ctrlAltDel = nullptr;
    QPushButton *m_esc = nullptr;
    QPixmap m_pixmap;

    bool m_controlling = false;
    bool m_connected = false;
    int m_screenW = 0;
    int m_screenH = 0;
};
