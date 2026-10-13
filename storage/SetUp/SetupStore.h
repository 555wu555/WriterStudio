//
// Created by shijie on 2026/10/8.
//

#ifndef WRITE_SETUPSTORE_H
#define WRITE_SETUPSTORE_H
#include "../../models/StorySetup.h"

class SetupStore {
public:
    // projectDir 是单部作品的目录：<作品库>/projects/<作品UUID>。
    // setup.json 与该目录下的 project.json 同级。
    static bool load(const QString &projectDir, storySetup &setup,QString &error);
    static bool save(const QString &projectDir,const storySetup &setup,QString &error);
};


#endif //WRITE_SETUPSTORE_H
