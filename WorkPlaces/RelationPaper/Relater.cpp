#include "Relater.h"
#include "CreateRelationshipDialog.h"
#include "../../models/Project.h"
#include "../../storage/EntityStore/EntityStore.h"
#include "../../storage/ProjectStore.h"
#include "../../storage/StoragePaths.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QAbstractItemView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleOptionGraphicsItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QBrush>
#include <QPen>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {
// 先错落铺开，再用弹簧与排斥调整初始位置。已有节点固定，只安置新资料。
QHash<QString, QPointF> initialGraphPositions(
    const WorldData &data, const QHash<QString, QPointF> &previous) {
    const int count = static_cast<int>(data.entities.size());
    if (count == 0) return {};

    QHash<QString, int> indices;
    QVector<int> degrees(count, 0);
    for (int i = 0; i < count; ++i) indices.insert(data.entities.at(i).id, i);
    QVector<QPair<int, int>> links;
    QSet<QPair<int, int>> uniqueLinks;
    for (const Relationship &relation : data.relationships) {
        const int from = indices.value(relation.fromId, -1);
        const int to = indices.value(relation.toId, -1);
        if (from < 0 || to < 0 || from == to) continue;
        const auto pair = qMakePair(std::min(from, to), std::max(from, to));
        if (uniqueLinks.contains(pair)) continue;
        uniqueLinks.insert(pair);
        links.append(pair);
        ++degrees[from];
        ++degrees[to];
    }

    QVector<int> order;
    for (int i = 0; i < count; ++i) order.append(i);
    std::stable_sort(order.begin(), order.end(), [&degrees](int a, int b) {
        return degrees.at(a) > degrees.at(b);
    });
    QVector<QPointF> positions(count), seeds(count), velocities(count);
    QVector<bool> fixed(count, false);
    QPointF center;
    int fixedCount = 0;
    for (int i = 0; i < count; ++i) {
        const auto found = previous.constFind(data.entities.at(i).id);
        if (found == previous.cend()) continue;
        positions[i] = found.value();
        fixed[i] = true;
        center += found.value();
        ++fixedCount;
    }
    if (fixedCount > 0) center /= fixedCount;

    // 黄金角螺旋没有行列对齐；关系最多的节点优先放在中间。
    constexpr qreal goldenAngle = 2.399963229728653;
    for (int rank = 0; rank < count; ++rank) {
        const int i = order.at(rank);
        if (fixed.at(i)) continue;
        QPointF anchor = center;
        int neighbors = 0;
        QPointF neighborCenter;
        for (const auto &link : links) {
            const int other = link.first == i ? link.second
                : link.second == i ? link.first : -1;
            if (other >= 0 && fixed.at(other)) {
                neighborCenter += positions.at(other);
                ++neighbors;
            }
        }
        if (neighbors > 0) anchor = neighborCenter / neighbors;
        const qreal angle = rank * goldenAngle + 0.45;
        const qreal radius = fixedCount > 0
            ? 175.0 + 28.0 * std::sqrt(rank + 1.0)
            : 115.0 * std::sqrt(static_cast<qreal>(rank));
        positions[i] = anchor + QPointF(std::cos(angle), std::sin(angle)) * radius;
    }
    seeds = positions;

    // 初始计算在显示前完成，避免打开页面时先看到规则网格。
    const int iterations = std::clamp(18000 / count, 40, 180);
    for (int step = 0; step < iterations && fixedCount < count; ++step) {
        QVector<QPointF> forces(count);
        for (const auto &link : links) {
            const QPointF delta = positions.at(link.second) - positions.at(link.first);
            const qreal distance = std::hypot(delta.x(), delta.y());
            if (distance < 0.001) continue;
            const QPointF pull = delta / distance *
                std::clamp((distance - 175.0) * 0.045, -12.0, 12.0);
            forces[link.first] += pull;
            forces[link.second] -= pull;
        }
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                if (fixed.at(i) && fixed.at(j)) continue;
                const QPointF delta = positions.at(j) - positions.at(i);
                const qreal distance = std::hypot(delta.x(), delta.y());
                const qreal angle = (i + j + 1) * goldenAngle;
                const QPointF direction = distance < 0.001
                    ? QPointF(std::cos(angle), std::sin(angle)) : delta / distance;
                const qreal strength = std::min(10.0, 9000.0 /
                    std::max(900.0, distance * distance)) +
                    5.0 * std::max(0.0, 1.0 - distance / 190.0);
                QPointF push = direction * strength;
                // 名称在圆点下方，横向接近时也要给文字留出空间。
                if (std::abs(delta.x()) < 170.0 && std::abs(delta.y()) < 60.0) {
                    const qreal sign = delta.y() < 0 ? -1.0 : 1.0;
                    push.ry() += sign * 3.0 * (1.0 - std::abs(delta.x()) / 170.0) *
                        (1.0 - std::abs(delta.y()) / 60.0);
                }
                forces[i] -= push;
                forces[j] += push;
            }
        }
        for (int i = 0; i < count; ++i) {
            if (fixed.at(i)) continue;
            // 轻微保留初始方向，链式关系也能保持舒展的弧度。
            forces[i] += (seeds.at(i) - positions.at(i)) * 0.008;
            forces[i] += (center - positions.at(i)) * 0.003;
            QPointF velocity = (velocities.at(i) + forces.at(i)) * 0.78;
            const qreal speed = std::hypot(velocity.x(), velocity.y());
            const qreal maxSpeed = 10.0 * (1.0 - qreal(step) / iterations) + 0.5;
            if (speed > maxSpeed) velocity *= maxSpeed / speed;
            velocities[i] = velocity;
            positions[i] += velocity;
        }
    }
    QHash<QString, QPointF> result;
    for (int i = 0; i < count; ++i) result.insert(data.entities.at(i).id, positions.at(i));
    return result;
}

class GraphNodeItem final : public QGraphicsEllipseItem {
public:
    GraphNodeItem(std::function<void()> onMoved,
                  std::function<void(bool)> onDragChanged,
                  std::function<void()> onDoubleClicked)
        : QGraphicsEllipseItem(0, 0, 12, 12),
          onMoved_(std::move(onMoved)),
          onDragChanged_(std::move(onDragChanged)),
          onDoubleClicked_(std::move(onDoubleClicked)) {}

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override {
        QStyleOptionGraphicsItem plainOption(*option);
        plainOption.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
        QGraphicsEllipseItem::paint(painter, &plainOption, widget);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            QGraphicsEllipseItem::mouseDoubleClickEvent(event);
            return;
        }
        if (scene()) scene()->clearSelection();
        setSelected(true);
        event->accept();
        if (onDoubleClicked_) onDoubleClicked_();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        QGraphicsEllipseItem::mousePressEvent(event);
        if (event->button() == Qt::LeftButton && event->isAccepted() &&
            onDragChanged_) onDragChanged_(true);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
        QGraphicsEllipseItem::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && onDragChanged_)
            onDragChanged_(false);
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        const QVariant result = QGraphicsEllipseItem::itemChange(change, value);
        if (change == ItemPositionHasChanged && onMoved_) onMoved_();
        return result;
    }

