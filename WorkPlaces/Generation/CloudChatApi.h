#ifndef WRITE_CLOUDCHATAPI_H
#define WRITE_CLOUDCHATAPI_H

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QString>
#include "../../storage/CloudProfileStore.h"

namespace CloudChatApi {
QNetworkRequest request(const CloudProfile &profile, const QString &apiKey);
QJsonObject body(const CloudProfile &profile, const QString &systemPrompt,
                 const QString &userPrompt, int maxTokens);
QString serverError(const QByteArray &response);
bool parseContent(const QByteArray &response, QString &content, QString &error);
}

#endif // WRITE_CLOUDCHATAPI_H
