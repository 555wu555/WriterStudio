//
// Created by shijie on 2026/10/4.
//

#include "ChapterStore.h"
#include "../StoragePaths.h"
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QSaveFile>
bool ChapterStore::saveChapter(const QString &projectDirectory, const QString &chapterId, const QString &content,
    QString &error) {
    error.clear();

    const QUuid uuid(chapterId);

    if (uuid.isNull()) {
        error = "章节id无效";
        return false;
    }
    if (projectDirectory.isEmpty()||!QDir::isAbsolutePath(projectDirectory)) {
        error = "路径无效";
        return false;
    }

    const QString chapterDirectory = StoragePaths::chapterDirectory(projectDirectory);
    if (!QDir().mkpath(chapterDirectory)) {
        error = QString("无法创建作品目录：%1").arg(chapterDirectory);
        return false;
    }
    const QString filePath = StoragePaths::chapterFilePath(
        projectDirectory, uuid.toString(QUuid::WithoutBraces));

    QFile oldFile(filePath);
    if (oldFile.exists()) {
        if (!oldFile.open(QIODevice::ReadOnly)) {
            error = oldFile.errorString();
            return false;
        }

        const QByteArray oldData = oldFile.readAll();
        if (oldFile.error() != QFileDevice::NoError) {
            error = oldFile.errorString();
            return false;
        }

        const QString bakPath = StoragePaths::backupDirectory(projectDirectory);
        if (!QDir().mkpath(bakPath)) {
            error = QString("无法创建备份目录：%1").arg(bakPath);
            return false;
        }
        const QString bakFilePath = StoragePaths::chapterBackupFilePath(
            projectDirectory, uuid.toString(QUuid::WithoutBraces));
        QSaveFile bakFile(bakFilePath);

        if (!bakFile.open(QIODevice::WriteOnly)) {
            error = bakFile.errorString();
            return false;
        }
        if (bakFile.write(oldData)!=oldData.size()) {
            error = bakFile.errorString();
            return false;
        }
        if (!bakFile.commit()) {
            error = bakFile.errorString();
            return false;
        }
    }
    oldFile.close();

    const QByteArray data = content.toUtf8();
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }


    if (file.write(data)!=data.size()) {
        error = file.errorString();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;

}

bool ChapterStore::loadChapter(const QString &projectDirectory, const QString &chapterId, QString &content,
    QString &error) {
    error.clear();
    const QUuid uuid(chapterId);

    if (uuid.isNull()) {
        error = "章节id无效";
        return false;
    }
    if (projectDirectory.isEmpty()||!QDir::isAbsolutePath(projectDirectory)) {
        error = "路径无效";
        return false;
    }
    const QString filePath = StoragePaths::chapterFilePath(
        projectDirectory, uuid.toString(QUuid::WithoutBraces));
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    if (file.error()!=QFileDevice::NoError) {
        error = file.errorString();
        return false;
    }
    content = QString::fromUtf8(data);
    return true;

}

bool ChapterStore::loadChapterBackup(const QString &projectDirectory, const QString &chapterId,  QString &content,
     QString &error) {
    error.clear();
    const QUuid uuid(chapterId);

    if (uuid.isNull()) {
        error = "章节id无效";
        return false;
    }
    if (projectDirectory.isEmpty()||!QDir::isAbsolutePath(projectDirectory)) {
        error = "路径无效";
        return false;
    }
    const QString filePath = StoragePaths::chapterBackupFilePath(
        projectDirectory, uuid.toString(QUuid::WithoutBraces));

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        error = file.errorString();
        return false;
    }
    file.close();
    content = QString::fromUtf8(data);
    return true;

}