private:
    std::function<void()> onMoved_;
    std::function<void(bool)> onDragChanged_;
    std::function<void()> onDoubleClicked_;
};

class GraphView final : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;

    void requestInitialFraming() {
        initialFramingPending_ = true;
        if (isVisible()) scheduleInitialFraming();
    }

    void showWholeGraph() {
        initialFramingPending_ = false;
        if (!scene() || scene()->items().isEmpty()) return;
        frameScene(0.0);
    }

    void focusOnNode(const QPointF &position) {
        initialFramingPending_ = false;
        const qreal currentScale = transform().m11();
        const qreal halfWidth = viewport()->width() / (2 * currentScale) + 40;
        const qreal halfHeight = viewport()->height() / (2 * currentScale) + 40;
        scene()->setSceneRect(scene()->sceneRect().united(QRectF(
            position.x() - halfWidth, position.y() - halfHeight,
            2 * halfWidth, 2 * halfHeight)));
        centerOn(position);
    }

protected:
    void showEvent(QShowEvent *event) override {
        QGraphicsView::showEvent(event);
        if (initialFramingPending_) scheduleInitialFraming();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton &&
            !hasSelectableItemAt(event->position().toPoint())) {
            scene()->clearSelection();
            panning_ = true;
            lastMousePos_ = event->position().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        const QPoint currentPos = event->position().toPoint();
        if (panning_) {
            const QPoint delta = currentPos - lastMousePos_;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            lastMousePos_ = currentPos;
            event->accept();
            return;
        }
        setCursor(hasSelectableItemAt(currentPos)
                      ? Qt::PointingHandCursor : Qt::OpenHandCursor);
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (panning_ && event->button() == Qt::LeftButton) {
            panning_ = false;
            setCursor(hasSelectableItemAt(event->position().toPoint())
                          ? Qt::PointingHandCursor : Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override {
        const int delta = event->angleDelta().y() != 0
                              ? event->angleDelta().y()
                              : event->pixelDelta().y() * 2;
        if (delta == 0) {
            event->ignore();
            return;
        }
        const qreal currentScale = transform().m11();
        const qreal nextScale = std::clamp(
            currentScale * std::pow(1.0015, delta), 0.35, 3.5);
        const qreal factor = nextScale / currentScale;
        scale(factor, factor);
        event->accept();
    }

private:
    void frameScene(qreal minimumScale) {
        const QRectF bounds = scene()->itemsBoundingRect().adjusted(-40, -40, 40, 40);
        scene()->setSceneRect(scene()->sceneRect().united(bounds));
        const qreal fittedScale = std::min(
            viewport()->width() / bounds.width(),
            viewport()->height() / bounds.height());
        if (!std::isfinite(fittedScale) || fittedScale <= 0) return;
        const qreal scaleFactor = std::clamp(fittedScale, minimumScale, 1.25);
        resetTransform();
        scale(scaleFactor, scaleFactor);
        centerOn(bounds.center());
    }

    void scheduleInitialFraming() {
        // 页面常在后台先加载作品，必须等画布显示并完成布局后再读取尺寸。
        QTimer::singleShot(0, this, [this] {
            if (!initialFramingPending_ || !isVisible() || !scene()) return;
            frameScene(0.85);
            initialFramingPending_ = false;
        });
    }

    bool hasSelectableItemAt(const QPoint &position) const {
        for (QGraphicsItem *item = itemAt(position); item;
             item = item->parentItem()) {
            if (item->flags() & QGraphicsItem::ItemIsSelectable) return true;
        }
        return false;
    }

    bool panning_ = false;
    bool initialFramingPending_ = false;
    QPoint lastMousePos_;
};

QString kindText(const QString &kind) {
    if (kind == "person") return "人物";
    if (kind == "place") return "地点";
    if (kind == "organization") return "组织";
    if (kind == "item") return "物件";
    return "资料";
}

constexpr int relationshipLabelRole = Qt::UserRole + 1;

class RelationshipItemDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return QSize(0, 68);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        const bool selected = option.state & QStyle::State_Selected;
        const QRect card = option.rect.adjusted(3, 3, -3, -3);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(QColor(selected ? "#6379ad" : "#363f4d"), 1));
        painter->setBrush(QColor(selected ? "#2e3b59" : "#1b2027"));
        painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5),
                                 8, 8);

        const QRect titleRect = card.adjusted(14, 7, -14, -31);
        QFont titleFont = option.font;
        titleFont.setBold(true);
        titleFont.setPointSize(12);
        painter->setFont(titleFont);
        painter->setPen(QColor(selected ? "#f0f4ff" : "#e4eaf3"));
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(titleFont).elidedText(
                              index.data(Qt::DisplayRole).toString(),
                              Qt::ElideRight, titleRect.width()));

        const QRect relationRect = card.adjusted(14, 31, -14, -8);
        QFont relationFont = option.font;
        relationFont.setPointSize(10);
        painter->setFont(relationFont);
        painter->setPen(QColor(selected ? "#c1cef0" : "#9daabd"));
        const QString relationText = QString("关系 · %1")
            .arg(index.data(relationshipLabelRole).toString());
        painter->drawText(relationRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(relationFont).elidedText(
                              relationText, Qt::ElideRight, relationRect.width()));
        painter->restore();
    }
};
}
Relater::Relater(QWidget *parent) : QWidget(parent) {
    setObjectName("graphPage");
    setAttribute(Qt::WA_StyledBackground, true);

    motionTimer_ = new QTimer(this);
    motionTimer_->setInterval(16);
    connect(motionTimer_, &QTimer::timeout,
            this, &Relater::advanceGraphMotion);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(0);

    pageStack_ = new QStackedWidget(this);
    pageStack_->setObjectName("graphPageStack");
    root->addWidget(pageStack_);

    auto *statePage = new QWidget(pageStack_);
    auto *stateLayout = new QVBoxLayout(statePage);
    stateLayout->setContentsMargins(24, 24, 24, 24);
    stateLabel_ = new QLabel("请先在作品库打开一部作品", statePage);
    stateLabel_->setObjectName("graphStateLabel");
    stateLabel_->setAlignment(Qt::AlignCenter);
    stateLabel_->setWordWrap(true);
    stateLayout->addWidget(stateLabel_);
    pageStack_->addWidget(statePage);

    auto *projectPage = new QWidget(pageStack_);
    projectPage->setObjectName("graphProjectPage");
    auto *projectLayout = new QVBoxLayout(projectPage);
    projectLayout->setContentsMargins(0, 0, 0, 0);
    projectLayout->setSpacing(18);

    auto *header = new QHBoxLayout;
    header->setSpacing(16);
    auto *heading = new QVBoxLayout;
    heading->setSpacing(5);
    auto *title = new QLabel("人物关系图谱", projectPage);
    title->setObjectName("graphTitle");
    projectLabel_ = new QLabel(projectPage);
    projectLabel_->setObjectName("graphSubtitle");
    heading->addWidget(title);
    heading->addWidget(projectLabel_);
    header->addLayout(heading, 1);
    addRelationshipButton_ = new QPushButton("＋ 新建关系", projectPage);
    addRelationshipButton_->setObjectName("graphAddRelationship");
    addRelationshipButton_->setEnabled(false);
    addRelationshipButton_->setToolTip("至少需要两条资料才能创建关系");
    header->addWidget(addRelationshipButton_, 0, Qt::AlignTop);
    projectLayout->addLayout(header);

    auto *body = new QSplitter(Qt::Horizontal, projectPage);
    body->setObjectName("graphSplitter");
    body->setChildrenCollapsible(false);
    body->setHandleWidth(12);
    projectLayout->addWidget(body, 1);

    auto *canvasPane = new QFrame(body);
    canvasPane->setObjectName("graphPane");
    canvasPane->setMinimumWidth(320);
    auto *canvasLayout = new QVBoxLayout(canvasPane);
    canvasLayout->setContentsMargins(18, 16, 18, 18);
    canvasLayout->setSpacing(14);
    auto *canvasHeader = new QHBoxLayout;
    auto *canvasTitle = new QLabel("关系画布", canvasPane);
    canvasTitle->setObjectName("graphPaneTitle");
    countLabel_ = new QLabel("0 条资料 · 0 条关系", canvasPane);
    countLabel_->setObjectName("graphCount");
    canvasHeader->addWidget(canvasTitle);
    canvasHeader->addStretch();
    showAllButton_ = new QPushButton("显示全图", canvasPane);
    showAllButton_->setObjectName("graphShowAllButton");
    showAllButton_->setEnabled(false);
    showAllButton_->setToolTip("将所有资料节点放进画布视野");
    canvasHeader->addWidget(showAllButton_);
    canvasHeader->addWidget(countLabel_);
    canvasLayout->addLayout(canvasHeader);

    auto *searchPanel = new QWidget(canvasPane);
    searchPanel->setObjectName("graphSearchPanel");
    searchPanel->setMinimumHeight(0);
    searchPanel->setMaximumHeight(108);
    auto *searchPanelLayout = new QVBoxLayout(searchPanel);
    searchPanelLayout->setContentsMargins(0, 0, 0, 0);
    searchPanelLayout->setSpacing(10);

    auto *searchBar = new QHBoxLayout;
    searchBar->setSpacing(10);
    searchEdit_ = new QLineEdit(canvasPane);
    searchEdit_->setObjectName("graphSearchEdit");
    searchEdit_->setPlaceholderText("搜索人物或资料名称…");
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setEnabled(false);
    searchEdit_->setToolTip("输入名字的一部分，选择匹配资料后定位；回车可定位当前结果");
    searchStatus_ = new QLabel(canvasPane);
    searchStatus_->setObjectName("graphSearchStatus");
    searchStatus_->hide();
    searchBar->addWidget(searchEdit_, 1);
    searchBar->addWidget(searchStatus_);
    searchPanelLayout->addLayout(searchBar);

    searchResultsRow_ = new QWidget(canvasPane);
    auto *resultsLayout = new QHBoxLayout(searchResultsRow_);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    resultsLayout->setSpacing(10);
    searchResults_ = new QComboBox(searchResultsRow_);
    searchResults_->setObjectName("graphSearchResults");
    searchResults_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    searchResults_->setMinimumContentsLength(12);
    searchResults_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    searchResults_->setPlaceholderText("没有匹配的资料");
    searchResults_->setToolTip("选择一条资料，画布将移到对应圆点");
    locateButton_ = new QPushButton("定位", searchResultsRow_);
    locateButton_->setObjectName("graphLocateButton");
    locateButton_->setEnabled(false);
    resultsLayout->addWidget(searchResults_, 1);
    resultsLayout->addWidget(locateButton_);
    searchPanelLayout->addWidget(searchResultsRow_);
    searchResultsRow_->hide();

    auto *searchSplitter = new QSplitter(Qt::Vertical, canvasPane);
    searchSplitter->setObjectName("graphSearchSplitter");
    searchSplitter->setChildrenCollapsible(false);
    searchSplitter->setHandleWidth(10);
    searchSplitter->addWidget(searchPanel);

    scene_ = new QGraphicsScene(this);
    scene_->setSceneRect(-320, -190, 640, 380);
    view_ = new GraphView(scene_, canvasPane);
    view_->setObjectName("graphView");
    view_->setFrameShape(QFrame::NoFrame);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setAlignment(Qt::AlignCenter);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setMouseTracking(true);
    view_->setCursor(Qt::OpenHandCursor);
    view_->setToolTip("拖动圆点可牵动相连节点；拖动空白处可平移画布，滚轮缩放");
    searchSplitter->addWidget(view_);
    searchSplitter->setCollapsible(0, true);
    searchSplitter->setStretchFactor(0, 0);
    searchSplitter->setStretchFactor(1, 1);
    searchSplitter->setSizes({56, 700});
    canvasLayout->addWidget(searchSplitter, 1);
    body->addWidget(canvasPane);

    auto *inspectorPane = new QFrame(body);
    inspectorPane->setObjectName("graphPane");
    inspectorPane->setMinimumWidth(220);
    inspectorPane->setMaximumWidth(360);
    auto *inspectorLayout = new QVBoxLayout(inspectorPane);
    inspectorLayout->setContentsMargins(20, 18, 20, 20);
    inspectorLayout->setSpacing(16);
    inspectorTitle_ = new QLabel("资料概况", inspectorPane);
    inspectorTitle_->setObjectName("graphPaneTitle");
    inspectorTitle_->setWordWrap(true);
    inspectorCountLabel_ = new QLabel(inspectorPane);
    inspectorCountLabel_->setObjectName("graphInspectorCount");
    inspectorCountLabel_->setWordWrap(true);
    inspectorHint_ = new QLabel("选择一个圆点，查看与这条资料有关的关系。",
                                inspectorPane);
    inspectorHint_->setObjectName("graphHint");
    inspectorHint_->setWordWrap(true);
    relationshipList_ = new QListWidget(inspectorPane);
    relationshipList_->setObjectName("graphRelationshipList");
    relationshipList_->setWordWrap(true);
    relationshipList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    relationshipList_->setSelectionMode(QAbstractItemView::SingleSelection);
    relationshipList_->setItemDelegate(
        new RelationshipItemDelegate(relationshipList_));
    relationshipList_->setVisible(false);
    editRelationshipButton_ = new QPushButton("编辑关系", inspectorPane);
    editRelationshipButton_->setObjectName("graphEditRelationship");
    editRelationshipButton_->setEnabled(false);
    editRelationshipButton_->setVisible(false);
    deleteRelationshipButton_ = new QPushButton("删除关系", inspectorPane);
    deleteRelationshipButton_->setObjectName("graphDeleteRelationship");
    deleteRelationshipButton_->setEnabled(false);
    deleteRelationshipButton_->setVisible(false);
    inspectorLayout->addWidget(inspectorTitle_);
    inspectorLayout->addWidget(inspectorCountLabel_);
    inspectorLayout->addWidget(inspectorHint_);
    inspectorLayout->addWidget(relationshipList_, 1);
    auto *relationActions = new QHBoxLayout;
    relationActions->addStretch();
    relationActions->addWidget(editRelationshipButton_);
    relationActions->addWidget(deleteRelationshipButton_);
    inspectorLayout->addLayout(relationActions);
    inspectorLayout->addStretch();
    body->addWidget(inspectorPane);
    body->setStretchFactor(0, 1);
    body->setStretchFactor(1, 0);
    body->setSizes({760, 270});

    pageStack_->addWidget(projectPage);
    pageStack_->setCurrentIndex(0);
    connect(addRelationshipButton_, &QPushButton::clicked,
            this, &Relater::createRelationship);
    connect(showAllButton_, &QPushButton::clicked, this, [this] {
        static_cast<GraphView *>(view_)->showWholeGraph();
    });
    connect(relationshipList_, &QListWidget::currentItemChanged,
            editRelationshipButton_,
            [button = editRelationshipButton_](QListWidgetItem *current) {
        button->setEnabled(current &&
                           !current->data(Qt::UserRole).toString().isEmpty());
    });
    connect(relationshipList_, &QListWidget::currentItemChanged,
            deleteRelationshipButton_,
            [button = deleteRelationshipButton_](QListWidgetItem *current) {
        button->setEnabled(current &&
                           !current->data(Qt::UserRole).toString().isEmpty());
    });
    connect(deleteRelationshipButton_, &QPushButton::clicked,
            this, &Relater::deleteSelectedRelationship);
    connect(editRelationshipButton_, &QPushButton::clicked,
            this, &Relater::editSelectedRelationship);
    connect(scene_, &QGraphicsScene::selectionChanged,
            this, &Relater::onSelectionChanged);
    connect(searchEdit_, &QLineEdit::textChanged, this, &Relater::updateSearchResults);
    connect(searchEdit_, &QLineEdit::returnPressed, this, &Relater::locateSearchResult);
    connect(searchResults_, &QComboBox::activated, this, &Relater::locateSearchResult);
    connect(locateButton_, &QPushButton::clicked, this, &Relater::locateSearchResult);
    applyStyle();
}

