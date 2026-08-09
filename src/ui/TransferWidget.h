#pragma once

#include <QListWidget>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

enum class TransferDirection { Send, Receive };

struct TransferItem {
    QString token;
    TransferDirection direction = TransferDirection::Send;
    QString peerName;
    QString name;
    qint64 total = 0;
    qint64 done = 0;
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
};
