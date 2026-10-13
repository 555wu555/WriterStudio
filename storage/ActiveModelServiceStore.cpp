#include "ActiveModelServiceStore.h"
#include "AppSettings.h"

namespace {
constexpr auto ActiveServiceKey = "model/activeService";
}

bool ActiveModelServiceStore::load(ActiveModelService &service, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    const QString value = settings.value(ActiveServiceKey, "ollama").toString();
    if (settings.status() != QSettings::NoError ||
        (value != "ollama" && value != "cloud")) {
        error = QStringLiteral("读取当前模型服务失败。");
        return false;
    }
    service = value == "cloud" ? ActiveModelService::Cloud
                               : ActiveModelService::Ollama;
    return true;
}

bool ActiveModelServiceStore::save(ActiveModelService service, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    settings.setValue(ActiveServiceKey,
                      service == ActiveModelService::Cloud ? "cloud" : "ollama");
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("保存当前模型服务失败。");
        return false;
    }
    return true;
}
