//
// Created by shijie on 2026/10/10.
//

#include "DataRootMigration.h"
#include "StoragePaths.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>

namespace {
bool copyTree(const QString &sourcePath, const QString &targetPath, QString &error) {
    if (QFileInfo(sourcePath).isSymLink()) {
        error = QStringLiteral("不能复制符号链接目录：%1").arg(sourcePath);
        return false;
    }
    if (!QDir().mkpath(targetPath)) {
        error = QStringLiteral("无法创建目标目录：%1").arg(targetPath);
        return false;
    }

    const QDir sourceDir(sourcePath);
    const QDir targetDir(targetPath);
    QDirIterator it(sourcePath,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo entry = it.fileInfo();
        if (entry.isSymLink()) {
            error = QStringLiteral("不能复制符号链接：%1").arg(entry.filePath());
            return false;
        }
        const QString target = targetDir.filePath(
            sourceDir.relativeFilePath(entry.filePath()));
        if (entry.isDir()) {
            if (!QDir().mkpath(target)) {
                error = QStringLiteral("无法创建目标目录：%1").arg(target);
                return false;
            }
        } else if (entry.isFile()) {
            if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
                error = QStringLiteral("无法创建文件所在目录：%1").arg(target);
                return false;
            }
            QFile sourceFile(entry.filePath());
            if (!sourceFile.copy(target)) {
                error = QStringLiteral("复制文件失败：%1 → %2（%3）")
                            .arg(entry.filePath(), target, sourceFile.errorString());
                return false;
            }
        } else {
            error = QStringLiteral("无法复制这种文件类型：%1").arg(entry.filePath());
            return false;
        }
    }
    return true;
}

bool collectLibraryEntries(const QString &root, QSet<QString> &entries,
                           QString &error) {
    const QDir rootDir(root);
    for (const QString &name : {StoragePaths::ProjectsDirName,
                                StoragePaths::ArchiveDirName}) {
        const QString path = rootDir.filePath(name);
        const QFileInfo directory(path);
        if (directory.isSymLink()) {
            error = QStringLiteral("不能核对符号链接：%1").arg(name);
            return false;
        }
        if (!directory.exists()) {
            if (name == StoragePaths::ProjectsDirName) {
                error = QStringLiteral("缺少 projects 目录：%1").arg(root);
                return false;
            }
            continue;
        }
        if (!directory.isDir() || !directory.isReadable()) {
            error = QStringLiteral("目录无效或无法读取：%1").arg(name);
            return false;
        }

        entries.insert(name);
        QDirIterator it(path,
                        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo entry = it.fileInfo();
            const QString relative = rootDir.relativeFilePath(entry.filePath());
            if (entry.isSymLink() || (!entry.isDir() && !entry.isFile()) ||
                (entry.isDir() && !entry.isReadable())) {
                error = QStringLiteral("无法核对这种文件或目录：%1").arg(relative);
                return false;
            }
            entries.insert(relative);
        }
    }
    return true;
}

bool fileDigest(const QString &path, QByteArray &digest, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (true) {
        const QByteArray block = file.read(64 * 1024);
        if (block.isEmpty()) {
            if (file.error() != QFileDevice::NoError) {
                error = file.errorString();
                return false;
            }
            break;
        }
        hash.addData(block);
    }
    digest = hash.result();
    return true;
}
}

bool DataRootMigration::validateDestination(const QString &sourceRoot, const QString &targetRoot, QString &error) {
    error.clear();
    if (sourceRoot.trimmed().isEmpty() || targetRoot.trimmed().isEmpty() ||
        !QDir::isAbsolutePath(sourceRoot) || !QDir::isAbsolutePath(targetRoot)) {
        error = QStringLiteral("旧目录和新目录都必须是绝对路径");
        return false;
    }

    const QFileInfo sourceInfo(sourceRoot);
    const QFileInfo targetInfo(targetRoot);
    if (!sourceInfo.isDir()) {
        error = QStringLiteral("旧作品库目录不存在或不是文件夹：%1").arg(sourceRoot);
        return false;
    }
    if (!targetInfo.isDir()) {
        error = QStringLiteral("新作品库目录不存在或不是文件夹：%1").arg(targetRoot);
        return false;
    }
    if (!sourceInfo.isReadable() || !targetInfo.isReadable()) {
        error = QStringLiteral("旧目录或新目录无法读取");
        return false;
    }

    const QString source = QDir::fromNativeSeparators(
        QDir::cleanPath(sourceInfo.canonicalFilePath()));
    const QString target = QDir::fromNativeSeparators(
        QDir::cleanPath(targetInfo.canonicalFilePath()));
    if (source.isEmpty() || target.isEmpty()) {
        error = QStringLiteral("无法解析旧目录或新目录的真实路径");
        return false;
    }
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    const auto isInside = [](const QString &child, const QString &parent) {
        const QString prefix = parent.endsWith(QLatin1Char('/'))
                                   ? parent : parent + QLatin1Char('/');
        return child.startsWith(prefix, pathCase);
    };
    if (source.compare(target, pathCase) == 0 ||
        isInside(source, target) || isInside(target, source)) {
        error = QStringLiteral("新旧作品库目录不能相同，也不能互相包含");
        return false;
    }

    const QDir targetDir(target);
    const auto targetEntries = targetDir.entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    if (!targetEntries.isEmpty()) {
        error = QStringLiteral("新作品库目录必须为空：%1").arg(targetRoot);
        return false;
    }

    const QString projectsPath = QDir(source).filePath(StoragePaths::ProjectsDirName);
    if (!QFileInfo(projectsPath).isDir()) {
        error = QStringLiteral("旧目录缺少 projects 文件夹：%1").arg(projectsPath);
        return false;
    }
    return true;
}

