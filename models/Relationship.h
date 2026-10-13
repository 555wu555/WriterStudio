//
// Created by shijie on 2026/10/8.
//

#ifndef WRITE_RELATIONSHIP_H
#define WRITE_RELATIONSHIP_H
#include <QString>
struct Relationship {
    QString id; // id
    QString fromId; // from实体id
    QString toId; //  to实体id
    QString label; // fromId → toId 的关系称呼
    QString reverseLabel; // toId → fromId 的关系称呼，空表示未定义
    QString notes; // 补充说明

};

#endif //WRITE_RELATIONSHIP_H
