#include "InlineSuggestionFilter.h"
#include "InlinePrompt.h"
#include <QList>

QString InlineSuggestionFilter::clean(const QString &reply) {
    QString result = reply.trimmed();

    // 只去掉包住整段回复的一对引号，不改变短语内部的文字。
    constexpr struct QuotePair { char16_t open; char16_t close; } quotes[] = {
        {u'“', u'”'}, {u'‘', u'’'}, {u'"', u'"'}, {u'\'', u'\''}
    };
    for (const auto &pair : quotes) {
        if (result.size() >= 2 && result.front() == QChar(pair.open) &&
            result.back() == QChar(pair.close)) {
            result = result.mid(1, result.size() - 2).trimmed();
            break;
        }
    }

    // 模型偶尔会在短语后附句号；清理末尾标点，不把它插入正文。
    const QString sentencePunctuation = QStringLiteral("。！？!?；;，,.");
    while (!result.isEmpty() && sentencePunctuation.contains(result.back()))
        result.chop(1);
    result = result.trimmed();

    // 一处句中逗号可以是自然的短分句，例如“发生什么，她都得先报警”。
    // 其余句中标点或多个逗号通常意味着模型写成了整句/段落。
    int commaCount = 0;
    for (qsizetype i = 0; i < result.size(); ++i) {
        const QChar ch = result.at(i);
        if (ch == QChar(u'，') || ch == QChar(u',')) {
            if (++commaCount > 1 || i == 0 || i == result.size() - 1)
                return {};
        } else if (sentencePunctuation.contains(ch)) {
            return {};
        }
    }

    if (result.isEmpty() || result.contains(QLatin1Char('\n')) ||
        result.contains(QLatin1Char('\r')) ||
        result.contains(QChar(0x2028)) || result.contains(QChar(0x2029)) ||
        result.toUcs4().size() > InlinePrompt::MaxSuggestionCharacters)
        return {};

    return result;
}
