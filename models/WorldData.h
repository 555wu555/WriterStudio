//
// Created by shijie on 2026/10/8.
//

#ifndef WRITE_WORLDDATA_H
#define WRITE_WORLDDATA_H
#include <QVector>
#include "Entity.h"
#include "Relationship.h"
struct WorldData {
    QVector<Entity> entities;
    QVector<Relationship> relationships;
};

#endif //WRITE_WORLDDATA_H