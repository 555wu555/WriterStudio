//
// Created by shijie on 2026/10/5.
//

#ifndef WRITE_WRITERWORKSPACE_H
#define WRITE_WRITERWORKSPACE_H
#include <QWidget>
#include <QTabWidget>
#include "../Writer.h"
#include <QHash>
#include <QStackedWidget>
class WriterWorkspace : public QWidget{
    Q_OBJECT
public:
    explicit WriterWorkspace(QWidget *parent = nullptr);
    ~WriterWorkspace() override = default;
    bool openProject(const QString &projectsPath,const QString &projectId,QString &error,
                     bool resumeWriting = true);
    QString currentProjectId() const;
    bool hasOpenProjects() const { return !openedProjects_.isEmpty(); }
    bool hasActiveChapter() const;
    void refreshCurrentReferences();
    void setFocusMode(bool enabled);
    void setInlineSuggestionEnabled(bool enabled);
    bool confirmCloseAll();
    bool closeProject(const QString &projectId);
    void updateOpenProjectInfo(const Project &project);
signals:
    void libraryRequested();
    void currentProjectChanged(const QString &projectId);
    void focusModeExitRequested();
    void modelAvailabilityChanged(const QString &modelKey, const QString &modelName,
                                  const QString &state, const QString &details);

private:
    QStackedWidget *bodyStack_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QHash<QString, Writer *> openedProjects_;
    Writer *focusedPage_ = nullptr;
    bool focusMode_ = false;
    bool inlineSuggestionEnabled_ = true;
    void setUi();
    void uiStyle();
    void closeProjectTab(int index);

};


#endif //WRITE_WRITERWORKSPACE_H
