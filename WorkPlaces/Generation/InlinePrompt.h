#ifndef WRITE_INLINEPROMPT_H
#define WRITE_INLINEPROMPT_H

#include <QString>
#include <QtGlobal>

struct GenerationRequest;

// 行内补词的固定规则集中在这里。作品资料和正文由 ContextBuilder 组装。
namespace InlinePrompt {
inline constexpr qsizetype RecentTextCharacters = 500;
inline constexpr qsizetype LastFragmentCharacters = 120;
inline constexpr int PreferredSuggestionMinCharacters = 2;
inline constexpr int PreferredSuggestionMaxCharacters = 12;
inline constexpr int MaxSuggestionCharacters = 20;

QString instruction();
QString defaultSystemRules();
QString systemPrompt(bool ollamaCloud, const QString &systemRules);
QString userPrompt(const GenerationRequest &request);
}

#endif // WRITE_INLINEPROMPT_H
