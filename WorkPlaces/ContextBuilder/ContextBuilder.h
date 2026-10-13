//
// Created by shijie on 2026/10/10.
//

#ifndef WRITE_CONTEXTBUILDER_H
#define WRITE_CONTEXTBUILDER_H
#include <QString>
#include "../../models/Project.h"
#include "../../models/StorySetup.h"

namespace ContextBuilder {
    QString buildPreview(const Project &project,
                         const Chapter &chapter,
                         const QString &chapterText,
                         const storySetup &setup,
                         const WritingConvention &globalConvention);
    QString buildInlineSuggestionContext(const Project &project,
                         const Chapter &chapter,
                         const QString &chapterText,
                         const storySetup &setup,
                         const WritingConvention &globalConvention);
}


#endif //WRITE_CONTEXTBUILDER_H
