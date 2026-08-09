#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
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

private:
    QLabel *m_title = nullptr;
    QTextEdit *m_log = nullptr;
    QLineEdit *m_input = nullptr;
};
