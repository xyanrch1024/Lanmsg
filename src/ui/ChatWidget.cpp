#include "ui/ChatWidget.h"

#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const QColor kAccent(0x33, 0x90, 0xec);

QString formatBytes(qint64 bytes) {
    if (bytes >= 1024LL * 1024 * 1024)
        return QStringLiteral("%1 GB").arg(bytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}

QString formatSpeed(double bytesPerSec) {
    if (bytesPerSec >= 1024.0 * 1024)
        return QStringLiteral("%1 MB/s").arg(bytesPerSec / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytesPerSec >= 1024.0)
        return QStringLiteral("%1 KB/s").arg(bytesPerSec / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B/s").arg(bytesPerSec, 0, 'f', 0);
}

QPixmap makeAvatar(const QString &name, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x57, 0xa4, 0x6c));
    p.drawEllipse(0, 0, size, size);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(size * 2 / 3);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, name.left(1).toUpper());
    p.end();
    return pm;
}

QIcon makeAttachIcon() {
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x70, 0x79, 0x80), 2.4, Qt::SolidLine, Qt::RoundCap));
    const int c = 16;
    p.drawLine(c - 6, c, c + 6, c);
    p.drawLine(c, c - 6, c, c + 6);
    p.end();
    return QIcon(pm);
}

QIcon makeSendIcon() {
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(kAccent);
    p.drawEllipse(0, 0, 32, 32);
    QPolygon plane;
    plane << QPoint(21, 6) << QPoint(26, 26) << QPoint(17, 19) << QPoint(11, 24) << QPoint(10, 17);
    p.setBrush(Qt::white);
    p.drawPolygon(plane);
    p.end();
    return QIcon(pm);
}

} // namespace

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent) {
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("chatHeader"));
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(12, 8, 12, 8);
    headerLay->setSpacing(10);
    m_avatar = new QLabel(header);
    m_avatar->setFixedSize(36, 36);
    m_avatar->setPixmap(makeAvatar(QStringLiteral("?"), 36));
    headerLay->addWidget(m_avatar);
    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(0);
    m_title = new QLabel(QStringLiteral("聊天"), header);
    m_title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:bold; color:#0f0f0f; background:transparent;"));
    m_subtitle = new QLabel(header);
    m_subtitle->setStyleSheet(QStringLiteral("font-size:12px; color:#8d969e; background:transparent;"));
    titleCol->addWidget(m_title);
    titleCol->addWidget(m_subtitle);
    headerLay->addLayout(titleCol, 1);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("chatScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bubblesHost = new QWidget;
    m_bubblesHost->setObjectName(QStringLiteral("chatBubbles"));
    m_bubbles = new QVBoxLayout(m_bubblesHost);
    m_bubbles->setContentsMargins(0, 4, 0, 4);
    m_bubbles->setSpacing(2);
    m_bubbles->addStretch(1);
    m_scroll->setWidget(m_bubblesHost);

    auto *compose = new QWidget(this);
    auto *composeLay = new QHBoxLayout(compose);
    composeLay->setContentsMargins(8, 6, 8, 8);
    composeLay->setSpacing(6);

    m_attachButton = new QPushButton(compose);
    m_attachButton->setObjectName(QStringLiteral("attachButton"));
    m_attachButton->setIcon(makeAttachIcon());
    m_attachButton->setIconSize(QSize(32, 32));
    m_attachButton->setFixedSize(36, 36);
    m_attachButton->setToolTip(QStringLiteral("发送文件"));
    m_attachButton->setCursor(Qt::PointingHandCursor);
    composeLay->addWidget(m_attachButton, 0, Qt::AlignBottom);

    m_input = new QTextEdit(compose);
    m_input->setObjectName(QStringLiteral("chatInput"));
    m_input->setPlaceholderText(QStringLiteral("输入消息，回车发送，Shift+回车换行"));
    m_input->setFixedHeight(90);
    composeLay->addWidget(m_input, 1);

    m_sendButton = new QPushButton(compose);
    m_sendButton->setObjectName(QStringLiteral("sendButton"));
    m_sendButton->setIcon(makeSendIcon());
    m_sendButton->setIconSize(QSize(32, 32));
    m_sendButton->setFixedSize(36, 36);
    m_sendButton->setToolTip(QStringLiteral("发送"));
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setEnabled(false);
    composeLay->addWidget(m_sendButton, 0, Qt::AlignBottom);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(m_scroll, 1);
    layout->addWidget(compose);

    m_input->installEventFilter(this);
    connect(m_attachButton, &QPushButton::clicked, this, [this] { emit attachRequested(); });
    connect(m_sendButton, &QPushButton::clicked, this, [this] { doSend(); });
    connect(m_input, &QTextEdit::textChanged, this, [this] {
        m_sendButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty());
    });

    setEnabled(false);
}

void ChatWidget::setPeerInfo(const QString &name, const QString &subtitle) {
    m_title->setText(name.isEmpty() ? QStringLiteral("聊天") : name);
    m_subtitle->setText(subtitle);
    m_avatar->setPixmap(makeAvatar(name.isEmpty() ? QStringLiteral("?") : name, 36));
}

