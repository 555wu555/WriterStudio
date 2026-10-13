#include "InlineSuggestionSettings.h"
#include "AppSettings.h"

namespace {
constexpr auto EnabledKey = "writing/inlineSuggestionEnabled";
constexpr bool DefaultEnabled = true;
}

bool InlineSuggestionSettings::load(bool &enabled, QString &error) {
    error.clear();
    enabled = DefaultEnabled;
    QSettings settings = AppSettings::open();
    const bool loaded = settings.value(EnabledKey, DefaultEnabled).toBool();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("读取自动短语建议设置失败。");
        return false;
    }
    enabled = loaded;
    return true;
}

bool InlineSuggestionSettings::save(bool enabled, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    settings.setValue(EnabledKey, enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("保存自动短语建议设置失败，请检查设置目录的写入权限。");
        return false;
    }
    return true;
}
