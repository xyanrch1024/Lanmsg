#include "ui/ChatWidget.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_title(new QLabel(this))
    , m_log(new QTextEdit(this))
    , m_input(new QTextEdit(this))
    , m_sendButton(new QPushButton(QStringLiteral("发送"), this)) {
    m_title->setStyleSheet("font-weight:bold; padding:4px;");
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(QStringLiteral("还没有消息"));
    m_input->setPlaceholderText(QStringLiteral("输入消息，回车发送，Shift+回车换行"));
    m_input->setFixedHeight(96);
    m_sendButton->setFixedWidth(64);
    m_sendButton->setFixedHeight(32);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_title);
    layout->addWidget(m_log, 1);

    auto *inputRow = new QHBoxLayout;
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendButton, 0, Qt::AlignBottom);
    layout->addLayout(inputRow);

    m_input->installEventFilter(this);
    connect(m_sendButton, &QPushButton::clicked, this, [this] { doSend(); });
    connect(m_input, &QTextEdit::textChanged, this, [this] {
        m_sendButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty());
    });
    m_sendButton->setEnabled(false);
}

bool ChatWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers() & Qt::ShiftModifier) {
                m_input->insertPlainText(QStringLiteral("\n"));
                return true;
            }
            doSend();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChatWidget::doSend() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;
    emit sendRequested(text);
    m_input->clear();
    m_sendButton->setEnabled(false);
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
