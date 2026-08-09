#include "ui/ChatWidget.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_title(new QLabel(this))
    , m_log(new QTextEdit(this))
    , m_input(new QLineEdit(this)) {
    m_title->setStyleSheet("font-weight:bold; padding:4px;");
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(QStringLiteral("还没有消息"));
    m_input->setPlaceholderText(QStringLiteral("输入消息，回车发送"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_title);
    layout->addWidget(m_log, 1);
    layout->addWidget(m_input);

    connect(m_input, &QLineEdit::returnPressed, this, [this] {
        const QString text = m_input->text().trimmed();
        if (text.isEmpty())
            return;
        emit sendRequested(text);
        m_input->clear();
    });
}

void ChatWidget::setPeerName(const QString &name) {
    m_title->setText(name.isEmpty() ? QStringLiteral("聊天") : name);
}

void ChatWidget::appendMessage(const QString &who, const QString &text, qint64 ts, bool isSelf) {
    const QString time = QDateTime::fromMSecsSinceEpoch(ts).toString("HH:mm:ss");
    const QString prefix = isSelf ? QStringLiteral("我") : who;
    m_log->append(QStringLiteral("<b>[%1] %2:</b><br>%3").arg(time, prefix.toHtmlEscaped(), text.toHtmlEscaped()));
}

void ChatWidget::clearLog() {
    m_log->clear();
}
