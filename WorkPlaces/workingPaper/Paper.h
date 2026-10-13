//
// Created by shijie on 2026/10/3.
//

#ifndef WRITE_PAPER_H
#define WRITE_PAPER_H
#include <QWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include "../../models/Project.h"
#include "../../models/dialogs/CreateProjectDialog.h"
struct ProjectPreview {
    QString id;
    QString name;
    QString genre;
    QStringList tags;
    QString updated;
    int chapterCount = 0;
    qint64 wordCount = 0;
    QString synopsis;
    QString lastChapter;
};

class Paper : public QWidget{
    Q_OBJECT
public:
    explicit  Paper( const QString &projectsPath,QWidget *parent);
    ~Paper() override = default;
    void setDefaultAuthorName(const QString &name);
public slots:
    void exportSelectedProject();
    void refreshProjects();
    void selectProject(const QString &id);
    void removeSelectedProject();
    void importProjectTxt();
    void editSelectedProject();
signals:
    void openProjectRequested(const QString &projectId);
    void openPlanningRequested(const QString &projectId);
    void archiveProjectRequested(const QString &id);
    void restoreProjectRequested(const QString &id);
    void projectInfoChanged(const Project &project);
private:
    QString projectsPath_;
    QPushButton *btnLoad;
    QPushButton *btnCreate;
    QStackedWidget *bodyStack = nullptr;
    QFrame * emptyPage;
    QFrame * listPanel;
    QFrame *detailPanel;
    QSplitter *filledPage;
    QListWidget *projectList = nullptr;
    QLabel *bookNameLabel = nullptr;
    QList<ProjectPreview> bookProjects;
    QLabel *chapterCountLabel = nullptr;
    QLabel *wordCountLabel = nullptr;
    QLabel *synopsisLabel = nullptr;
    QLabel *lastChapterLabel = nullptr;
    QLabel *genreLabel = nullptr;
    QLabel *tagsLabel = nullptr;
    QLabel *updatedLabel = nullptr;
    QLabel *projectCountLabel = nullptr;
    QString authorName ;
    void createProject();
    void headerStyle();
    void emptyStyle();
    void fileStyle();
    ProjectPreview makePreview(const Project &project) const;
    void loadBooksList();
    void renderProjectList();
    void showProjectDetails(const QString &projectId);
};


#endif //WRITE_PAPER_H
