#ifndef WRITE_INLINEPROMPTSETTINGSSTORE_H
#define WRITE_INLINEPROMPTSETTINGSSTORE_H

#include <QString>

// 保存用户可编辑的行内建议系统规则；输出格式约束由 InlinePrompt 固定补上。
namespace InlinePromptSettingsStore {
bool load(QString &rules, QString &error);
bool save(const QString &rules, QString &error);
}

#endif // WRITE_INLINEPROMPTSETTINGSSTORE_H