void ChatWidget::setPeerName(const QString &name) {
    setPeerInfo(name, QString());
}

void ChatWidget::doSend() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;
    emit sendRequested(text);
    m_input->clear();
    m_sendButton->setEnabled(false);
}

void ChatWidget::insertRow(QWidget *row) {
    m_bubbles->insertWidget(m_bubbles->count() - 1, row);
}

void ChatWidget::scrollToBottom() {
    QTimer::singleShot(0, this, [this] {
        QScrollBar *sb = m_scroll->verticalScrollBar();
        sb->setValue(sb->maximum());
    });
}

void ChatWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_bubbleMaxWidth = qMax(220, width() * 72 / 100);
    for (QFrame *b : m_bubblesFrames)
        b->setMaximumWidth(m_bubbleMaxWidth);
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
    if (event->type() == QEvent::MouseButtonRelease) {
        for (auto it = m_cards.begin(); it != m_cards.end(); ++it) {
            if (it.value().bubble == watched) {
                if (it.value().finished && it.value().ok)
                    emit openFileRequested(it.key());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChatWidget::applyBubbleColors(QFrame *bubble, bool isSend) {
    if (isSend) {
        bubble->setStyleSheet(
            QStringLiteral("QFrame#bubble { background:%1; border-radius:14px; }")
                .arg(kAccent.name()));
    } else {
        bubble->setStyleSheet(
            QStringLiteral("QFrame#bubble { background:#ffffff; border:1px solid #e4e9ef; border-radius:14px; }"));
    }
}

void ChatWidget::appendMessage(const QString &who, const QString &text, qint64 ts, bool isSelf) {
    const QDate date = QDateTime::fromMSecsSinceEpoch(ts).date();
    if (date != m_lastDate) {
        m_lastDate = date;
        auto *sep = new QWidget(m_bubblesHost);
        auto *sepLay = new QHBoxLayout(sep);
        sepLay->setContentsMargins(0, 6, 0, 6);
        auto *sepLabel = new QLabel(date.toString(QStringLiteral("yyyy年M月d日")), sep);
        sepLabel->setStyleSheet(QStringLiteral(
            "color:#8d969e; font-size:12px; padding:3px 12px; border-radius:10px;"
            "background:rgba(0,0,0,0.05);"));
        sepLay->addStretch();
        sepLay->addWidget(sepLabel);
        sepLay->addStretch();
        insertRow(sep);
    }

    auto *row = new QWidget(m_bubblesHost);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(8, 2, 8, 2);
    rowLay->setSpacing(8);

    auto *bubble = new QFrame(row);
    bubble->setObjectName(QStringLiteral("bubble"));
    bubble->setMaximumWidth(m_bubbleMaxWidth);
    auto *bubbleLay = new QVBoxLayout(bubble);
    bubbleLay->setContentsMargins(10, 6, 12, 5);
    bubbleLay->setSpacing(2);

    if (!isSelf && !who.isEmpty()) {
        auto *whoLabel = new QLabel(who, bubble);
        whoLabel->setStyleSheet(QStringLiteral(
            "color:%1; font-size:13px; font-weight:bold; background:transparent;")
                                    .arg(kAccent.name()));
        bubbleLay->addWidget(whoLabel);
    }

    auto *msg = new QLabel(text, bubble);
    msg->setWordWrap(true);
    msg->setTextInteractionFlags(Qt::TextSelectableByMouse);
    msg->setStyleSheet(QStringLiteral("color:%1; font-size:14px; background:transparent;")
                           .arg(isSelf ? QStringLiteral("white") : QStringLiteral("#0f0f0f")));
    bubbleLay->addWidget(msg);

    auto *timeLabel = new QLabel(QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("HH:mm")), bubble);
    timeLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px; background:transparent;")
                                 .arg(isSelf ? QStringLiteral("rgba(255,255,255,0.75)")
                                             : QStringLiteral("#8d969e")));
    timeLabel->setAlignment(Qt::AlignRight);
    bubbleLay->addWidget(timeLabel);

    applyBubbleColors(bubble, isSelf);

    if (isSelf) {
        rowLay->addStretch(1);
        rowLay->addWidget(bubble, 0, Qt::AlignVCenter);
    } else {
        rowLay->addWidget(bubble, 0, Qt::AlignVCenter);
        rowLay->addStretch(1);
    }

    m_bubblesFrames.append(bubble);
    insertRow(row);
    scrollToBottom();
}

void ChatWidget::clearLog() {
    // Remove all rows but keep the trailing stretch that pins content to the top.
    while (m_bubbles->count() > 1) {
        QLayoutItem *item = m_bubbles->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_bubblesFrames.clear();
    m_cards.clear();
    m_lastDate = QDate();
}

void ChatWidget::addFileCard(const QString &token, bool isSend, const QString &name, qint64 total,
                             const QString &path) {
    Q_UNUSED(path);
    if (m_cards.contains(token))
        return;

    auto *row = new QWidget(m_bubblesHost);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(8, 2, 8, 2);
    rowLay->setSpacing(8);

    auto *bubble = new QFrame(row);
    bubble->setObjectName(QStringLiteral("bubble"));
    bubble->setMaximumWidth(m_bubbleMaxWidth);
    auto *bl = new QVBoxLayout(bubble);
    bl->setContentsMargins(12, 8, 12, 8);
    bl->setSpacing(4);

    auto *top = new QHBoxLayout;
    top->setSpacing(4);
    auto *nameLabel = new QLabel(name, bubble);
    nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    top->addWidget(nameLabel, 1);

    auto *cancel = new QPushButton(QStringLiteral("✕"), bubble);
    cancel->setFixedSize(20, 20);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setToolTip(QStringLiteral("取消传输"));
    cancel->setStyleSheet(QStringLiteral(
        "border:none; border-radius:10px; background:transparent; color:#8d969e;"
        "font-weight:bold; font-size:13px;"));
    cancel->setVisible(false);
    top->addWidget(cancel, 0, Qt::AlignTop);
    connect(cancel, &QPushButton::clicked, this, [this, token] { emit cancelFileRequested(token); });
    bl->addLayout(top);

    auto *detail = new QLabel(QStringLiteral("0% · 共 %1").arg(formatBytes(total)), bubble);
    bl->addWidget(detail);

    auto *bar = new QProgressBar(bubble);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(6);
    bl->addWidget(bar);

    if (isSend) {
        nameLabel->setStyleSheet(QStringLiteral("color:white; font-size:14px; font-weight:bold; background:transparent;"));
        detail->setStyleSheet(QStringLiteral("color:rgba(255,255,255,0.8); font-size:12px; background:transparent;"));
        bubble->setStyleSheet(QStringLiteral("QFrame#bubble { background:%1; border-radius:14px; }").arg(kAccent.name()));
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { border:none; border-radius:3px; background:rgba(255,255,255,0.3); }"
            "QProgressBar::chunk { border-radius:3px; background:white; }"));
    } else {
        nameLabel->setStyleSheet(QStringLiteral("color:#0f0f0f; font-size:14px; font-weight:bold; background:transparent;"));
        detail->setStyleSheet(QStringLiteral("color:#8d969e; font-size:12px; background:transparent;"));
        bubble->setStyleSheet(QStringLiteral("QFrame#bubble { background:#ffffff; border:1px solid #e4e9ef; border-radius:14px; }"));
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { border:none; border-radius:3px; background:rgba(51,144,236,0.15); }"
            "QProgressBar::chunk { border-radius:3px; background:#3390ec; }"));
    }

    if (isSend) {
        rowLay->addStretch(1);
        rowLay->addWidget(bubble, 0, Qt::AlignVCenter);
    } else {
        rowLay->addWidget(bubble, 0, Qt::AlignVCenter);
        rowLay->addStretch(1);
    }

    FileCardRow card;
    card.bubble = bubble;
    card.name = nameLabel;
    card.detail = detail;
    card.bar = bar;
    card.cancel = cancel;
    m_cards.insert(token, card);

    // Pause-style card: the cancel button only matters while the transfer runs.
    cancel->setVisible(true);
    bubble->installEventFilter(this);
    m_bubblesFrames.append(bubble);
    insertRow(row);
    scrollToBottom();
}

