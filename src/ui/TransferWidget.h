#pragma once

#include <QListWidget>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

enum class TransferDirection { Send, Receive };

struct TransferItem {
    QString token;
    TransferDirection direction = TransferDirection::Send;
    QString peerName;
    QString name;
    qint64 total = 0;
    qint64 done = 0;
    qint64 lastSampleMs = 0;    // time of last speed sample (ms epoch)
    qint64 lastSampleDone = 0;  // bytes at last sample
    double speedBps = 0;        // smoothed transfer rate (bytes/s)
};

class TransferWidget : public QListWidget {
    Q_OBJECT
public:
    explicit TransferWidget(QWidget *parent = nullptr);

    void addItem(const TransferItem &item);
    void updateProgress(const QString &token, qint64 done, qint64 total);
    void setStatus(const QString &token, bool finished, const QString &text);

    QListWidgetItem *itemFor(const QString &token) const;

signals:
    void cancelRequested(const QString &token);

private:
    void refreshRow(QListWidgetItem *it);
    void refreshActiveRows();

    QTimer *m_refreshTimer = nullptr;
};
