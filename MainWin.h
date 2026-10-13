//
// Created by shijie on 2026/10/2.
//

#ifndef WRITE_MAINWIN_H
#define WRITE_MAINWIN_H
#include <QWidget>
#include <QMainWindow>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QByteArray>
#include "WorkPlaces/WritePaper/WriterWorkspace/WriterWorkspace.h"
#include "WorkPlaces/DesignPaper/designer.h"
class Setter;
class Relater;
class QPushButton;
class QSplitter;
class QShortcut;
class QFrame;
class QLabel;
class MainWin : public QMainWindow{
    Q_OBJECT
public:
    explicit MainWin( const QString &projectsPath,QWidget *parent = nullptr);
    ~MainWin() override = default;
protected:
    void closeEvent( QCloseEvent * ) override;
    void showEvent(QShowEvent *event) override;
private:

    void setUi();
    void leftNavStyle();
    void updateAuthorEntry(const QString &name);
    void updateAuthorAvatar();
    void setFocusMode(bool enabled);
    void selectModelIdentity(const QString &modelKey, const QString &modelName);
    void setModelAvailability(const QString &state, const QString &details);
    QString projectsPath_;
    QWidget *central = nullptr;
    QWidget *leftNav = nullptr;
    QWidget *workPlace = nullptr;
    QSplitter *mainSplitter_ = nullptr;
    QByteArray mainSplitterState_;
    bool focusMode_ = false;
    QShortcut *exitFocusShortcut_ = nullptr;
    WriterWorkspace *writerWorkspace_ = nullptr;
    designer * designerPage = nullptr;
    Relater *relaterPage_ = nullptr;
    Setter *settingsPage_ = nullptr;
    QPushButton *authorAvatarButton_ = nullptr;
    QPushButton *authorNameButton_ = nullptr;
    QFrame *modelAvailabilityBadge_ = nullptr;
    QLabel *modelAvailabilityDot_ = nullptr;
    QLabel *modelAvailabilityText_ = nullptr;
    QString modelStatusKey_;
    QString modelStatusName_;
    QList<QString> funIcons = {"作品库","写作","作品设定","关系图谱","设置"};
    QStackedWidget* workPages = nullptr;
    QButtonGroup* buttonGroup = nullptr;
    QString currentProjectId;

};


#endif //WRITE_MAINWIN_H
