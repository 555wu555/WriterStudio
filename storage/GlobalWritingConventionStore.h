#ifndef WRITE_GLOBALWRITINGCONVENTIONSTORE_H
#define WRITE_GLOBALWRITINGCONVENTIONSTORE_H

#include "../models/StorySetup.h"

class GlobalWritingConventionStore {
public:
    static bool load(WritingConvention &convention, QString &error);
    static bool save(const WritingConvention &convention, QString &error);
};

#endif // WRITE_GLOBALWRITINGCONVENTIONSTORE_H