Relater::~Relater() {
    motionTimer_->stop();
    // QWidget 销毁子控件时，QGraphicsScene 仍可能发出 selectionChanged。
    // 此时 Relater 的派生类析构已经结束，不能再调用它的成员函数。
    disconnect(scene_, &QGraphicsScene::selectionChanged,
               this, &Relater::onSelectionChanged);
}

void Relater::setProjectId(const QString &projectsPath, const QString &id) {
    motionTimer_->stop();
    draggedNodeId_.clear();
    if (id.isEmpty()) {
        projectId_.clear();
        projectDir_.clear();
        selectedEntityId_.clear();
        worldData_ = WorldData{};
        searchEdit_->clear();
        updateSearchResults();
        nodes_.clear();
        edges_.clear();
        nodeVelocities_.clear();
        const QSignalBlocker blocker(scene_);
        scene_->clear();
        addRelationshipButton_->setEnabled(false);
        showAllButton_->setEnabled(false);
        stateLabel_->setText("请先在作品库打开一部作品");
        pageStack_->setCurrentIndex(0);
        return;
    }

    const QString projectDir = StoragePaths::projectDirectory(projectsPath, id);
    Project project;
    QString error;
    if (!ProjectStore::loadProject(StoragePaths::projectFilePath(projectDir),
                                   project, error) || project.id != id) {
        stateLabel_->setText(QString("读取作品失败：%1")
                                 .arg(error.isEmpty() ? "作品 ID 与目录不一致" : error));
        pageStack_->setCurrentIndex(0);
        return;
    }

    WorldData loaded;
    if (!EntityStore::load(projectDir, loaded, error)) {
        stateLabel_->setText(QString("读取图谱资料失败：%1").arg(error));
        pageStack_->setCurrentIndex(0);
        return;
    }

    const bool resetLayout = projectId_ != id;
    if (resetLayout) {
        selectedEntityId_.clear();
        const QSignalBlocker blocker(searchEdit_);
        searchEdit_->clear();
    }
    projectId_ = id;
    projectDir_ = projectDir;
    worldData_ = loaded;
    projectLabel_->setText(QString("《%1》").arg(project.name));
    updateCounts();
    renderGraph(resetLayout);
    pageStack_->setCurrentIndex(1);
}

