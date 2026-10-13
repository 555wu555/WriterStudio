//
// Created by shijie on 2026/10/4.
//

#ifndef WRITE_PROJECTSTORE_H
#define WRITE_PROJECTSTORE_H
#include <QJsonObject>
#include "../models/Project.h"
class ProjectStore {
public:
    static QJsonObject  toJson(const Project& project);
    static bool saveProject(const Project &project,const QString &projectsPath,QString &error);
    static bool loadProject( const QString &filePath,Project &project,QString &error);
    static QList<Project> loadProjects(const QString &projectsPath,QStringList &warnings);
};


#endif //WRITE_PROJECTSTORE_H