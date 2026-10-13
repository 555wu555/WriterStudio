#ifndef WRITE_TABCLOSEBUTTON_H
#define WRITE_TABCLOSEBUTTON_H

#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>

// 直接绘制叉号，避免小尺寸 PNG 在系统缩放时出现不均匀的斜线。
class TabCloseButton : public QAbstractButton {
public:
    explicit TabCloseButton(QWidget *parent = nullptr) : QAbstractButton(parent) {
        setFixedSize(20, 20);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(QStringLiteral("关闭作品"));
        setAccessibleName(QStringLiteral("关闭作品"));
    }

    QSize sizeHint() const override { return QSize(20, 20); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool hovered = underMouse() && isEnabled();
        if (hovered || isDown()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(isDown() ? QColor("#303958") : QColor("#3d4655"));
            painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
        }

        const QColor color = !isEnabled() ? QColor("#626c7b")
            : isDown() ? QColor("#c2cbff")
            : hovered ? QColor("#edf0f4") : QColor("#99a3b2");
        painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        const QPointF center(width() / 2.0, height() / 2.0);
        constexpr qreal half = 3.5;
        QPainterPath cross;
        cross.moveTo(center + QPointF(-half, -half));
        cross.lineTo(center + QPointF(half, half));
        cross.moveTo(center + QPointF(half, -half));
        cross.lineTo(center + QPointF(-half, half));
        painter.drawPath(cross);
    }

    void enterEvent(QEnterEvent *event) override {
        QAbstractButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override {
        QAbstractButton::leaveEvent(event);
        update();
    }
};

#endif // WRITE_TABCLOSEBUTTON_H
