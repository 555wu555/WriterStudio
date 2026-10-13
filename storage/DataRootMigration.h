//
// Created by shijie on 2026/10/10.
//

#ifndef WRITE_DATAROOTMIGRATION_H
#define WRITE_DATAROOTMIGRATION_H
#include <QString>

class DataRootMigration {
public:
    static bool validateDestination(const QString &sourceRoot,
                                    const QString &targetRoot,
                                    QString &error);
    static bool copyLibrary(const QString &sourceRoot,
                            const QString &targetRoot,
                            QString &error);
    static bool verifyCopy(const QString &sourceRoot,
                       const QString &targetRoot,
                       QString &error);

};

#endif //WRITE_DATAROOTMIGRATION_H
