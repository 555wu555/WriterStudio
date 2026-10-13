//
// Created by shijie on 2026/10/10.
//

#ifndef WRITE_DATAROOTSTORE_H
#define WRITE_DATAROOTSTORE_H
#include <QString>

class DataRootStore {
public:
    static bool load(QString &dataRoot, QString &error);
    static bool hasSavedPath(bool &saved, QString &error);
    static bool save(const QString &dataRoot, QString &error);

};


#endif //WRITE_DATAROOTSTORE_H
