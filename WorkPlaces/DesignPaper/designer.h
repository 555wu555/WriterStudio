//
// Created by shijie on 2026/10/3.
//

#ifndef WRITE_DESIGNER_H
#define WRITE_DESIGNER_H
#include <QWidget>
#include <QString>
#include "../../models/WorldData.h"
class QLabel;
class QPlainTextEdit;
class QComboBox;
class QStackedWidget;
class QScrollArea;
class QListWidget;
class QListWidgetItem;
class QPushButton;
struct storySetup;
class designer : public QWidget{
    Q_OBJECT
public:
    explicit designer(QWidget *parent = nullptr);
    ~designer() override = default;
    bool confirmLeave();
public slots:
    void setProjectId(const QString &projectsPath, const QString &id);
    void refreshEffectiveConvention();
    void selectEntityById (const QString &entityId);
    void showOutline();
signals:
    void locateEntityInGraphRequested(const QString &entityId);

private:
    void buildUi();
    void clearForm();
    void applySetup(const storySetup &setup);
    storySetup collectSetup() const;
    void markDirty();
    bool saveSetup();
    void setSaveStatus(const QString &state, const QString &text);
    void applyStyle();
    void refreshEntityList(const QString &preferredId = QString());
    void showSelectedEntity(QListWidgetItem *item);
    void editSelectedEntity();
    void removeSelectedEntity();
    WorldData worldData_;
    QStackedWidget *pageStack_ = nullptr;
    QStackedWidget *sectionStack_ = nullptr;
    QListWidget *sectionList_ = nullptr;
    QListWidget *entityList_ = nullptr;
    QPushButton *viewInGraphButton_ = nullptr;
    QComboBox *entityFilterCombo_ = nullptr;
    QLabel *entityEmptyLabel_ = nullptr;
    QStackedWidget *entityDetailStack_ = nullptr;
    QScrollArea *entityDetailScroll_ = nullptr;
    QLabel *entityNameLabel_ = nullptr;
    QLabel *entityKindLabel_ = nullptr;
    QWidget *entityRoleRow_ = nullptr;
    QLabel *entityRoleLabel_ = nullptr;
    QLabel *entitySummaryLabel_ = nullptr;
    QWidget *entityBackgroundRow_ = nullptr;
    QLabel *entityBackgroundLabel_ = nullptr;
    QLabel *entityNotesLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLabel *projectNameLabel_ = nullptr;
    QLabel *projectMetaLabel_ = nullptr;
    QLabel *saveStatusLabel_ = nullptr;
    QLabel *ruleSourceLabel_ = nullptr;
    QLabel *effectiveRulesLabel_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QPlainTextEdit *coreIdeaEdit_ = nullptr;
    QPlainTextEdit *outlineEdit_ = nullptr;
    QPlainTextEdit *styleEdit_ = nullptr;
    QPlainTextEdit *rhythmEdit_ = nullptr;
    QPlainTextEdit *constraintsEdit_ = nullptr;
    QComboBox *viewpointCombo_ = nullptr;
    QComboBox *tenseCombo_ = nullptr;
    bool loadingForm_ = false;
    bool dirty_ = false;
    QString id_;
    QString projectsPath_;
};


#endif //WRITE_DESIGNER_H
