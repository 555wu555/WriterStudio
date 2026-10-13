//
// Created by shijie on 2026/10/10.
//

#include "DataRootStore.h"
#include "AppSettings.h"
#include "StoragePaths.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
constexpr auto DataRootKey = "library/dataRoot";
}

bool DataRootStore::load(QString &dataRoot, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    QString path = settings.value(QString::fromLatin1(DataRootKey)).toString().trimmed();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("读取作品库目录设置失败");
        return false;
    }

    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
    }

    if (path.isEmpty()) {
        error = QStringLiteral("无法确定当前用户的应用数据目录，请手动选择作品库位置。");
        return false;
    }
    if (!QDir::isAbsolutePath(path)) {
        error = QStringLiteral("作品库目录必须是绝对路径：%1").arg(path);
        return false;
    }

    dataRoot = QDir::cleanPath(path);
    return true;
}

bool DataRootStore::hasSavedPath(bool &saved, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    saved = !settings.value(QString::fromLatin1(DataRootKey))
                 .toString().trimmed().isEmpty();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("读取作品库目录设置失败");
        return false;
    }
    return true;
}

bool DataRootStore::save(const QString &dataRoot, QString &error) {
    error.clear();
    if (dataRoot.trimmed().isEmpty() || !QDir::isAbsolutePath(dataRoot)) {
        error = QStringLiteral("作品库目录必须是绝对路径");
        return false;
    }
    const QFileInfo directory(dataRoot);
    if (!directory.isDir() || !directory.isReadable() ||
        !QFileInfo(QDir(dataRoot).filePath(StoragePaths::ProjectsDirName)).isDir()) {
        error = QStringLiteral("新作品库目录无效或缺少 projects 文件夹：%1").arg(dataRoot);
        return false;
    }
    const QString canonicalPath = directory.canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        error = QStringLiteral("无法解析新作品库目录：%1").arg(dataRoot);
        return false;
    }

    QSettings settings = AppSettings::open();
    settings.setValue(QString::fromLatin1(DataRootKey), canonicalPath);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("保存作品库目录设置失败");
        return false;
    }
    return true;
}
