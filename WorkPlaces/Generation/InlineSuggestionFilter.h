#ifndef WRITE_INLINESUGGESTIONFILTER_H
#define WRITE_INLINESUGGESTIONFILTER_H

#include <QString>

namespace InlineSuggestionFilter {
// 返回可直接显示的短语；不合格的回复返回空字符串。
QString clean(const QString &reply);
}

#endif // WRITE_INLINESUGGESTIONFILTER_H
