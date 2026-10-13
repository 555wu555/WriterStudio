#ifndef WRITE_CLOUDPROFILESTORE_H
#define WRITE_CLOUDPROFILESTORE_H

#include <QString>
#include <QVector>

struct CloudProfile {
    QString id;
    QString name;
    QString baseUrl;
    QString modelName;
};

namespace CloudProfileStore {
bool validate(const CloudProfile &profile, QString &error);
bool load(QVector<CloudProfile> &profiles, QString &activeId, QString &error);
bool save(const QVector<CloudProfile> &profiles, const QString &activeId,
          QString &error);
}

#endif // WRITE_CLOUDPROFILESTORE_H
