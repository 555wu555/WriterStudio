#include "ModelSettingsStore.h"
#include "AppSettings.h"
#include <QSettings>
#include <QUrl>

namespace {
constexpr auto BaseUrlKey = "model/ollamaBaseUrl";
constexpr auto ModelNameKey = "model/ollamaModelName";
constexpr auto KnownModelsKey = "model/ollamaKnownModels";
}

bool ModelSettingsStore::isOllamaCloudModel(const QString &modelName) {
    return modelName.endsWith(":cloud", Qt::CaseInsensitive) ||
           modelName.endsWith("-cloud", Qt::CaseInsensitive);
}

bool ModelSettingsStore::validate(const ModelSettings &settings, QString &error) {
    error.clear();
    const QUrl url(settings.baseUrl.trimmed());
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty() ||
        (!url.path().isEmpty() && url.path() != "/") ||
        !url.query().isEmpty() || !url.fragment().isEmpty()) {
        error = QStringLiteral("请输入完整服务地址，例如 http://127.0.0.1:11434。");
        return false;
    }
    if (settings.modelName.trimmed().isEmpty()) {
        error = QStringLiteral("请选择一个模型。");
        return false;
    }
    return true;
}

bool ModelSettingsStore::load(ModelSettings &settings, QString &error) {
    error.clear();
    ModelSettings loaded;
    QSettings store = AppSettings::open();
    loaded.baseUrl = store.value(BaseUrlKey, loaded.baseUrl).toString();
    loaded.modelName = store.value(ModelNameKey, loaded.modelName).toString();
    loaded.knownModels = store.value(KnownModelsKey, loaded.knownModels).toStringList();
    if (store.status() != QSettings::NoError) {
        error = QStringLiteral("读取模型设置失败。");
        return false;
    }
    if (!validate(loaded, error)) return false;
    loaded.baseUrl = loaded.baseUrl.trimmed();
    while (loaded.baseUrl.endsWith('/')) loaded.baseUrl.chop(1);
    loaded.modelName = loaded.modelName.trimmed();
    loaded.knownModels.removeAll(QString());
    if (!loaded.knownModels.contains(loaded.modelName))
        loaded.knownModels.prepend(loaded.modelName);
    loaded.knownModels.removeDuplicates();
    settings = loaded;
    return true;
}

bool ModelSettingsStore::save(const ModelSettings &settings, QString &error) {
    if (!validate(settings, error)) return false;
    QSettings store = AppSettings::open();
    QString baseUrl = settings.baseUrl.trimmed();
    while (baseUrl.endsWith('/')) baseUrl.chop(1);
    store.setValue(BaseUrlKey, baseUrl);
    store.setValue(ModelNameKey, settings.modelName.trimmed());
    QStringList names = settings.knownModels;
    names.removeAll(QString());
    names.removeDuplicates();
    store.setValue(KnownModelsKey, names);
    store.sync();
    if (store.status() != QSettings::NoError) {
        error = QStringLiteral("保存模型设置失败，请检查设置目录的写入权限。");
        return false;
    }
    error.clear();
    return true;
}