void ChatWidget::updateFileCard(const QString &token, qint64 done, qint64 total) {
    auto it = m_cards.find(token);
    if (it == m_cards.end())
        return;
    FileCardRow &c = it.value();
    if (c.finished)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (c.lastMs > 0 && done > c.lastDone) {
        const qint64 dt = now - c.lastMs;
        if (dt > 0) {
            const double inst = static_cast<double>(done - c.lastDone) * 1000.0 / dt;
            c.speedBps = c.speedBps <= 0.0 ? inst : c.speedBps * 0.7 + inst * 0.3;
        }
    }
    c.lastMs = now;
    c.lastDone = done;

    const int pct = total > 0 ? qMin<qint64>(1000, done * 1000 / total) : 0;
    c.bar->setValue(pct);
    c.detail->setText(QStringLiteral("%1% · %2 / %3 · %4")
                          .arg(pct / 10)
                          .arg(formatBytes(done), formatBytes(total), formatSpeed(c.speedBps)));
}

void ChatWidget::setFileCardStatus(const QString &token, bool ok, const QString &text) {
    auto it = m_cards.find(token);
    if (it == m_cards.end())
        return;
    FileCardRow &c = it.value();
    c.finished = true;
    c.ok = ok;
    c.cancel->setVisible(false);
    if (ok) {
        c.bar->setValue(1000);
        c.detail->setText(text);
        c.bubble->setCursor(Qt::PointingHandCursor);
        c.bubble->setToolTip(QStringLiteral("点击打开文件"));
    } else {
        c.bar->setVisible(false);
        c.detail->setText(text);
    }
}

void ChatWidget::removeFinishedCards() {
    const QStringList tokens = m_cards.keys();
    for (const QString &token : tokens) {
        const FileCardRow &c = m_cards.value(token);
        if (!c.finished)
            continue;
        m_bubblesFrames.removeOne(c.bubble);
        m_bubbles->removeWidget(c.bubble);
        c.bubble->deleteLater();
        m_cards.remove(token);
    }
}

