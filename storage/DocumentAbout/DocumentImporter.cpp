//
// Created by shijie on 2026/10/6.
//

#include "DocumentImporter.h"
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace {
QString normalizeLineEndings(const QString &text) {
    QString normalized = text;
    if (normalized.startsWith(QChar::ByteOrderMark)) normalized.remove(0, 1);
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QChar('\r'), QChar('\n'));
    return normalized;
}

QString chapterBody(const QStringList &lines, int begin, int end) {
    // 去掉章节标题旁用于排版的空行；正文内部的空行和缩进保持原样。
    while (begin < end && lines.at(begin).trimmed().isEmpty()) ++begin;
    while (end > begin && lines.at(end - 1).trimmed().isEmpty()) --end;
    return lines.mid(begin, end - begin).join('\n');
}
}

bool DocumentImporter::readTxt(QString path, QString &content, QString &error) {
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }

   auto bytes = file.readAll();
    if (file.error()!=QFile::NoError) {
        error = file.errorString();
        return false;
    }


    content = QString::fromUtf8(bytes);
    file.close();

    return true;
}

bool DocumentImporter::parseChapterHeading(const QString &line, bool previousLineBlank, QString &title) {
    title.clear();
    if (!previousLineBlank) return false;

    // 只移除 Windows 换行留下的 \r；行首空白必须保留用于判断缩进。
    QString headingLine = line;
    if (headingLine.endsWith('\r')) headingLine.chop(1);

    static const QRegularExpression headingPattern(
        QStringLiteral(R"(^第([0-9０-９零〇一二三四五六七八九十百千万两]+)章(.*)$)"));
    const QRegularExpressionMatch match = headingPattern.match(headingLine);
    if (!match.hasMatch()) return false;

    QString suffix = match.captured(2).trimmed();
    if (!suffix.isEmpty() && QStringLiteral("·：:、.．-—").contains(suffix.at(0))) {
        suffix.remove(0, 1);
    }
    title = suffix.trimmed();
    if (title.isEmpty()) title = QStringLiteral("未命名");
    return true;
}

SplitNovelText DocumentImporter::splitNovelTxt(const QString &content) {
    const QString normalized = normalizeLineEndings(content);
    const QStringList lines = normalized.split('\n', Qt::KeepEmptyParts);
    SplitNovelText result;
    int headingLine = -1;
    QString currentTitle;

    for (int i = 0; i < lines.size(); ++i) {
        const bool previousLineBlank = i > 0 && lines.at(i - 1).trimmed().isEmpty();
        QString title;
        if (!parseChapterHeading(lines.at(i), previousLineBlank, title)) continue;

        if (headingLine < 0) {
            result.headerText = lines.mid(0, i).join('\n');
        } else {
            result.chapters.append({currentTitle, chapterBody(lines, headingLine + 1, i)});
        }
        headingLine = i;
        currentTitle = title;
    }

    if (headingLine < 0) {
        result.headerText = normalized;
    } else {
        result.chapters.append({currentTitle,
                                chapterBody(lines, headingLine + 1, lines.size())});
    }
    return result;
}

bool DocumentImporter::parseBookHeader(const QString &headerText,ImportedBookHeader &header, QString &error) {
    error.clear();
    header = {};
    const QStringList lines = normalizeLineEndings(headerText).split('\n', Qt::KeepEmptyParts);
    if (lines.isEmpty()) {
        error = QStringLiteral("TXT 文件头为空");
        return false;
    }

    ImportedBookHeader parsed;
    const QString version = QStringLiteral("WriterStudio-TXT/1");
    const QString namePrefix = QStringLiteral("书名：");
    const QString authorPrefix = QStringLiteral("作者：");
    const QString countPrefix = QStringLiteral("章节数：");
    const QString synopsisPrefix = QStringLiteral("简介：");

    if (lines.first() == version) {
        if (lines.size() < 5 ||
            !lines.at(1).startsWith(namePrefix) ||
            !lines.at(2).startsWith(authorPrefix) ||
            !lines.at(3).startsWith(countPrefix) ||
            !lines.at(4).startsWith(synopsisPrefix)) {
            error = QStringLiteral("本地 TXT 文件头缺少书名、作者、章节数或简介字段，或字段顺序不正确");
            return false;
        }

        parsed.name = lines.at(1).mid(namePrefix.size()).trimmed();
        parsed.authorName = lines.at(2).mid(authorPrefix.size()).trimmed();
        bool countOk = false;
        parsed.declaredChapterCount = lines.at(3).mid(countPrefix.size()).trimmed().toInt(&countOk);
        if (!countOk || parsed.declaredChapterCount <= 0) {
            error = QStringLiteral("本地 TXT 的章节数不是有效的正整数");
            return false;
        }

        QStringList synopsisLines;
        synopsisLines.append(lines.at(4).mid(synopsisPrefix.size()));
        synopsisLines.append(lines.mid(5));
        parsed.synopsis = synopsisLines.join('\n').trimmed();
    } else {
        int index = 0;
        while (index < lines.size() && lines.at(index).trimmed().isEmpty()) ++index;
        if (index == lines.size()) {
            error = QStringLiteral("TXT 文件头没有书名");
            return false;
        }
        parsed.name = lines.at(index++).trimmed();

        while (index < lines.size() && lines.at(index).trimmed().isEmpty()) ++index;
        if (index < lines.size() && lines.at(index).startsWith(authorPrefix)) {
            parsed.authorName = lines.at(index++).mid(authorPrefix.size()).trimmed();
        }

        while (index < lines.size() && lines.at(index).trimmed().isEmpty()) ++index;
        if (index < lines.size() && lines.at(index).startsWith(synopsisPrefix)) {
            QStringList synopsisLines;
            synopsisLines.append(lines.at(index++).mid(synopsisPrefix.size()));
            synopsisLines.append(lines.mid(index));
            parsed.synopsis = synopsisLines.join('\n').trimmed();
        }
    }

    if (parsed.name.isEmpty()) {
        error = QStringLiteral("TXT 文件头中的书名为空");
        return false;
    }
    header = parsed;
    return true;
}