void Relater::renderGraph(bool resetLayout) {
    motionTimer_->stop();
    draggedNodeId_.clear();
    nodeVelocities_.clear();
    settledFrames_ = 0;
    QHash<QString, QPointF> previousPositions;
    if (!resetLayout) {
        for (auto it = nodes_.cbegin(); it != nodes_.cend(); ++it)
            previousPositions.insert(it.key(), it.value()->pos());
    }
    const auto positions = initialGraphPositions(worldData_, previousPositions);
    const bool frameInitialLayout = previousPositions.isEmpty();
    const QSignalBlocker blocker(scene_);
    nodes_.clear();
    edges_.clear();
    scene_->clear();
    if (worldData_.entities.isEmpty()) {
        scene_->setSceneRect(-1800, -1400, 3600, 2800);
        auto *text = scene_->addText(
            "还没有人物或世界资料\n先到“作品设定”中创建资料");
        QFont font;
        font.setPointSize(12);
        text->setFont(font);
        text->setDefaultTextColor(QColor("#9da7b5"));
        text->setPos(-text->boundingRect().width() / 2,
                     -text->boundingRect().height() / 2);
        selectedEntityId_.clear();
        updateInspector();
        updateSearchResults();
        return;
    }

    const QFont labelFont("Microsoft YaHei UI", 10);
    const QFontMetrics labelMetrics(labelFont);
    for (int index = 0; index < worldData_.entities.size(); ++index) {
        const Entity &entity = worldData_.entities.at(index);
        auto *node = new GraphNodeItem(
            [this, id = entity.id] {
                if (!updatingMotion_) updateEdgePositions();
                if (draggedNodeId_ == id) {
                    settledFrames_ = 0;
                    if (!motionTimer_->isActive()) motionTimer_->start();
                }
            },
            [this, id = entity.id](bool dragging) {
                if (dragging) {
                    draggedNodeId_ = id;
                    nodeVelocities_.insert(id, QPointF{});
                } else if (draggedNodeId_ == id) {
                    draggedNodeId_.clear();
                    settledFrames_ = 0;
                }
            },
            [this, id = entity.id] {
                emit entityOpenRequested(id);
            });
        node->setPen(QPen(QColor("#a4b2d4")));
        node->setBrush(QBrush(QColor("#8798be")));
        scene_->addItem(node);
        node->setData(0, entity.id);
        node->setFlag(QGraphicsItem::ItemIsSelectable);
        node->setFlag(QGraphicsItem::ItemIsMovable);
        node->setFlag(QGraphicsItem::ItemSendsGeometryChanges);
        auto *name = new QGraphicsTextItem(node);
        name->setFont(labelFont);
        name->setPlainText(labelMetrics.elidedText(entity.name,
                                                   Qt::ElideRight, 150));
        name->setDefaultTextColor(QColor("#dce4f0"));
        name->setPos(6 - name->boundingRect().width() / 2, 18);
        name->setAcceptedMouseButtons(Qt::NoButton);
        name->setToolTip(entity.name);
        node->setPos(positions.value(entity.id));
        nodes_.insert(entity.id, node);
        nodeVelocities_.insert(entity.id, QPointF{});
    }

    for (const Relationship &relation : worldData_.relationships) {
        auto *from = nodes_.value(relation.fromId, nullptr);
        auto *to = nodes_.value(relation.toId, nullptr);
        if (!from || !to) continue;
        const QPointF start = from->scenePos() + QPointF(6, 6);
        const QPointF end = to->scenePos() + QPointF(6, 6);
        auto *line = scene_->addLine(QLineF(start, end),
                                     QPen(QColor("#69748b"), 1.6));
        line->setZValue(-1);
        const QString tooltip = relation.reverseLabel.isEmpty()
            ? relation.label
            : QString("%1 / %2").arg(relation.label, relation.reverseLabel);
        line->setToolTip(tooltip);
        edges_.append({line, relation.fromId, relation.toId});
    }

    updateEdgePositions();

    const QRectF graphBounds = scene_->itemsBoundingRect();
    scene_->setSceneRect(graphBounds.adjusted(-1800, -1400, 1800, 1400));
    if (frameInitialLayout) {
        static_cast<GraphView *>(view_)->requestInitialFraming();
    }
    if (auto *selectedNode = nodes_.value(selectedEntityId_, nullptr)) {
        selectedNode->setSelected(true);
    } else {
        selectedEntityId_.clear();
    }
    updateSelection();
    updateSearchResults();
}

