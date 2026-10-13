#include "InlinePromptSettingsStore.h"
#include "AppSettings.h"
#include "../WorkPlaces/Generation/InlinePrompt.h"

namespace {
constexpr auto SystemRulesKey = "prompt/inlineSystemRules";
}

bool InlinePromptSettingsStore::load(QString &rules, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    const QString loaded = settings.value(
        SystemRulesKey, InlinePrompt::defaultSystemRules()).toString();
    if (settings.status() != QSettings::NoError || loaded.trimmed().isEmpty()) {
        error = QStringLiteral("读取行内建议系统提示词失败。");
        return false;
    }
    rules = loaded;
    return true;
}

bool InlinePromptSettingsStore::save(const QString &rules, QString &error) {
    error.clear();
    if (rules.trimmed().isEmpty()) {
        error = QStringLiteral("系统提示词不能为空。");
        return false;
    }
    QSettings settings = AppSettings::open();
    if (rules == InlinePrompt::defaultSystemRules())
        settings.remove(SystemRulesKey);
    else
        settings.setValue(SystemRulesKey, rules);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = QStringLiteral("保存系统提示词失败，请检查设置目录的写入权限。");
        return false;
    }
    return true;
}
