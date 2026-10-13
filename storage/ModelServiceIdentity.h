#ifndef WRITE_MODELSERVICEIDENTITY_H
#define WRITE_MODELSERVICEIDENTITY_H

#include "ActiveModelServiceStore.h"
#include "CloudProfileStore.h"
#include "ModelSettingsStore.h"

struct ModelServiceIdentity {
    ActiveModelService service = ActiveModelService::Ollama;
    QString key;
    QString displayName;
};

namespace ModelServiceIdentityStore {
inline ModelServiceIdentity forOllama(const ModelSettings &settings) {
    return {ActiveModelService::Ollama,
            QStringLiteral("ollama:%1|%2").arg(settings.baseUrl, settings.modelName),
            settings.modelName};
}

inline ModelServiceIdentity forCloud(const CloudProfile &profile) {
    return {ActiveModelService::Cloud,
            QStringLiteral("cloud:%1|%2|%3")
                .arg(profile.id, profile.baseUrl, profile.modelName),
            profile.name};
}

inline bool loadActive(ModelServiceIdentity &identity, QString &error) {
    ActiveModelService service;
    if (!ActiveModelServiceStore::load(service, error)) return false;
    if (service == ActiveModelService::Ollama) {
        ModelSettings settings;
        if (!ModelSettingsStore::load(settings, error)) return false;
        identity = forOllama(settings);
        return true;
    }
    QVector<CloudProfile> profiles;
    QString activeId;
    if (!CloudProfileStore::load(profiles, activeId, error)) return false;
    for (const CloudProfile &profile : profiles) {
        if (profile.id == activeId) {
            identity = forCloud(profile);
            return true;
        }
    }
    error = QStringLiteral("尚未选择云端模型配置。");
    return false;
}
}

#endif // WRITE_MODELSERVICEIDENTITY_H
