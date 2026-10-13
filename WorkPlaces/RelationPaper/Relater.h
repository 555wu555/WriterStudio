//
// Created by shijie on 2026/10/3.
//

#ifndef WRITE_RELATER_H
#define WRITE_RELATER_H
#include <QLabel>
#include <QWidget>
#include <QString>
#include "../../models/WorldData.h"
#include <QStackedWidget>
#include <QHash>
#include <QPointF>

class QGraphicsScene;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsView;
class QListWidget;
class QPushButton;
class QTimer;
class QLineEdit;
class QComboBox;

class Relater : public QWidget{
    Q_OBJECT
public:
    explicit Relater(QWidget *parent = nullptr);
    ~Relater() override;
public slots:
    void setProjectId(const QString &projectsPath, const QString &id);
    bool locateEntityById(const QString &entityId);
signals:
    void entityOpenRequested(const QString &entityId);

private:
    QStackedWidget *pageStack_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *inspectorCountLabel_ = nullptr;
    QGraphicsScene *scene_ = nullptr;
    QGraphicsView *view_ = nullptr;
    QPushButton *addRelationshipButton_ = nullptr;
    QPushButton *showAllButton_ = nullptr;
    QLabel *inspectorTitle_ = nullptr;
    QLabel *inspectorHint_ = nullptr;
    QListWidget *relationshipList_ = nullptr;
    QPushButton *editRelationshipButton_ = nullptr;
    QPushButton *deleteRelationshipButton_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QComboBox *searchResults_ = nullptr;
    QWidget *searchResultsRow_ = nullptr;
    QLabel *searchStatus_ = nullptr;
    QPushButton *locateButton_ = nullptr;
    QHash<QString, QGraphicsEllipseItem *> nodes_;
    QHash<QString, QPointF> nodeVelocities_;
    QTimer *motionTimer_ = nullptr;
    QString draggedNodeId_;
    int settledFrames_ = 0;
    bool updatingMotion_ = false;
    struct EdgeItem {
        QGraphicsLineItem *line = nullptr;
        QString fromId;
        QString toId;
    };
    QVector<EdgeItem> edges_;
    WorldData worldData_;
    QString projectId_;
    QString projectDir_;
    QString selectedEntityId_;

    void renderGraph(bool resetLayout = false);
    void updateEdgePositions();
    void advanceGraphMotion();
    void createRelationship();
    void editSelectedRelationship();
    void deleteSelectedRelationship();
    void updateCounts();
    void updateInspector();
    void updateSelection();
    void onSelectionChanged();
    void updateSearchResults();
    void locateSearchResult();
    void applyStyle();
};


#endif //WRITE_RELATER_H
