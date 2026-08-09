#include "ui/TransferWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

struct RowWidgets {
    QLabel *label = nullptr;
    QProgressBar *bar = nullptr;
    QPushButton *cancel = nullptr;
};

TransferItem itemFromRow(QListWidgetItem *it) {
    const QVariant v = it->data(Qt::UserRole);
    if (v.canConvert<TransferItem>())
        return v.value<TransferItem>();
    return TransferItem{};
}

} // namespace

Q_DECLARE_METATYPE(TransferItem)

TransferWidget::TransferWidget(QWidget *parent)
    : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TransferWidget::addItem(const TransferItem &item) {
    auto *it = new QListWidgetItem(this);
    it->setData(Qt::UserRole, QVariant::fromValue(item));
    auto *w = new QWidget(this);
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(6, 3, 6, 3);
    auto *label = new QLabel(w);
    auto *bar = new QProgressBar(w);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(true);
    auto *cancel = new QPushButton(QStringLiteral("取消"), w);
    cancel->setMaximumWidth(60);
    lay->addWidget(label, 1);
    lay->addWidget(bar, 1);
    lay->addWidget(cancel);
    setItemWidget(it, w);

    const QString arrow = item.direction == TransferDirection::Send ? QStringLiteral("→") : QStringLiteral("←");
    label->setText(QStringLiteral("%1 %2 %3").arg(item.peerName, arrow, item.name));
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
    QList<QLabel *> labels = w->findChildren<QLabel *>();
    if (bars.isEmpty() || labels.isEmpty())
        return;
    const int pct = ti.total > 0 ? static_cast<int>(ti.done * 1000 / ti.total) : 0;
    bars.first()->setValue(pct);
    const QString arrow = ti.direction == TransferDirection::Send ? QStringLiteral("→") : QStringLiteral("←");
    labels.first()->setText(QStringLiteral("%1 %2 %3 (%4%)")
                                .arg(ti.peerName, arrow, ti.name)
                                .arg(pct / 10));
}

void TransferWidget::updateProgress(const QString &token, qint64 done, qint64 total) {
    QListWidgetItem *it = itemFor(token);
    if (!it)
        return;
    TransferItem ti = itemFromRow(it);
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
    QList<QLabel *> labels = w->findChildren<QLabel *>();
    QList<QPushButton *> buttons = w->findChildren<QPushButton *>();
    if (!labels.isEmpty())
        labels.first()->setText(QStringLiteral("%1").arg(text));
    if (!bars.isEmpty())
        bars.first()->setValue(finished ? 1000 : bars.first()->value());
    if (!buttons.isEmpty())
        buttons.first()->setEnabled(!finished);
}
