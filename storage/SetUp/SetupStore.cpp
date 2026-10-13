//
// Created by shijie on 2026/10/8.
//

#include "SetupStore.h"
#include "../StoragePaths.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonParseError>
#include <QJsonObject>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QStringList>

bool SetupStore::load(const QString &projectDir, storySetup &setup, QString &error) {
    error.clear();

    if (projectDir.isEmpty() || !QDir::isAbsolutePath(projectDir) ||
        !QFileInfo(projectDir).isDir()) {
        error = "作品目录无效";
        return false;
    }

    const QString setupFilePath = StoragePaths::setupFilePath(projectDir);
    if (!QFileInfo::exists(setupFilePath)) {
        setup = storySetup{};
        return true;
    }

    QFile file(setupFilePath);
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
        error = "设定文件的根节点必须是对象";
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value("schemaVersion").toInt(-1) != 1) {
        error = "不支持这个设定文件版本";
        return false;
    }

    const QStringList fields = {
        "coreIdea", "outline", "viewpoint", "tense", "style", "constraints"
    };
    for (const QString &field : fields) {
        if (!root.value(field).isString()) {
            error = QString("设定字段 %1 必须是字符串").arg(field);
            return false;
        }
    }

    // 新字段可缺省，以便读取增加该字段之前保存的作品。
    if (root.contains("rhythm") && !root.value("rhythm").isString()) {
        error = "设定字段 rhythm 必须是字符串";
        return false;
    }

    storySetup loaded;
    loaded.coreIdea = root.value("coreIdea").toString();
    loaded.outline = root.value("outline").toString();
    loaded.viewpoint = root.value("viewpoint").toString();
    loaded.tense = root.value("tense").toString();
    loaded.style = root.value("style").toString();
    loaded.rhythm = root.value("rhythm").toString();
    loaded.constraints = root.value("constraints").toString();
    setup = loaded;

    return true;
}

bool SetupStore::save(const QString &projectDir, const storySetup &setup, QString &error) {
    error.clear();
    if (projectDir.isEmpty()||!QDir::isAbsolutePath(projectDir)) {
        error = "路径无效";
        return false;
    }
    const QString setupFilePath = StoragePaths::setupFilePath(projectDir);
    QJsonObject root;
    root["schemaVersion"] = 1;
    root["constraints"] = setup.constraints;
    root["coreIdea"] = setup.coreIdea;
    root["outline"] = setup.outline;
    root["viewpoint"] = setup.viewpoint;
    root["style"] = setup.style;
    root["rhythm"] = setup.rhythm;
    root["tense"] = setup.tense;

    const QJsonDocument document(root);

    const QByteArray data =
        document.toJson(QJsonDocument::Indented);

    QSaveFile file(setupFilePath);
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
