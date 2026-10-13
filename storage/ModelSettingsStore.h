#ifndef WRITE_MODELSETTINGSSTORE_H
#define WRITE_MODELSETTINGSSTORE_H

#include <QString>
#include <QStringList>

struct ModelSettings {
    QString baseUrl = QStringLiteral("http://127.0.0.1:11434");
    QString modelName = QStringLiteral("deepseek-r1:8b");
    QStringList knownModels = {QStringLiteral("deepseek-r1:8b")};
};

namespace ModelSettingsStore {
bool isOllamaCloudModel(const QString &modelName);
bool validate(const ModelSettings &settings, QString &error);
bool load(ModelSettings &settings, QString &error);
bool save(const ModelSettings &settings, QString &error);
}

#endif // WRITE_MODELSETTINGSSTORE_H