void Relater::updateSearchResults() {
    const bool hasEntities = !projectId_.isEmpty() && !worldData_.entities.isEmpty();
    searchEdit_->setEnabled(hasEntities);
    const QString query = searchEdit_->text().trimmed();
    const QString previousId = searchResults_->currentData().toString();
    const QSignalBlocker blocker(searchResults_);
    searchResults_->clear();
    if (hasEntities && !query.isEmpty()) {
        for (const Entity &entity : worldData_.entities) {
            const QString name = entity.name.trimmed().isEmpty()
                ? QStringLiteral("未命名资料") : entity.name;
            if (!name.contains(query, Qt::CaseInsensitive)) continue;
            QString text = QString("%1 · %2").arg(name, kindText(entity.kind));
            if (!entity.role.trimmed().isEmpty()) text += " · " + entity.role.trimmed();
            searchResults_->addItem(text, entity.id);
            const QString tooltip = entity.summary.trimmed().isEmpty()
                ? text : text + "\n" + entity.summary.trimmed();
            searchResults_->setItemData(searchResults_->count() - 1, tooltip, Qt::ToolTipRole);
        }
    }
    int selectedIndex = searchResults_->findData(previousId);
    if (selectedIndex < 0) selectedIndex = searchResults_->findData(selectedEntityId_);
    if (selectedIndex < 0 && searchResults_->count() > 0) selectedIndex = 0;
    searchResults_->setCurrentIndex(selectedIndex);
    const bool searching = hasEntities && !query.isEmpty();
    const bool hasMatches = searchResults_->count() > 0;
    searchStatus_->setText(hasMatches
        ? QString("%1 条匹配").arg(searchResults_->count()) : QStringLiteral("无匹配"));
    searchStatus_->setVisible(searching);
    searchResultsRow_->setVisible(searching);
    searchResults_->setEnabled(hasMatches);
    locateButton_->setEnabled(hasMatches);
}

void Relater::locateSearchResult() {
    const QString id = searchResults_->currentData().toString();
    if (!locateButton_->isEnabled()) return;
    locateEntityById(id);
}

bool Relater::locateEntityById(const QString &entityId) {
    if (entityId.isEmpty() || projectId_.isEmpty()) return false;
    auto *node = nodes_.value(entityId, nullptr);
    if (!node) return false;
    {
        const QSignalBlocker blocker(scene_);
        scene_->clearSelection();
        node->setSelected(true);
    }
    selectedEntityId_ = entityId;
    updateSelection();
    static_cast<GraphView *>(view_)->focusOnNode(node->mapToScene(node->rect().center()));
    return true;
}

void Relater::updateEdgePositions() {
    for (const EdgeItem &edge : edges_) {
        auto *from = nodes_.value(edge.fromId, nullptr);
        auto *to = nodes_.value(edge.toId, nullptr);
        if (!from || !to || !edge.line) continue;
        edge.line->setLine(QLineF(from->mapToScene(from->rect().center()),
                                  to->mapToScene(to->rect().center())));
    }
}

