#ifndef WRITE_CLOUDCREDENTIALSTORE_H
#define WRITE_CLOUDCREDENTIALSTORE_H

#include <QString>

// API Key 只保存在 Windows 凭据管理器；配置文件和请求日志不保存它。
namespace CloudCredentialStore {
bool load(const QString &profileId, QString &apiKey, QString &error);
bool save(const QString &profileId, const QString &apiKey, QString &error);
bool remove(const QString &profileId, QString &error);
}

#endif // WRITE_CLOUDCREDENTIALSTORE_H
