#include "CloudGenerationService.h"
#include "CloudChatApi.h"
#include "InlinePrompt.h"
#include "ModelRequestLog.h"
#include "../../storage/CloudCredentialStore.h"
#include "../../storage/InlinePromptSettingsStore.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QSslSocket>
#include <algorithm>

CloudGenerationService::CloudGenerationService(QObject *parent)
    : GenerationService(parent) {}

void CloudGenerationService::start(const GenerationRequest &request) {
    cancel();
    QVector<CloudProfile> profiles;
    QString activeId;
    QString error;
    if (!CloudProfileStore::load(profiles, activeId, error)) {
        emit failed(request.requestId, error);
        return;
    }
    const auto selected = std::find_if(profiles.cbegin(), profiles.cend(),
        [&activeId](const CloudProfile &profile) { return profile.id == activeId; });
    if (selected == profiles.cend()) {
        emit failed(request.requestId, QStringLiteral("请先在设置页启用一个云端模型配置。"));
        return;
    }
    if (selected->baseUrl.startsWith("https://", Qt::CaseInsensitive) &&
        !QSslSocket::supportsSsl()) {
        emit failed(request.requestId,
            QStringLiteral("无法使用 HTTPS：缺少 Qt TLS 后端，请重新构建并检查 plugins/tls/qschannelbackend.dll。"));
        return;
    }
    QString apiKey;
    if (!CloudCredentialStore::load(activeId, apiKey, error)) {
        emit failed(request.requestId, error);
        return;
    }
    if (apiKey.isEmpty()) {
        emit failed(request.requestId, QStringLiteral("当前云端配置没有保存 API Key。"));
        return;
    }
    QString systemRules;
    if (!InlinePromptSettingsStore::load(systemRules, error)) {
        emit failed(request.requestId, error);
        return;
    }
    sendRequest(request, *selected, apiKey, systemRules, 256, false);
}

void CloudGenerationService::sendRequest(const GenerationRequest &request,
                                         const CloudProfile &profile,
                                         const QString &apiKey,
                                         const QString &systemRules,
                                         int tokenLimit, bool retry) {
    const QString systemPrompt = InlinePrompt::systemPrompt(true, systemRules);
    const QString userPrompt = InlinePrompt::userPrompt(request);
    const QByteArray json = QJsonDocument(CloudChatApi::body(
        profile, systemPrompt, userPrompt, tokenLimit))
        .toJson(QJsonDocument::Compact);
    const quint64 logId = ModelRequestLog::instance().begin(
        profile.modelName,
        QStringLiteral("系统提示词：\n%1\n\n用户提示词：\n%2")
            .arg(systemPrompt, userPrompt), json, retry);
    QNetworkReply *currentReply = network_.post(
        CloudChatApi::request(profile, apiKey), json);
    reply_ = currentReply;
    activeLogId_ = logId;
    connect(currentReply, &QNetworkReply::finished, this,
            [this, currentReply, request, profile, apiKey, systemRules,
             logId, retry] {
        if (reply_ != currentReply) return;
        reply_ = nullptr;
        activeLogId_ = 0;
        const auto networkError = currentReply->error();
        const QString networkErrorText = currentReply->errorString();
        const int httpStatus = currentReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = currentReply->isOpen()
            ? currentReply->readAll() : QByteArray{};
        currentReply->deleteLater();
        const QString serverError = CloudChatApi::serverError(data);
        if (networkError != QNetworkReply::NoError || httpStatus >= 300 ||
            !serverError.isEmpty()) {
            const QString reason = !serverError.isEmpty() ? serverError
                : networkError != QNetworkReply::NoError ? networkErrorText
                : QStringLiteral("HTTP %1").arg(httpStatus);
            ModelRequestLog::instance().finish(logId, data,
                QStringLiteral("云端请求失败：%1").arg(reason));
            emit failed(request.requestId, reason);
            return;
        }
        QString content;
        QString error;
        if (!CloudChatApi::parseContent(data, content, error)) {
            ModelRequestLog::instance().finish(logId, data, error);
            emit failed(request.requestId, error);
            return;
        }
        if (content.trimmed().isEmpty() && !retry) {
            ModelRequestLog::instance().finish(logId, data, "云端空回复 · 正在重试");
            sendRequest(request, profile, apiKey, systemRules, 512, true);
            return;
        }
        if (content.trimmed().isEmpty()) {
            const QJsonObject root = QJsonDocument::fromJson(data).object();
            const QJsonArray choices = root.value("choices").toArray();
            const bool tokenLimitReached = !choices.isEmpty() &&
                choices.first().toObject().value("finish_reason").toString() == "length";
            const QString reason = tokenLimitReached
                ? QStringLiteral("云端模型已达到生成长度上限，仍没有返回建议文本。")
                : QStringLiteral("云端模型连续两次没有返回建议文本。");
            ModelRequestLog::instance().finish(logId, data, reason);
            emit failed(request.requestId, reason);
            return;
        }
        ModelRequestLog::instance().finish(logId, data, "已返回建议");
        emit generated(request.requestId, content);
    });
}

void CloudGenerationService::cancel() {
    QNetworkReply *oldReply = reply_;
    reply_ = nullptr;
    const quint64 logId = activeLogId_;
    activeLogId_ = 0;
    if (!oldReply) return;
    ModelRequestLog::instance().finish(logId, {}, "已取消");
    oldReply->abort();
    oldReply->deleteLater();
}
