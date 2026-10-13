#ifndef WRITE_PROJECTLISTDELEGATE_H
#define WRITE_PROJECTLISTDELEGATE_H

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyle>

// 作品卡片只显示两行：加粗名称，以及较淡的章节数和字数。
class ProjectListDelegate final : public QStyledItemDelegate {
public:
    enum ItemDataRole {
        SummaryRole = Qt::UserRole + 1
    };

    explicit ProjectListDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override {
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        QFont titleFont = itemOption.font;
        titleFont.setPixelSize(14);
        titleFont.setBold(true);
        QFont summaryFont = itemOption.font;
        summaryFont.setPixelSize(12);
        summaryFont.setBold(false);
        const int textHeight = QFontMetrics(titleFont).height()
            + QFontMetrics(summaryFont).height() + 4;
        return QSize(0, qMax(64, textHeight + 20));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        const QString title = itemOption.text;
        const QString summary = index.data(SummaryRole).toString();
        const QIcon icon = itemOption.icon;
        const bool selected = itemOption.state & QStyle::State_Selected;

        painter->save();
        painter->setClipRect(itemOption.rect, Qt::IntersectClip);

        // 选中、悬停和圆角背景仍由现有 QSS 绘制。
        itemOption.text.clear();
        itemOption.icon = QIcon();
        itemOption.features &= ~(QStyleOptionViewItem::HasDisplay
                                 | QStyleOptionViewItem::HasDecoration);
        QStyle *style = itemOption.widget
            ? itemOption.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter,
                           itemOption.widget);

        const QRect card = itemOption.rect;
        const QRect iconRect(card.left() + 12, card.center().y() - 12, 24, 24);
        icon.paint(painter, iconRect, Qt::AlignCenter,
                   selected ? QIcon::Selected : QIcon::Normal);

        QFont titleFont = itemOption.font;
        titleFont.setPixelSize(14);
        titleFont.setBold(true);
        QFont summaryFont = itemOption.font;
        summaryFont.setPixelSize(12);
        summaryFont.setBold(false);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics summaryMetrics(summaryFont);
        const int textHeight = titleMetrics.height() + 4 + summaryMetrics.height();
        const int textTop = card.top() + (card.height() - textHeight) / 2;
        const int textLeft = card.left() + 48;
        const int textWidth = qMax(0, card.width() - 60);

        painter->setFont(titleFont);
        painter->setPen(selected ? QColor("#eef1ff") : QColor("#edf0f4"));
        painter->drawText(QRect(textLeft, textTop, textWidth, titleMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(title, Qt::ElideRight, textWidth));

        painter->setFont(summaryFont);
        painter->setPen(selected ? QColor("#b4bfdd") : QColor("#929dab"));
        painter->drawText(QRect(textLeft, textTop + titleMetrics.height() + 4,
                                textWidth, summaryMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          summaryMetrics.elidedText(summary, Qt::ElideRight, textWidth));
        painter->restore();
    }
};

#endif // WRITE_PROJECTLISTDELEGATE_H
