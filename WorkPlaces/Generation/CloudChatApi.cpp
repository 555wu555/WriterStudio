#include "CloudChatApi.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

QNetworkRequest CloudChatApi::request(const CloudProfile &profile,
                                      const QString &apiKey) {
    QString baseUrl = profile.baseUrl.trimmed();
    while (baseUrl.endsWith('/')) baseUrl.chop(1);
    QNetworkRequest request(QUrl(baseUrl + "/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    // 避免重定向时把密钥转发到意外的地址。
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(90'000);
    return request;
}

QJsonObject CloudChatApi::body(const CloudProfile &profile,
                               const QString &systemPrompt,
                               const QString &userPrompt, int maxTokens) {
    QJsonObject body{
        {"model", profile.modelName},
        {"messages", QJsonArray{
            QJsonObject{{"role", "system"}, {"content", systemPrompt}},
            QJsonObject{{"role", "user"}, {"content", userPrompt}}}},
        {"stream", false},
        {"max_tokens", maxTokens}
    };
    // DeepSeek 默认启用思考模式。行内补全只需要一小段最终文本，
    // 避免生成预算全部消耗在 reasoning_content，导致 content 为空。
    if (QUrl(profile.baseUrl).host().compare(
            QStringLiteral("api.deepseek.com"), Qt::CaseInsensitive) == 0) {
        body["thinking"] = QJsonObject{{"type", "disabled"}};
    }
    return body;
}

QString CloudChatApi::serverError(const QByteArray &response) {
    const QJsonDocument document = QJsonDocument::fromJson(response);
    if (!document.isObject()) return {};
    const QJsonValue value = document.object().value("error");
    if (value.isString()) return value.toString();
    if (value.isObject()) return value.toObject().value("message").toString();
    return {};
}

bool CloudChatApi::parseContent(const QByteArray &response,
                                QString &content, QString &error) {
    content.clear();
    error.clear();
    const QJsonDocument document = QJsonDocument::fromJson(response);
    if (!document.isObject()) {
        error = QStringLiteral("云端服务返回的不是有效 JSON 对象。");
        return false;
    }
    const QJsonArray choices = document.object().value("choices").toArray();
    if (choices.isEmpty() || !choices.first().isObject()) {
        error = QStringLiteral("云端响应缺少 choices。");
        return false;
    }
    const QJsonObject message = choices.first().toObject().value("message").toObject();
    const QJsonValue value = message.value("content");
    if (value.isNull()) return true;
    if (!value.isString()) {
        error = QStringLiteral("云端响应缺少文本内容。");
        return false;
    }
    content = value.toString();
    return true;
}