void Relater::advanceGraphMotion() {
    if (nodes_.size() < 2) {
        motionTimer_->stop();
        return;
    }

    QHash<QString, QPointF> forces;
    QVector<QString> ids;
    ids.reserve(worldData_.entities.size());
    for (const Entity &entity : worldData_.entities) {
        if (!nodes_.contains(entity.id)) continue;
        ids.append(entity.id);
        forces.insert(entity.id, QPointF{});
    }

    // 连线像弹簧，距离过长时拉近两端，距离过短时推开。
    constexpr qreal preferredEdgeLength = 165.0;
    constexpr qreal springStrength = 0.025;
    for (const EdgeItem &edge : edges_) {
        auto *from = nodes_.value(edge.fromId, nullptr);
        auto *to = nodes_.value(edge.toId, nullptr);
        if (!from || !to) continue;
        const QPointF delta = to->pos() - from->pos();
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance < 0.001) continue;
        const qreal strength = std::clamp(
            (distance - preferredEdgeLength) * springStrength, -6.0, 6.0);
        const QPointF pull = delta * (strength / distance);
        forces[edge.fromId] += pull;
        forces[edge.toId] -= pull;
    }

    // 近距离排斥，避免节点和名称叠在一起。
    constexpr qreal repelRadius = 160.0;
    constexpr qreal repelStrength = 1.4;
    for (int i = 0; i < ids.size(); ++i) {
        for (int j = i + 1; j < ids.size(); ++j) {
            const QPointF delta = nodes_.value(ids.at(j))->pos() -
                                  nodes_.value(ids.at(i))->pos();
            const qreal distance = std::hypot(delta.x(), delta.y());
            if (distance >= repelRadius) continue;
            const QPointF direction = distance < 0.001
                ? QPointF(1, 0) : delta / distance;
            const QPointF push = direction *
                (repelStrength * (repelRadius - distance) / repelRadius);
            forces[ids.at(i)] -= push;
            forces[ids.at(j)] += push;
        }
    }

    // 鼠标按住的节点由 Qt 直接拖动，其余节点由速度和阻尼驱动。
    constexpr qreal damping = 0.84;
    constexpr qreal maxSpeed = 14.0;
    qreal maxMovement = 0.0;
    updatingMotion_ = true;
    for (const QString &id : ids) {
        if (id == draggedNodeId_) {
            nodeVelocities_[id] = QPointF{};
            continue;
        }
        QPointF velocity = (nodeVelocities_.value(id) + forces.value(id)) *
                           damping;
        const qreal speed = std::hypot(velocity.x(), velocity.y());
        if (speed > maxSpeed) velocity *= maxSpeed / speed;
        nodeVelocities_[id] = velocity;
        maxMovement = std::max(maxMovement,
                               std::hypot(velocity.x(), velocity.y()));
        nodes_.value(id)->setPos(nodes_.value(id)->pos() + velocity);
    }
    updatingMotion_ = false;
    updateEdgePositions();

    if (draggedNodeId_.isEmpty() && maxMovement < 0.15) {
        if (++settledFrames_ >= 18) {
            motionTimer_->stop();
            for (const QString &id : ids) nodeVelocities_[id] = QPointF{};
        }
    } else {
        settledFrames_ = 0;
    }
}

void Relater::createRelationship() {
    if (projectDir_.isEmpty() || worldData_.entities.size() < 2) return;

    CreateRelationshipDialog dialog(worldData_.entities, this);
    while (dialog.exec() == QDialog::Accepted) {
        if (dialog.fromId() == dialog.toId() || dialog.label().isEmpty()) return;

        Relationship relation;
        relation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        relation.fromId = dialog.fromId();
        relation.toId = dialog.toId();
        relation.label = dialog.label();
        relation.reverseLabel = dialog.reverseLabel();
        relation.notes = dialog.notes();

        WorldData candidate;
        QString error;
        if (!EntityStore::load(projectDir_, candidate, error)) {
            QMessageBox::warning(this, "创建关系失败", QString("读取最新资料失败：%1").arg(error));
            continue;
        }
        const auto hasEndpoint = [&candidate](const QString &id) {
            for (const Entity &entity : candidate.entities)
                if (entity.id == id) return true;
            return false;
        };
        if (!hasEndpoint(relation.fromId) || !hasEndpoint(relation.toId)) {
            QMessageBox::warning(this, "创建关系失败", "关系两端的资料已变化，请重新打开关系图谱。");
            return;
        }
        candidate.relationships.append(relation);
        if (!EntityStore::save(projectDir_, candidate, error)) {
            QMessageBox::warning(this, "创建关系失败", error);
            continue;
        }

        worldData_ = candidate;
        selectedEntityId_ = relation.fromId;
        updateCounts();
        renderGraph();
        if (auto *node = nodes_.value(selectedEntityId_, nullptr))
            view_->centerOn(node);
        return;
    }
}

void Relater::editSelectedRelationship() {
    const QListWidgetItem *selectedItem = relationshipList_->currentItem();
    if (!selectedItem || projectDir_.isEmpty()) return;

    const QString relationId = selectedItem->data(Qt::UserRole).toString();
    const Relationship *original = nullptr;
    for (const Relationship &relation : worldData_.relationships) {
        if (relation.id == relationId) {
            original = &relation;
            break;
        }
    }
    if (!original) return;

    CreateRelationshipDialog dialog(worldData_.entities, this);
    if (!dialog.setRelationshipForEditing(*original)) {
        QMessageBox::warning(this, "编辑关系失败", "关系连接的资料不存在");
        return;
    }

    while (dialog.exec() == QDialog::Accepted) {
        if (dialog.label().isEmpty()) return;

        WorldData candidate;
        QString error;
        if (!EntityStore::load(projectDir_, candidate, error)) {
            QMessageBox::warning(this, "保存关系失败", QString("读取最新资料失败：%1").arg(error));
            continue;
        }
        bool found = false;
        for (Relationship &relation : candidate.relationships) {
            if (relation.id != relationId) continue;
            relation.label = dialog.label();
            relation.reverseLabel = dialog.reverseLabel();
            relation.notes = dialog.notes();
            found = true;
            break;
        }
        if (!found) {
            QMessageBox::warning(this, "保存关系失败", "这条关系已不存在。");
            return;
        }

        if (!EntityStore::save(projectDir_, candidate, error)) {
            QMessageBox::warning(this, "保存关系失败", error);
            continue;
        }

        worldData_ = candidate;
        renderGraph();
        for (int row = 0; row < relationshipList_->count(); ++row) {
            if (relationshipList_->item(row)->data(Qt::UserRole).toString() ==
                relationId) {
                relationshipList_->setCurrentRow(row);
                break;
            }
        }
        return;
    }
}

