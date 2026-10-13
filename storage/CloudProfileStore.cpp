#include "CloudProfileStore.h"
#include "AppSettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

namespace {
constexpr auto ProfilesKey = "model/cloudProfiles";
constexpr auto ActiveIdKey = "model/activeCloudProfileId";
}

bool CloudProfileStore::validate(const CloudProfile &profile, QString &error) {
    error.clear();
    if (profile.id.trimmed().isEmpty() || profile.name.trimmed().isEmpty() ||
        profile.modelName.trimmed().isEmpty()) {
        error = QStringLiteral("请填写配置名称和模型名称。");
        return false;
    }
    const QUrl url(profile.baseUrl.trimmed());
    const QString host = url.host().toLower();
    const bool loopback = host == "localhost" || host == "127.0.0.1" ||
                          host == "[::1]" || host == "::1";
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() ||
        !url.query().isEmpty() || !url.fragment().isEmpty() ||
        (url.scheme() != "https" && !(url.scheme() == "http" && loopback))) {
        error = QStringLiteral("云端接口地址必须使用 HTTPS；只有本机回环地址允许 HTTP。");
        return false;
    }
    if (url.path().endsWith("/chat/completions")) {
        error = QStringLiteral("请填写接口基础地址，不要包含 /chat/completions。");
        return false;
    }
    return true;
}

bool CloudProfileStore::load(QVector<CloudProfile> &profiles,
                             QString &activeId, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    const QByteArray json = settings.value(ProfilesKey).toByteArray();
    const QString loadedActiveId = settings.value(ActiveIdKey).toString();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("读取云端模型配置失败。");
        return false;
    }
    QVector<CloudProfile> loaded;
    if (!json.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(json);
        if (!document.isArray()) {
            error = QStringLiteral("云端模型配置格式错误。");
            return false;
        }
        QSet<QString> ids;
        for (const QJsonValue &value : document.array()) {
            if (!value.isObject()) {
                error = QStringLiteral("云端模型配置包含无效条目。");
                return false;
            }
            const QJsonObject object = value.toObject();
            if (!object.value("id").isString() ||
                !object.value("name").isString() ||
                !object.value("baseUrl").isString() ||
                !object.value("modelName").isString()) {
                error = QStringLiteral("云端模型配置缺少必要字段。");
                return false;
            }
            CloudProfile profile{object.value("id").toString(),
                                 object.value("name").toString(),
                                 object.value("baseUrl").toString(),
                                 object.value("modelName").toString()};
            if (!validate(profile, error)) return false;
            if (ids.contains(profile.id)) {
                error = QStringLiteral("云端模型配置 ID 重复。");
                return false;
            }
            ids.insert(profile.id);
            loaded.append(profile);
        }
        if (!loadedActiveId.isEmpty() && !ids.contains(loadedActiveId)) {
            error = QStringLiteral("当前云端模型配置不存在。");
            return false;
        }
    } else if (!loadedActiveId.isEmpty()) {
        error = QStringLiteral("当前云端模型配置不存在。");
        return false;
    }
    profiles = loaded;
    activeId = loadedActiveId;
    return true;
}

bool CloudProfileStore::save(const QVector<CloudProfile> &profiles,
                             const QString &activeId, QString &error) {
    error.clear();
    QSet<QString> ids;
    QJsonArray entries;
    for (const CloudProfile &profile : profiles) {
        if (!validate(profile, error)) return false;
        if (ids.contains(profile.id)) {
            error = QStringLiteral("云端模型配置 ID 重复。");
            return false;
        }
        ids.insert(profile.id);
        entries.append(QJsonObject{{"id", profile.id},
                                   {"name", profile.name.trimmed()},
                                   {"baseUrl", profile.baseUrl.trimmed()},
                                   {"modelName", profile.modelName.trimmed()}});
    }
    if (!activeId.isEmpty() && !ids.contains(activeId)) {
        error = QStringLiteral("当前云端模型配置不存在。");
        return false;
    }
    QSettings settings = AppSettings::open();
    settings.setValue(ProfilesKey, QJsonDocument(entries).toJson(QJsonDocument::Compact));
    settings.setValue(ActiveIdKey, activeId);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("保存云端模型配置失败，请检查设置目录的写入权限。");
        return false;
    }
    return true;
}
