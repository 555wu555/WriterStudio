//
// Created by shijie on 2026/10/10.
//

#include "ContextBuilder.h"
#include "../Generation/InlinePrompt.h"
#include <QStringList>

namespace {
QString textTail(const QString &text, qsizetype maxLength) {
    qsizetype start = qMax(qsizetype(0), text.size() - maxLength);
    if (start > 0 && text.at(start).isLowSurrogate() &&
        text.at(start - 1).isHighSurrogate())
        --start;
    return text.mid(start);
}

QString lastSentenceOrFragment(const QString &recentText) {
    const QString text = recentText.trimmed();
    qsizetype start = 0;
    // 从末尾往前找句号、问号、感叹号或换行；末尾本身是标点时也保留这一句。
    for (qsizetype i = text.size(); i > 1; --i) {
        const QChar ch = text.at(i - 2);
        if (ch == QChar(u'。') || ch == QChar(u'！') ||
            ch == QChar(u'？') || ch == QLatin1Char('!') ||
            ch == QLatin1Char('?') || ch == QLatin1Char('.') ||
            ch == QChar(u'；') || ch == QLatin1Char(';') ||
            ch == QLatin1Char('\n')) {
            start = i - 1;
            break;
        }
    }
    return textTail(text.mid(start).trimmed(),
                    InlinePrompt::LastFragmentCharacters);
}

QStringList commonSections(const Project &project, const Chapter &chapter,
                           const storySetup &setup,
                           const WritingConvention &globalConvention) {
    QStringList sections;
    sections << QStringLiteral("作品名：%1\n章节标题：%2").arg(project.name, chapter.title);

    if (!setup.coreIdea.trimmed().isEmpty())
        sections << QStringLiteral("核心设想：\n%1").arg(setup.coreIdea);
    if (!setup.outline.trimmed().isEmpty())
        sections << QStringLiteral("整体大纲：\n%1").arg(setup.outline);

    const WritingConvention convention = effectiveWritingConvention(setup, globalConvention);
    QStringList conventionLines;
    if (!convention.viewpoint.trimmed().isEmpty())
        conventionLines << QStringLiteral("叙事视角：%1").arg(convention.viewpoint);
    if (!convention.tense.trimmed().isEmpty())
        conventionLines << QStringLiteral("叙事时态：%1").arg(convention.tense);
    if (!convention.style.trimmed().isEmpty())
        conventionLines << QStringLiteral("语言风格：%1").arg(convention.style);
    if (!convention.rhythm.trimmed().isEmpty())
        conventionLines << QStringLiteral("遣词与节奏：%1").arg(convention.rhythm);
    if (!convention.constraints.trimmed().isEmpty())
        conventionLines << QStringLiteral("必须遵守或避免的事项：%1").arg(convention.constraints);
    if (!conventionLines.isEmpty())
        sections << QStringLiteral("写作约定：\n%1").arg(conventionLines.join(QLatin1Char('\n')));

    return sections;
}
}

QString ContextBuilder::buildPreview(const Project &project, const Chapter &chapter, const QString &chapterText,
    const storySetup &setup, const WritingConvention &globalConvention) {
    QStringList sections = commonSections(project, chapter, setup, globalConvention);
    sections << QStringLiteral("正文：\n%1").arg(chapterText);
    return sections.join(QStringLiteral("\n\n"));
}

QString ContextBuilder::buildInlineSuggestionContext(const Project &project, const Chapter &chapter,
    const QString &chapterText, const storySetup &setup, const WritingConvention &globalConvention) {
    // 作品约定只要有一项有值，就整组使用；全空时才使用全局约定。
    QStringList sections = commonSections(project, chapter, setup, globalConvention);

    // 行内建议只需要最近的正文和光标前片段；末尾截取时不拆开代理对。
    const QString recentText = textTail(chapterText,
                                        InlinePrompt::RecentTextCharacters);
    sections << QStringLiteral("正文最近片段（光标紧接在末尾）：\n%1").arg(recentText);
    sections << QStringLiteral("光标前最后一句或片段：\n%1")
                    .arg(lastSentenceOrFragment(recentText));
    return sections.join(QStringLiteral("\n\n"));
}
