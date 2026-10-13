//
// Created by shijie on 2026/10/8.
//

#ifndef WRITE_STORYSETUP_H
#define WRITE_STORYSETUP_H
#include <QString>

struct WritingConvention {
    QString viewpoint;
    QString tense;
    QString style;
    QString rhythm;
    QString constraints;

    // 任意一项有实际内容，就使用整组作品约定；不再逐项补全局值。
    bool isEmpty() const {
        return viewpoint.trimmed().isEmpty() && tense.trimmed().isEmpty() &&
               style.trimmed().isEmpty() && rhythm.trimmed().isEmpty() &&
               constraints.trimmed().isEmpty();
    }
};

struct storySetup {
    QString coreIdea;
    QString outline;
    QString viewpoint;
    QString tense;
    QString style;
    QString rhythm;
    QString constraints;

    WritingConvention writingConvention() const {
        return {viewpoint, tense, style, rhythm, constraints};
    }
};

// 后续组装 AI 提示词时也使用此函数，避免界面与提示词采用不同规则。
inline WritingConvention effectiveWritingConvention(const storySetup &project,
                                                     const WritingConvention &global) {
    const WritingConvention local = project.writingConvention();
    return local.isEmpty() ? global : local;
}


#endif //WRITE_STORYSETUP_H
