#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTextEdit;

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void setPeerName(const QString &name);
    void appendMessage(const QString &who, const QString &text, qint64 ts, bool isSelf);

    void clearLog();

signals:
    void sendRequested(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void doSend();

    QLabel *m_title = nullptr;
    QTextEdit *m_log = nullptr;
    QTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
};
