//
// Created by shijie on 2026/10/8.
//

#ifndef WRITE_ENTITY_H
#define WRITE_ENTITY_H
#include <QString>

struct Entity {
    QString id; // id
    QString kind; // 类别
    QString name; // 姓名
    QString role; // 角色定位
    QString summary; // 简介
    QString background; // 人物背景（非人物留空）
    QString notes; // 详细设定
};

#endif //WRITE_ENTITY_H
