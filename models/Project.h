//
// Created by shijie on 2026/10/4.
//

#ifndef WRITE_PROJECT_H
#define WRITE_PROJECT_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QStringList>
// 章节
struct Chapter {
    QString id;
    QString title;

    QString filePath;       // 相对于作品文件夹的路径
    qint64 wordCount = 0;    // 用于作品库显示

};
// 作品
struct Project {
    QString id;
    QString name;
    QString genre;  // 主标签
    QStringList tags;    // 多个标签
    QString synopsis;
    QString authorName;
    QDateTime createdAt;
    QDateTime updatedAt;

    QList<Chapter> chapters;
    QString lastChapterId;
};

#endif




