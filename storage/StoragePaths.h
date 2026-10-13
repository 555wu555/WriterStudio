#ifndef WRITE_STORAGEPATHS_H
#define WRITE_STORAGEPATHS_H

#include <QDir>
#include <QFileInfo>
#include <QString>

// 只集中管理应用内部的文件和目录名称；用户选择的数据根目录由调用方传入。
namespace StoragePaths {
inline const QString ProjectsDirName = QStringLiteral("projects");
inline const QString ArchiveDirName = QStringLiteral("archive");
inline const QString ProjectFileName = QStringLiteral("project.json");
inline const QString SetupFileName = QStringLiteral("setup.json");
inline const QString ChaptersDirName = QStringLiteral("chapters");
inline const QString BackupDirName = QStringLiteral("backup");
inline const QString ChapterExtension = QStringLiteral(".md");
inline const QString BackupExtension = QStringLiteral(".bak");
inline const QString AuthorAvatarFileName = QStringLiteral("author-avatar.png");
inline const QString EntitiesFileName = QStringLiteral("entities.json");
inline QString projectDirectory(const QString &projectsRoot, const QString &projectId) {
    return QDir(projectsRoot).filePath(projectId);
}

inline QString projectFilePath(const QString &projectDir) {
    return QDir(projectDir).filePath(ProjectFileName);
}

inline QString setupFilePath(const QString &projectDir) {
    return QDir(projectDir).filePath(SetupFileName);
}

inline QString chapterDirectory(const QString &projectDir) {
    return QDir(projectDir).filePath(ChaptersDirName);
}

inline QString chapterRelativePath(const QString &chapterId) {
    return QDir(ChaptersDirName).filePath(chapterId + ChapterExtension);
}

inline QString chapterFilePath(const QString &projectDir, const QString &chapterId) {
    return QDir(projectDir).filePath(chapterRelativePath(chapterId));
}

inline QString backupDirectory(const QString &projectDir) {
    return QDir(chapterDirectory(projectDir)).filePath(BackupDirName);
}

inline QString chapterBackupFilePath(const QString &projectDir, const QString &chapterId) {
    return QDir(backupDirectory(projectDir)).filePath(
        chapterId + ChapterExtension + BackupExtension);
}

inline QString archiveDirectory(const QString &projectsRoot) {
    return QFileInfo(projectsRoot).dir().filePath(ArchiveDirName);
}
inline QString entitiesFilePath(const QString &projectDir) {
    return QDir(projectDir).filePath(EntitiesFileName);
}

}

#endif // WRITE_STORAGEPATHS_H
