#include "ui/PeerListDelegate.h"

#include <QDateTime>
#include <QPainter>

namespace {

// Rows store the peer display data in custom data roles (see MainWindow).
enum PeerRoles {
    PeerIdRole = Qt::UserRole,
    PeerIpRole = Qt::UserRole + 1,
    PeerNameRole = Qt::UserRole + 2,
    PeerSubtitleRole = Qt::UserRole + 3,
    PeerTimeRole = Qt::UserRole + 4,
    PeerUnreadRole = Qt::UserRole + 5,
};

// A deterministic, evenly-spread palette for avatar backgrounds.
QColor avatarColor(const QString &name) {
    static const QColor kPalette[] = {
        QColor(0xe1, 0x62, 0x5b), QColor(0xf5, 0x9e, 0x42), QColor(0xf7, 0xc7, 0x48),
        QColor(0x57, 0xa4, 0x6c), QColor(0x36, 0x8f, 0xd9), QColor(0x6b, 0x6b, 0xbd),
        QColor(0x9c, 0x6b, 0xc4), QColor(0xcf, 0x59, 0x8b),
    };
    uint h = qHash(name);
    return kPalette[h % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

} // namespace

PeerListDelegate::PeerListDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize PeerListDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return QSize(200, 64);
}

void PeerListDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const QRect row = opt.rect;
    const QString name = idx.data(PeerNameRole).toString();
    const QString ip = idx.data(PeerIpRole).toString();
    const QString subtitle = idx.data(PeerSubtitleRole).toString();
    const QString time = idx.data(PeerTimeRole).toString();
    const int unread = idx.data(PeerUnreadRole).toInt();

    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const QColor bg = selected ? QColor(0xe3, 0xee, 0xfb)
                               : (hover ? QColor(0xf5, 0xf6, 0xf8) : Qt::white);
    p->fillRect(row, bg);

    const int cy = row.center().y();

    // Round avatar with the first character of the name.
    const qreal r = 18;
    const QPointF c(row.left() + 24 + r, cy);
    p->setPen(Qt::NoPen);
    p->setBrush(avatarColor(name.isEmpty() ? ip : name));
    p->drawEllipse(c, r, r);
    p->setPen(Qt::white);
    QFont f = opt.font;
    f.setBold(true);
    f.setPixelSize(16);
    p->setFont(f);
    p->drawText(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), Qt::AlignCenter,
                name.left(1).toUpper());

    const int textLeft = row.left() + 24 + 2 * r + 14;
    const int textRight = row.right() - 12;

    // Right column: timestamp on top, unread badge below it.
    QFont metaFont = opt.font;
    metaFont.setPixelSize(12);
    p->setFont(metaFont);
    const QFontMetrics fm(metaFont);
    if (!time.isEmpty()) {
        p->setPen(QColor(0x8d, 0x96, 0x9e));
        p->drawText(QRect(textRight - fm.horizontalAdvance(time), row.top() + 10,
                          fm.horizontalAdvance(time), fm.height()),
                    Qt::AlignRight | Qt::AlignTop, time);
    }
    if (unread > 0) {
        const QString badge = QString::number(unread);
        const qreal br = 11;
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0x33, 0x90, 0xec));
        const QPointF bc(textRight - br, cy + 14);
        p->drawEllipse(bc, br, br);
        p->setPen(Qt::white);
        p->drawText(QRectF(bc.x() - br, bc.y() - br, 2 * br, 2 * br), Qt::AlignCenter, badge);
    }

    // Name (bold) and subtitle (preview / ip · os).
    QFont nameFont = opt.font;
    nameFont.setBold(true);
    nameFont.setPixelSize(14);
    p->setFont(nameFont);
    const QFontMetrics nameFm(nameFont);
    const int nameWidth = textRight - textLeft - (unread > 0 ? 34 : 0);
    p->setPen(QColor(0x0f, 0x0f, 0x0f));
    p->drawText(QRect(textLeft, row.top() + 12, nameWidth, nameFm.height()),
                Qt::AlignLeft | Qt::AlignTop,
                nameFm.elidedText(name, Qt::ElideRight, nameWidth));

    QFont subFont = opt.font;
    subFont.setPixelSize(13);
    p->setFont(subFont);
    const QFontMetrics subFm(subFont);
    const int subWidth = textRight - textLeft;
    const bool accent = unread > 0;
    p->setPen(accent ? QColor(0x33, 0x90, 0xec) : QColor(0x8d, 0x96, 0x9e));
    QFont accentFont = subFont;
    accentFont.setBold(accent);
    p->setFont(accentFont);
    p->drawText(QRect(textLeft, row.top() + 32, subWidth, subFm.height()),
                Qt::AlignLeft | Qt::AlignTop,
                subFm.elidedText(subtitle, Qt::ElideRight, subWidth));

    p->restore();
}
