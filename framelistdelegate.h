#ifndef FRAMELISTDELEGATE_H
#define FRAMELISTDELEGATE_H
#include <qabstractitemdelegate.h>


class FrameListDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    FrameListDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;


};

#endif // FRAMELISTDELEGATE_H
