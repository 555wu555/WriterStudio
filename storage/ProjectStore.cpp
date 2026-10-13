//
// Created by shijie on 2026/10/4.
//

#include "ProjectStore.h"
#include "StoragePaths.h"
#include <QJsonArray>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QSaveFile>
#include <QFile>
#include <algorithm>
#include <QFileInfo>
// 转为json
QJsonObject  ProjectStore::toJson(const Project &project) {
    QJsonObject root;
    // 作品集
    root["schemaVersion"] = 2;
    root["id"] = project.id;
    root["name"] = project.name;
    root["genre"] = project.genre;
    root["authorName"] = project.authorName;
    QJsonArray tagArray;
    for (const auto &tag : project.tags) {
        tagArray.append(tag);
    }
    root["tags"] = tagArray;
    root["synopsis"] = project.synopsis;
    root["lastChapterId"] = project.lastChapterId;
    root["createdAt"] = project.createdAt.toUTC().toString(Qt::ISODateWithMs);
    root["updatedAt"] = project.updatedAt.toUTC().toString(Qt::ISODateWithMs);

    // 章节转换
    QJsonArray chapterArray;
    for (const auto &chapter : project.chapters) {
        QJsonObject chapterObject;

        chapterObject["id"] = chapter.id;
        chapterObject["title"] = chapter.title;
        chapterObject["file"] = chapter.filePath;
        chapterObject["wordCount"] = chapter.wordCount;

        chapterArray.append(chapterObject);
    }
    root["chapters"] = chapterArray;

    return root;

}
// 保存
bool ProjectStore::saveProject(const Project &project, const QString &projectsPath, QString &error) {
    error.clear();
    if (projectsPath.isEmpty()||!QDir::isAbsolutePath(projectsPath)) {
        error = "路径无效";
        return false;
    }
    const QUuid projectUuid(project.id);
    if (projectUuid.isNull()) {
        error = "作品 ID 无效";
        return false;
    }
    const QString projectDirectory = StoragePaths::projectDirectory(
        projectsPath, projectUuid.toString(QUuid::WithoutBraces));

    if (!QDir().mkpath(projectDirectory)) {
        error = QString("无法创建作品目录：%1").arg(projectDirectory);
        return false;
    }
    for (auto &chapter : project.chapters) {
        if (QUuid(chapter.id).isNull()) {
            error = "文本id失效";
            return false;
        }
        const QString expectedFile = StoragePaths::chapterRelativePath(
            QUuid(chapter.id).toString(QUuid::WithoutBraces));
        if (chapter.filePath != expectedFile) {
            error = QString("章节路径与 ID 不匹配：%1").arg(chapter.title);
            return false;
        }
        const QString chapterFilePath =QDir(projectDirectory).filePath(chapter.filePath);
        if (!QFileInfo(chapterFilePath).isFile()) {
            error =  "文本错误";
            return false;
        }

    }
    const QString filePath = StoragePaths::projectFilePath(projectDirectory);
    const QJsonDocument document(toJson(project));

    const QByteArray data =
        document.toJson(QJsonDocument::Indented);

    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }

    if (file.write(data) != data.size()) {
        error = file.errorString();
        return false;
    }

    if (!file.commit()) {
        error = file.errorString();
        return false;
    }

    return true;
}
// 加载单个
bool ProjectStore::loadProject(const QString &filePath, Project &project, QString &error) {
    error.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QString("JSON 解析失败：%1").arg(parseError.errorString());
        return false;
    }

    if (!document.isObject()) {
        error = "作品文件的根节点必须是对象";
        return false;
    }
    const QJsonObject root = document.object();
    const int version =
        root.value("schemaVersion").toInt(-1);

    if (version != 2) {
        error = "不支持这个作品文件版本";
        return false;
    }


    const QStringList textFields = {
        "id", "name", "genre", "synopsis",
        "lastChapterId", "createdAt", "updatedAt"
    };

    for (const auto &field : textFields) {
        if (!root.value(field).isString()) {
            error = QString("字段 %1 必须是字符串").arg(field);
            return false;
        }
    }
    Project loaded;
    loaded.id = root.value("id").toString();
    loaded.name = root.value("name").toString();
    loaded.genre = root.value("genre").toString();
    loaded.synopsis = root.value("synopsis").toString();
    loaded.lastChapterId = root.value("lastChapterId").toString();
    loaded.createdAt = QDateTime::fromString(
        root.value("createdAt").toString(), Qt::ISODateWithMs
    );
    loaded.updatedAt = QDateTime::fromString(
        root.value("updatedAt").toString(), Qt::ISODateWithMs
    );
    if (root.contains("authorName") && !root.value("authorName").isString()) {
        error = "字段 authorName 必须是字符串";
        return false;
    }
    loaded.authorName = root.value("authorName").toString();
    if (QUuid(loaded.id).isNull()) {
        error = "ID无效";
        return false;
    }
    if (loaded.name.trimmed().isEmpty()) {
        error = "作品名称不能为空";
        return false;
    }

    if (!loaded.createdAt.isValid() || !loaded.updatedAt.isValid()) {
        error = "作品时间无效";
        return false;
    }
    if (!root.value("tags").isArray() ||
    !root.value("chapters").isArray()) {
        error = "tags 和 chapters 必须是数组";
        return false;
    }
    const QJsonArray tagArray = root.value("tags").toArray();

    for (const auto &value : tagArray) {
        if (!value.isString()) {
            error = "标签必须是字符串";
            return false;
        }

        loaded.tags.append(value.toString());
    }
    const QJsonArray chapterArray = root.value("chapters").toArray();

    for (const auto &value : chapterArray) {
        if (!value.isObject()) {
            error = "章节必须是对象";
            return false;
        }

        const QJsonObject chapterObject = value.toObject();

        if (!chapterObject.value("id").isString() ||
            !chapterObject.value("title").isString()) {
                error = "章节的 ID 和标题必须是字符串";
                return false;
            }

        Chapter chapter;
        chapter.id = chapterObject.value("id").toString();
        chapter.title = chapterObject.value("title").toString();
        const QUuid chapterUuid(chapter.id);

        if (chapterUuid.isNull()) {
            error = "章节 ID 无效";
            return false;
        }

        const QString expectedFile = StoragePaths::chapterRelativePath(
            chapterUuid.toString(QUuid::WithoutBraces));
        const qint64 count =
            chapterObject.value("wordCount").toInteger(-1);

        if (count < 0) {
            error = "章节字数必须是非负整数";
            return false;
        }

        if (!chapterObject.value("file").isString() ||
            chapterObject.value("file").toString() != expectedFile) {
            error = "章节路径错误";
            return false;
        }

        chapter.wordCount = count;
        chapter.filePath = chapterObject.value("file").toString();
        loaded.chapters.append(chapter);
    }
    project = loaded;
    return true;
}
// 加载全部
QList<Project> ProjectStore::loadProjects(const QString &projectsPath, QStringList &warnings) {
    warnings.clear();
    QList<Project> projects;
    const QDir rootDirectory(projectsPath);
    if (projectsPath.isEmpty() ||
       !QDir::isAbsolutePath(projectsPath) ||
       !rootDirectory.exists() ||
       !rootDirectory.isReadable()) {
        warnings.append("作品目录不存在或无法读取");
        return projects;
       }
    const QStringList directoryNames = rootDirectory.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks
    );
    for (const auto &directoryName : directoryNames) {
        const QUuid directoryId(directoryName);
        if (directoryId.isNull()) {
            continue;
        }

        const QDir projectDirectory(
            rootDirectory.filePath(directoryName)
        );
        const QString filePath = StoragePaths::projectFilePath(projectDirectory.path());

        Project project;
        QString error;

        if (!loadProject(filePath, project, error)) {
            warnings.append(QString("%1：%2").arg(filePath, error));
            continue;
        }

        if (QUuid(project.id) != directoryId) {
            warnings.append(QString("%1：作品 ID 与目录不一致").arg(filePath));
            continue;
        }

        projects.append(project);
    }
    std::sort(projects.begin(), projects.end(),
       [](const Project &a, const Project &b) {
           return a.updatedAt > b.updatedAt;
       });

    return projects;
}
