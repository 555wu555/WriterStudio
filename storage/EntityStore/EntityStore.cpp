//
// Created by shijie on 2026/10/8.
//

#include "EntityStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include "../StoragePaths.h"

namespace {
bool validateWorldData(const WorldData &data, QString &error) {
    QSet<QString> entityIds;
    for (const Entity &entity : data.entities) {
        if (entity.id.trimmed().isEmpty()) {
            error = "实体 ID 不能为空";
            return false;
        }
        if (entityIds.contains(entity.id)) {
            error = QString("实体 ID 重复：%1").arg(entity.id);
            return false;
        }
        entityIds.insert(entity.id);
    }

    QSet<QString> relationIds;
    for (const Relationship &relation : data.relationships) {
        if (relation.id.trimmed().isEmpty()) {
            error = "关系 ID 不能为空";
            return false;
        }
        if (relationIds.contains(relation.id)) {
            error = QString("关系 ID 重复：%1").arg(relation.id);
            return false;
        }
        relationIds.insert(relation.id);

        if (!entityIds.contains(relation.fromId) ||
            !entityIds.contains(relation.toId)) {
            error = QString("关系 %1 指向不存在的实体").arg(relation.id);
            return false;
        }
    }

    return true;
}
}

bool EntityStore::save(const QString &projectDir, const WorldData &data, QString &error) {
    error.clear();
    if (projectDir.isEmpty() || !QDir::isAbsolutePath(projectDir) ||
        !QFileInfo(projectDir).isDir()) {
        error = "作品目录无效";
        return false;
    }
    if (!validateWorldData(data, error)) {
        return false;
    }

    const QString entityFilePath = StoragePaths::entitiesFilePath(projectDir);
    QJsonObject root;
    root["schemaVersion"] = 1;
    QJsonArray entitiesArray;
    for (const auto &entity : data.entities) {
        QJsonObject item;
        item["id"] = entity.id;
        item["kind"] = entity.kind;
        item["name"] = entity.name;
        item["role"] = entity.role;
        item["summary"] = entity.summary;
        item["background"] = entity.background;
        item["notes"] = entity.notes;
        entitiesArray.append(item);
    }
    QJsonArray relationsArray;
    for (const auto &relation : data.relationships) {
        QJsonObject item;
        item["id"] = relation.id;
        item["fromId"] = relation.fromId;
        item["toId"] = relation.toId;
        item["label"] = relation.label;
        item["reverseLabel"] = relation.reverseLabel;
        item["notes"] = relation.notes;

        relationsArray.append(item);
    }
    root["entities"] = entitiesArray;
    root["relations"] = relationsArray;

    const QJsonDocument dataDocument = QJsonDocument(root);

    const QByteArray entityData = dataDocument.toJson(QJsonDocument::Indented);

    QSaveFile file(entityFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }

    if (file.write(entityData) != entityData.size()) {
        error = file.errorString();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool EntityStore::load(const QString &projectDir, WorldData &data, QString &error) {
    error.clear();

    if (projectDir.isEmpty() || !QDir::isAbsolutePath(projectDir) ||
        !QFileInfo(projectDir).isDir()) {
        error = "作品目录无效";
        return false;
    }

    const QString entityFilePath = StoragePaths::entitiesFilePath(projectDir);
    if (!QFileInfo::exists(entityFilePath)) {
        data = WorldData{};
        return true;
    }

    QFile file(entityFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QString("JSON 解析失败：%1").arg(parseError.errorString());
        return false;
    }

    if (!document.isObject()) {
        error = "实体文件的根节点必须是对象";
        return false;
    }

    QJsonObject root = document.object();

    if (root.value("schemaVersion").toInt(-1) != 1) {
        error = "不支持这个实体文件版本";
        return false;
    }

    if (!root.value("entities").isArray() ||
        !root.value("relations").isArray()) {
        error = "实体或关系列表格式错误";
        return false;
    }


    WorldData loaded;
    const QJsonArray entitiesArray = root.value("entities").toArray();

    for (const QJsonValue &value : entitiesArray) {
        if (!value.isObject()) {
            error = "实体列表中存在非对象数据";
            return false;
        }

        const QJsonObject item = value.toObject();
        const QStringList fields = {
            "id", "kind", "name", "role", "summary", "notes"
        };
        for (const QString &field : fields) {
            if (!item.value(field).isString()) {
                error = QString("实体字段 %1 必须是字符串").arg(field);
                return false;
            }
        }
        // 已有 entities.json 没有 background，按空字符串读取。
        if (item.contains("background") && !item.value("background").isString()) {
            error = "实体字段 background 必须是字符串";
            return false;
        }

        Entity entity;
        entity.id = item.value("id").toString();
        entity.kind = item.value("kind").toString();
        entity.name = item.value("name").toString();
        entity.role = item.value("role").toString();
        entity.summary = item.value("summary").toString();
        entity.background = item.value("background").toString();
        entity.notes = item.value("notes").toString();

        loaded.entities.append(entity);
    }

    const QJsonArray relationshipArray = root.value("relations").toArray();

    for (const QJsonValue &value : relationshipArray) {
        if (!value.isObject()) {
            error = "关系列表中存在非对象数据";
            return false;
        }

        const QJsonObject item = value.toObject();
        const QStringList fields = {"id", "fromId", "toId", "label",  "notes"};

        for (const QString &field : fields) {
            if (!item.value(field).isString()) {
                error = QString("关系字段 %1 必须是字符串").arg(field);
                return false;
            }
        }
        if (item.contains("reverseLabel") &&
            !item.value("reverseLabel").isString()) {
            error = "关系字段 reverseLabel 必须是字符串";
            return false;
        }

        Relationship relationship;
        relationship.id = item.value("id").toString();
        relationship.fromId = item.value("fromId").toString();
        relationship.toId = item.value("toId").toString();
        relationship.label = item.value("label").toString();
        relationship.reverseLabel = item.value("reverseLabel").toString();
        relationship.notes = item.value("notes").toString();

        loaded.relationships.append(relationship);

    }

    if (!validateWorldData(loaded, error)) {
        return false;
    }
    data = loaded;
    return true;
}
