//
// Created by shijie on 2026/10/4.
//

#ifndef WRITE_CHAPTERSTORE_H
#define WRITE_CHAPTERSTORE_H
#include <QString>

class ChapterStore {
public:
    static bool saveChapter(
        const QString &projectDirectory, //文件夹
        const QString &chapterId,       // 章节id
        const QString &content,         // 正文
        QString &error                 // 失败原因
    );
    static bool loadChapter(
        const QString &projectDirectory,
        const QString &chapterId,
        QString &content,  // 输出内容
        QString &error
    );
    static bool loadChapterBackup(const QString &projectDirectory,
        const QString &chapterId,
         QString &content,
         QString &error);
};


#endif //WRITE_CHAPTERSTORE_H