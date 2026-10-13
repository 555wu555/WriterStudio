//
// Created by shijie on 2026/10/6.
//

#ifndef WRITE_DOCUMENTIMPORTER_H
#define WRITE_DOCUMENTIMPORTER_H
#include <QString>
#include <QList>

struct ImportedChapterText {
    QString title;
    QString content;
};

struct SplitNovelText {
    QString headerText;                 // 第一章标题之前的文字，供下一步读取书名、作者和简介
    QList<ImportedChapterText> chapters;
};

struct ImportedBookHeader {
    QString name;
    QString authorName;
    QString synopsis;
    int declaredChapterCount = -1;    // 外部 TXT 没有章节数
};

class DocumentImporter {
public:
    static bool readTxt(QString path, QString &content,QString &error);
    static bool parseChapterHeading(const QString &line,bool previousLineBlank,QString &title);
    static SplitNovelText splitNovelTxt(const QString &content);
    static bool parseBookHeader(const QString &headerText,
                                ImportedBookHeader &header, QString &error);
};


#endif //WRITE_DOCUMENTIMPORTER_H