void Relater::deleteSelectedRelationship() {
    const QListWidgetItem *selectedItem = relationshipList_->currentItem();
    if (!selectedItem || projectDir_.isEmpty()) return;

    const QString relationId = selectedItem->data(Qt::UserRole).toString();
    if (relationId.isEmpty()) return;

    const Relationship *selectedRelation = nullptr;
    for (const Relationship &relation : worldData_.relationships) {
        if (relation.id == relationId) {
            selectedRelation = &relation;
            break;
        }
    }
    if (!selectedRelation) return;

    const auto entityName = [this](const QString &id) {
        for (const Entity &entity : worldData_.entities) {
            if (entity.id == id)
                return entity.name.trimmed().isEmpty()
                    ? QStringLiteral("未命名资料") : entity.name;
        }
        return QStringLiteral("未知资料");
    };

    QMessageBox confirm(this);
    confirm.setIcon(QMessageBox::Warning);
    confirm.setWindowTitle("删除关系");
    confirm.setText("确定删除这条关系吗？");
    QString relationDescription = QString("%1 → %2：%3")
        .arg(entityName(selectedRelation->fromId),
             entityName(selectedRelation->toId), selectedRelation->label);
    if (!selectedRelation->reverseLabel.isEmpty()) {
        relationDescription += QString("\n%1 → %2：%3")
            .arg(entityName(selectedRelation->toId),
                 entityName(selectedRelation->fromId),
                 selectedRelation->reverseLabel);
    }
    confirm.setInformativeText(relationDescription +
                               "\n\n删除后无法撤销，资料本身不会被删除。");
    auto *deleteButton = confirm.addButton("删除关系", QMessageBox::DestructiveRole);
    confirm.addButton("取消", QMessageBox::RejectRole);
    confirm.exec();
    if (confirm.clickedButton() != deleteButton) return;

    WorldData candidate;
    QString error;
    if (!EntityStore::load(projectDir_, candidate, error)) {
        QMessageBox::warning(this, "删除关系失败", QString("读取最新资料失败：%1").arg(error));
        return;
    }
    bool found = false;
    for (int index = 0; index < candidate.relationships.size(); ++index) {
        if (candidate.relationships.at(index).id == relationId) {
            candidate.relationships.removeAt(index);
            found = true;
            break;
        }
    }
    if (!found) {
        QMessageBox::warning(this, "删除关系失败", "这条关系已不存在。");
        return;
    }

    if (!EntityStore::save(projectDir_, candidate, error)) {
        QMessageBox::warning(this, "删除关系失败", error);
        return;
    }

    worldData_ = candidate;
    updateCounts();
    renderGraph();
}

void Relater::updateCounts() {
    countLabel_->setText(QString("%1 条资料 · %2 条关系")
                             .arg(worldData_.entities.size())
                             .arg(worldData_.relationships.size()));
    const bool canAdd = !projectDir_.isEmpty() && worldData_.entities.size() >= 2;
    showAllButton_->setEnabled(!projectDir_.isEmpty() && !worldData_.entities.isEmpty());
    addRelationshipButton_->setEnabled(canAdd);
    addRelationshipButton_->setToolTip(canAdd ? "连接两条资料"
                                             : "至少需要两条资料才能创建关系");
}

void Relater::onSelectionChanged() {
    selectedEntityId_.clear();
    for (QGraphicsItem *item : scene_->selectedItems()) {
        const QString id = item->data(0).toString();
        if (id.isEmpty()) continue;
        selectedEntityId_ = id;
        break;
    }
    updateSelection();
}

void Relater::updateSelection() {
    const bool hasSelection = !selectedEntityId_.isEmpty();
    QSet<QString> neighbors;
    if (hasSelection) {
        for (const Relationship &relation : worldData_.relationships) {
            if (relation.fromId == selectedEntityId_)
                neighbors.insert(relation.toId);
            if (relation.toId == selectedEntityId_)
                neighbors.insert(relation.fromId);
        }
    }

    for (auto it = nodes_.cbegin(); it != nodes_.cend(); ++it) {
        const bool center = it.key() == selectedEntityId_;
        const bool related = neighbors.contains(it.key());
        it.value()->setOpacity(!hasSelection || center || related ? 1.0 : 0.22);
        it.value()->setPen(QPen(QColor(center ? "#e1e9ff" : "#a4b2d4"),
                                   center ? 2.0 : 1.0));
        it.value()->setZValue(center ? 2 : related ? 1 : 0);
    }
    for (const EdgeItem &edge : edges_) {
        const bool related = edge.fromId == selectedEntityId_ ||
                             edge.toId == selectedEntityId_;
        edge.line->setOpacity(!hasSelection || related ? 0.9 : 0.12);
        edge.line->setPen(QPen(QColor(related ? "#aebee7" : "#69748b"),
                                related ? 2.2 : 1.6));
    }
    updateInspector();
}

void Relater::updateInspector() {
    relationshipList_->clear();
    const Entity *selected = nullptr;
    for (const Entity &entity : worldData_.entities) {
        if (entity.id == selectedEntityId_) {
            selected = &entity;
            break;
        }
    }
    if (!selected) {
        inspectorTitle_->setText("资料概况");
        inspectorCountLabel_->setText(QString("资料 %1 条\n关系 %2 条")
                                          .arg(worldData_.entities.size())
                                          .arg(worldData_.relationships.size()));
        inspectorHint_->setText("选择一个圆点，查看与这条资料有关的关系。");
        relationshipList_->setVisible(false);
        editRelationshipButton_->setVisible(false);
        deleteRelationshipButton_->setVisible(false);
        return;
    }

    inspectorTitle_->setText(selected->name.trimmed().isEmpty()
                                 ? "未命名资料" : selected->name);
    QString detail = kindText(selected->kind);
    if (!selected->role.trimmed().isEmpty()) detail += " · " + selected->role;
    if (!selected->summary.trimmed().isEmpty())
        detail += "\n\n" + selected->summary.trimmed();
    inspectorCountLabel_->setText(detail);

    for (const Relationship &relation : worldData_.relationships) {
        if (relation.fromId != selectedEntityId_ &&
            relation.toId != selectedEntityId_) continue;

        const bool selectedIsFrom = relation.fromId == selectedEntityId_;
        const QString otherId = selectedIsFrom ? relation.toId : relation.fromId;
        QString otherName = "未命名资料";
        for (const Entity &entity : worldData_.entities) {
            if (entity.id == otherId) {
                if (!entity.name.trimmed().isEmpty()) otherName = entity.name;
                break;
            }
        }
        const QString ownLabel = selectedIsFrom ? relation.label
                                                : relation.reverseLabel;
        const QString displayLabel = ownLabel.trimmed().isEmpty()
            ? QStringLiteral("未设定称呼") : ownLabel.trimmed();
        auto *item = new QListWidgetItem(otherName, relationshipList_);
        item->setData(Qt::UserRole, relation.id);
        item->setData(relationshipLabelRole, displayLabel);
        QString tooltip = QString("%1：%2").arg(otherName, displayLabel);
        if (!relation.notes.trimmed().isEmpty())
            tooltip += "\n\n" + relation.notes.trimmed();
        item->setToolTip(tooltip);
    }
    const bool hasRelations = relationshipList_->count() > 0;
    inspectorHint_->setText(hasRelations ? "相关关系"
                                         : "还没有与这条资料有关的关系");
    relationshipList_->setVisible(hasRelations);
    editRelationshipButton_->setVisible(hasRelations);
    deleteRelationshipButton_->setVisible(hasRelations);
    editRelationshipButton_->setEnabled(false);
    deleteRelationshipButton_->setEnabled(false);
}

