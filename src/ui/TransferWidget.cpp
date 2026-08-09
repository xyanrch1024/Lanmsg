#include "ui/TransferWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

struct RowWidgets {
    QLabel *label = nullptr;
    QLabel *detail = nullptr;
    QProgressBar *bar = nullptr;
    QPushButton *cancel = nullptr;
};

TransferItem itemFromRow(QListWidgetItem *it) {
    const QVariant v = it->data(Qt::UserRole);
    if (v.canConvert<TransferItem>())
        return v.value<TransferItem>();
    return TransferItem{};
}

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

QString formatEta(qint64 remainingBytes, double bytesPerSec) {
    if (remainingBytes <= 0)
        return QStringLiteral("--");
    if (bytesPerSec <= 0.0)
        return QStringLiteral("--");
    const qint64 secs = static_cast<qint64>(remainingBytes / bytesPerSec);
    if (secs >= 3600)
        return QStringLiteral("%1:%2:%3")
            .arg(secs / 3600)
            .arg((secs % 3600) / 60, 2, 10, QLatin1Char('0'))
            .arg(secs % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2")
        .arg(secs / 60, 2, 10, QLatin1Char('0'))
        .arg(secs % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

Q_DECLARE_METATYPE(TransferItem)

TransferWidget::TransferWidget(QWidget *parent)
    : QListWidget(parent)
    , m_refreshTimer(new QTimer(this)) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Keep ETA ticking and reflect stalls even between chunk callbacks.
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &TransferWidget::refreshActiveRows);
    m_refreshTimer->start();
}

void TransferWidget::addItem(const TransferItem &item) {
    auto *it = new QListWidgetItem(this);
    it->setData(Qt::UserRole, QVariant::fromValue(item));
    auto *w = new QWidget(this);
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(6, 3, 6, 3);

    auto *texts = new QVBoxLayout;
    texts->setSpacing(0);
    auto *label = new QLabel(w);
    label->setObjectName(QStringLiteral("main"));
    auto *detail = new QLabel(w);
    detail->setObjectName(QStringLiteral("detail"));
    detail->setStyleSheet(QStringLiteral("color:gray; font-size:11px;"));
    texts->addWidget(label);
    texts->addWidget(detail);

    auto *bar = new QProgressBar(w);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(true);
    auto *cancel = new QPushButton(QStringLiteral("取消"), w);
    cancel->setMaximumWidth(60);

    lay->addLayout(texts, 2);
    lay->addWidget(bar, 3);
    lay->addWidget(cancel);
    setItemWidget(it, w);

    const QString arrow = item.direction == TransferDirection::Send ? QStringLiteral("→") : QStringLiteral("←");
    label->setText(QStringLiteral("%1 %2 %3").arg(item.peerName, arrow, item.name));
    detail->setText(QStringLiteral("0 B"));
    cancel->setEnabled(item.done < item.total || item.total == 0);

    connect(cancel, &QPushButton::clicked, this, [this, token = item.token] { emit cancelRequested(token); });
}

QListWidgetItem *TransferWidget::itemFor(const QString &token) const {
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *it = item(i);
        if (itemFromRow(it).token == token)
            return it;
    }
    return nullptr;
}

void TransferWidget::refreshRow(QListWidgetItem *it) {
    TransferItem ti = itemFromRow(it);
    if (!(it->flags() & Qt::ItemIsEnabled))
        return;
    QWidget *w = itemWidget(it);
    if (!w)
        return;
    QList<QProgressBar *> bars = w->findChildren<QProgressBar *>();
    QLabel *mainLabel = w->findChild<QLabel *>(QStringLiteral("main"));
    QLabel *detail = w->findChild<QLabel *>(QStringLiteral("detail"));
    if (bars.isEmpty() || !mainLabel)
        return;
    const int pct = ti.total > 0 ? static_cast<int>(ti.done * 1000 / ti.total) : 0;
    bars.first()->setValue(pct);
    const QString arrow = ti.direction == TransferDirection::Send ? QStringLiteral("→") : QStringLiteral("←");
    mainLabel->setText(QStringLiteral("%1 %2 %3 (%4%)")
                           .arg(ti.peerName, arrow, ti.name)
                           .arg(pct / 10));
    if (detail) {
        const qint64 remaining = ti.total - ti.done;
        detail->setText(QStringLiteral("%1 / %2 · %3 · 剩余 %4")
                            .arg(formatBytes(ti.done), formatBytes(ti.total),
                                 formatSpeed(ti.speedBps),
                                 formatEta(remaining, ti.speedBps)));
    }
}

void TransferWidget::refreshActiveRows() {
    for (int i = 0; i < count(); ++i)
        refreshRow(item(i));
}

void TransferWidget::updateProgress(const QString &token, qint64 done, qint64 total) {
    QListWidgetItem *it = itemFor(token);
    if (!it)
        return;
    TransferItem ti = itemFromRow(it);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (ti.lastSampleMs > 0) {
        const qint64 dt = now - ti.lastSampleMs;
        const qint64 db = done - ti.lastSampleDone;
        if (dt > 0 && db > 0) {
            const double inst = static_cast<double>(db) * 1000.0 / dt; // bytes/s
            ti.speedBps = (ti.speedBps <= 0.0) ? inst : (ti.speedBps * 0.7 + inst * 0.3);
        }
    }
    ti.lastSampleMs = now;
    ti.lastSampleDone = done;

    ti.done = done;
    ti.total = total;
    it->setData(Qt::UserRole, QVariant::fromValue(ti));
    refreshRow(it);
}

void TransferWidget::setStatus(const QString &token, bool finished, const QString &text) {
    QListWidgetItem *it = itemFor(token);
    if (!it)
        return;
    if (finished)
        it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
    QWidget *w = itemWidget(it);
    if (!w)
        return;
    QList<QProgressBar *> bars = w->findChildren<QProgressBar *>();
    QLabel *mainLabel = w->findChild<QLabel *>(QStringLiteral("main"));
    QLabel *detail = w->findChild<QLabel *>(QStringLiteral("detail"));
    QList<QPushButton *> buttons = w->findChildren<QPushButton *>();
    if (mainLabel)
        mainLabel->setText(QStringLiteral("%1").arg(text));
    if (detail)
        detail->setText(finished ? QString() : detail->text());
    if (!bars.isEmpty())
        bars.first()->setValue(finished ? 1000 : bars.first()->value());
    if (!buttons.isEmpty())
        buttons.first()->setEnabled(!finished);
}