bool DataRootMigration::copyLibrary(const QString &sourceRoot,
                                    const QString &targetRoot, QString &error) {
    if (!validateDestination(sourceRoot, targetRoot, error))
        return false;

    const QDir source(sourceRoot);
    const QDir target(targetRoot);
    const QString projectsName = StoragePaths::ProjectsDirName;
    if (!copyTree(source.filePath(projectsName), target.filePath(projectsName), error))
        return false;

    const QString archiveName = StoragePaths::ArchiveDirName;
    const QString archivePath = source.filePath(archiveName);
    const QFileInfo archiveInfo(archivePath);
    if (archiveInfo.isSymLink()) {
        error = QStringLiteral("不能复制符号链接目录：%1").arg(archivePath);
        return false;
    }
    if (archiveInfo.exists()) {
        if (!archiveInfo.isDir()) {
            error = QStringLiteral("旧归档路径不是文件夹：%1").arg(archivePath);
            return false;
        }
        if (!copyTree(archivePath, target.filePath(archiveName), error))
            return false;
    }
    return true;
}

bool DataRootMigration::verifyCopy(const QString &sourceRoot, const QString &targetRoot, QString &error) {
    error.clear();
    if (sourceRoot.trimmed().isEmpty() || targetRoot.trimmed().isEmpty() ||
        !QDir::isAbsolutePath(sourceRoot) || !QDir::isAbsolutePath(targetRoot)) {
        error = QStringLiteral("旧目录和新目录都必须是绝对路径");
        return false;
    }
    const QFileInfo sourceRootInfo(sourceRoot);
    const QFileInfo targetRootInfo(targetRoot);
    if (!sourceRootInfo.isDir() || !targetRootInfo.isDir() ||
        !sourceRootInfo.isReadable() || !targetRootInfo.isReadable()) {
        error = QStringLiteral("旧目录或新目录不存在，或无法读取");
        return false;
    }
    const QString source = QDir::fromNativeSeparators(sourceRootInfo.canonicalFilePath());
    const QString target = QDir::fromNativeSeparators(targetRootInfo.canonicalFilePath());
    if (source.isEmpty() || target.isEmpty()) {
        error = QStringLiteral("无法解析旧目录或新目录的真实路径");
        return false;
    }
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    const auto isInside = [](const QString &child, const QString &parent) {
        const QString prefix = parent.endsWith(QLatin1Char('/'))
                                   ? parent : parent + QLatin1Char('/');
        return child.startsWith(prefix, pathCase);
    };
    if (source.compare(target, pathCase) == 0 ||
        isInside(source, target) || isInside(target, source)) {
        error = QStringLiteral("新旧作品库目录不能相同，也不能互相包含");
        return false;
    }

    QSet<QString> sourceEntries;
    QSet<QString> targetEntries;
    if (!collectLibraryEntries(source, sourceEntries, error) ||
        !collectLibraryEntries(target, targetEntries, error))
        return false;

    const QDir sourceDir(source);
    const QDir targetDir(target);
    for (const QString &relative : sourceEntries) {
        if (!targetEntries.contains(relative)) {
            error = QStringLiteral("新目录缺少：%1").arg(relative);
            return false;
        }
        const QFileInfo sourceInfo(sourceDir.filePath(relative));
        const QFileInfo targetInfo(targetDir.filePath(relative));
        if (sourceInfo.isDir() != targetInfo.isDir() ||
            sourceInfo.isFile() != targetInfo.isFile()) {
            error = QStringLiteral("文件类型不同：%1").arg(relative);
            return false;
        }
        if (!sourceInfo.isFile()) continue;
        if (sourceInfo.size() != targetInfo.size()) {
            error = QStringLiteral("文件大小不同：%1").arg(relative);
            return false;
        }
        QByteArray sourceDigest;
        QByteArray targetDigest;
        QString fileError;
        if (!fileDigest(sourceInfo.filePath(), sourceDigest, fileError) ||
            !fileDigest(targetInfo.filePath(), targetDigest, fileError)) {
            error = QStringLiteral("读取文件失败：%1（%2）").arg(relative, fileError);
            return false;
        }
        if (sourceDigest != targetDigest) {
            error = QStringLiteral("文件内容不同：%1").arg(relative);
            return false;
        }
    }
    for (const QString &relative : targetEntries) {
        if (!sourceEntries.contains(relative)) {
            error = QStringLiteral("新目录多出：%1").arg(relative);
            return false;
        }
    }
    for (const QFileInfo &entry : targetDir.entryInfoList(
             QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        if (entry.fileName() != StoragePaths::ProjectsDirName &&
            entry.fileName() != StoragePaths::ArchiveDirName) {
            error = QStringLiteral("新目录多出：%1").arg(entry.fileName());
            return false;
        }
    }
    return true;
}