void Relater::applyStyle() {
    setStyleSheet(R"QSS(
        QWidget#graphPage {
            background-color: #1c1e22;
            color: #edf0f4;
            font-family: "Microsoft YaHei UI";
        }
        QStackedWidget#graphPageStack,
        QWidget#graphProjectPage { background: transparent; border: none; }
        QLabel#graphStateLabel {
            color: #aeb7c4;
            font-size: 16px;
            background: transparent;
        }
        QLabel#graphTitle {
            color: #edf0f4;
            font-size: 22px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#graphSubtitle {
            color: #9da7b5;
            font-size: 12px;
            background: transparent;
        }
        QFrame#graphPane {
            background-color: #202327;
            border: 1px solid #373c43;
            border-radius: 10px;
        }
        QLabel#graphPaneTitle {
            color: #edf0f4;
            font-size: 15px;
            font-weight: bold;
            background: transparent;
        }
        QLabel#graphCount {
            color: #aebbd5;
            background-color: #293243;
            border: 1px solid #40506b;
            border-radius: 6px;
            padding: 5px 9px;
            font-size: 11px;
        }
        QPushButton#graphShowAllButton {
            background-color: #29354f;
            color: #d6e0ff;
            border: 1px solid #53658d;
            border-radius: 6px;
            padding: 6px 11px;
            font-size: 12px;
        }
        QPushButton#graphShowAllButton:hover {
            background-color: #344365;
            border-color: #8196c8;
        }
        QPushButton#graphShowAllButton:disabled {
            color: #788292;
            border-color: #353c47;
            background-color: #252a33;
        }
        QLabel#graphInspectorCount {
            color: #dce4f0;
            background-color: #1b1f25;
            border: 1px solid #363d48;
            border-radius: 8px;
            padding: 14px;
            font-size: 13px;
        }
        QLabel#graphHint {
            color: #8793a5;
            font-size: 12px;
            background: transparent;
        }
        QLabel#graphSearchStatus {
            color: #aebbd5;
            background: transparent;
            font-size: 12px;
        }
        QLineEdit#graphSearchEdit, QComboBox#graphSearchResults {
            background-color: #1b1f25;
            color: #e4eaf3;
            border: 1px solid #414a59;
            border-radius: 6px;
            padding: 8px 10px;
            font-size: 13px;
            min-height: 18px;
        }
        QLineEdit#graphSearchEdit:focus, QComboBox#graphSearchResults:focus {
            border-color: #8196c8;
        }
        QLineEdit#graphSearchEdit:disabled, QComboBox#graphSearchResults:disabled {
            color: #788292;
            border-color: #353c47;
        }
        QComboBox#graphSearchResults { padding-right: 28px; }
        QComboBox#graphSearchResults::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border: none;
        }
        QComboBox#graphSearchResults::down-arrow {
            image: url(:/icons/chevron-down.xpm);
            width: 12px;
            height: 8px;
        }
        QComboBox#graphSearchResults QAbstractItemView {
            background-color: #252c38;
            color: #e4eaf3;
            selection-background-color: #3a476c;
            selection-color: #f0f4ff;
            border: 1px solid #53658d;
            padding: 4px;
        }
        QSplitter#graphSearchSplitter::handle:vertical {
            background-color: #1c1e22;
            border-top: 1px solid #414a59;
        }
        QSplitter#graphSearchSplitter::handle:vertical:hover {
            background-color: #303744;
            border-top-color: #8196c8;
        }
        QPushButton#graphLocateButton {
            background-color: #29354f;
            color: #d6e0ff;
            border: 1px solid #53658d;
            border-radius: 6px;
            padding: 8px 18px;
            font-size: 13px;
        }
        QPushButton#graphLocateButton:hover { background-color: #344365; }
        QPushButton#graphLocateButton:disabled {
            color: #788292;
            border-color: #353c47;
            background-color: #252a33;
        }
        QPushButton#graphAddRelationship {
            background-color: #303958;
            color: #d3dbff;
            border: 1px solid #56669b;
            border-radius: 7px;
            padding: 8px 15px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton#graphAddRelationship:hover {
            background-color: #3a476c;
            border-color: #7384ba;
        }
        QPushButton#graphAddRelationship:disabled {
            background-color: #2b2f38;
            color: #778092;
            border-color: #3b414c;
        }
        QListWidget#graphRelationshipList {
            background: transparent;
            color: #dce4f0;
            border: none;
            outline: none;
            font-size: 12px;
        }
        QPushButton#graphEditRelationship {
            background-color: #29354f;
            color: #d6e0ff;
            border: 1px solid #53658d;
            border-radius: 7px;
            padding: 7px 13px;
            font-size: 12px;
        }
        QPushButton#graphEditRelationship:hover {
            background-color: #344365;
            border-color: #8196c8;
        }
        QPushButton#graphEditRelationship:disabled {
            background-color: transparent;
            color: #788292;
            border-color: #414956;
        }
        QPushButton#graphDeleteRelationship {
            background-color: transparent;
            color: #e6a3aa;
            border: 1px solid #765158;
            border-radius: 7px;
            padding: 7px 13px;
            font-size: 12px;
        }
        QPushButton#graphDeleteRelationship:hover {
            background-color: #38282d;
            border-color: #b06c78;
        }
        QPushButton#graphDeleteRelationship:disabled {
            color: #78666b;
            border-color: #4b3c41;
            background-color: transparent;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px 0;
        }
        QScrollBar::handle:vertical {
            background-color: #4b5360;
            min-height: 26px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QGraphicsView#graphView {
            background-color: #1b1f25;
            border: 1px solid #343b46;
            border-radius: 8px;
        }
        QSplitter#graphSplitter::handle:horizontal {
            background-color: #1c1e22;
            width: 12px;
        }
        QSplitter#graphSplitter::handle:horizontal:hover {
            background-color: #303744;
        }
    )QSS");
}
