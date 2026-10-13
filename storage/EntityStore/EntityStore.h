//
// Created by shijie on 2026/10/8.
//

#ifndef WRITE_ENTITYSTORE_H
#define WRITE_ENTITYSTORE_H
#include <QString>
#include "../../models/Entity.h"
#include "../../models/WorldData.h"
class EntityStore {
public:
    static bool save(const QString &projectDir, const WorldData &data, QString &error);
    static bool load (const QString &projectDir, WorldData &data, QString &error);
};


#endif //WRITE_ENTITYSTORE_H