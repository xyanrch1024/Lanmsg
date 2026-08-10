#pragma once

#include <QStyledItemDelegate>

// Telegram-style chat-list rows for the device list: a colored round avatar
// with the first letter of the name, a bold name with a message preview below,
// and a timestamp + unread badge on the right.
class PeerListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PeerListDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};
