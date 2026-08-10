#pragma once

#include <QDate>
#include <QHash>
#include <QList>
#include <QWidget>

class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QVBoxLayout;

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void setPeerInfo(const QString &name, const QString &subtitle);
    void setPeerName(const QString &name);

    void appendMessage(const QString &who, const QString &text, qint64 ts, bool isSelf);
    void clearLog();

    // In-chat file transfer cards, keyed by transfer token.
    void addFileCard(const QString &token, bool isSend, const QString &name, qint64 total,
                     const QString &path);
    void updateFileCard(const QString &token, qint64 done, qint64 total);
    void setFileCardStatus(const QString &token, bool ok, const QString &text);
    void removeFinishedCards();

signals:
    void sendRequested(const QString &text);
    void attachRequested();
    void cancelFileRequested(const QString &token);
    void openFileRequested(const QString &token);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void doSend();
    void scrollToBottom();
    void insertRow(QWidget *row);
    void applyBubbleColors(QFrame *bubble, bool isSend);

    struct FileCardRow {
        QFrame *bubble = nullptr;
        QLabel *name = nullptr;
        QLabel *detail = nullptr;
        QProgressBar *bar = nullptr;
        QPushButton *cancel = nullptr;
        qint64 lastMs = 0;
        qint64 lastDone = 0;
        double speedBps = 0;
        bool finished = false;
        bool ok = false;
    };

    QLabel *m_avatar = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_bubblesHost = nullptr;
    QVBoxLayout *m_bubbles = nullptr;
    QTextEdit *m_input = nullptr;
    QPushButton *m_attachButton = nullptr;
    QPushButton *m_sendButton = nullptr;

    int m_bubbleMaxWidth = 460;
    QDate m_lastDate;
    QList<QFrame *> m_bubblesFrames;
    QHash<QString, FileCardRow> m_cards; // token -> card row
};
