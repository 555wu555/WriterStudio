//
// Created by shijie on 2026/10/11.
//

#include "OllamaGenerationService.h"
#include "../ModelRequestLog.h"
#include "../InlinePrompt.h"
#include "../../../storage/InlinePromptSettingsStore.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QUrl>
OllamaGenerationService::OllamaGenerationService(QObject *parent) : GenerationService(parent){
    networkAccessManager = new QNetworkAccessManager(this);
}

void OllamaGenerationService::start(const GenerationRequest &request) {
    cancel();
    ModelSettings settings;
    QString error;
    if (!ModelSettingsStore::load(settings, error)) {
        emit failed(request.requestId, error);
        return;
    }
    QString systemRules;
    if (!InlinePromptSettingsStore::load(systemRules, error)) {
        emit failed(request.requestId, error);
        return;
    }
    sendRequest(request, settings, systemRules,
                ModelSettingsStore::isOllamaCloudModel(settings.modelName) ? 256 : 128,
                false);
}

void OllamaGenerationService::sendRequest(const GenerationRequest &request,
                                         const ModelSettings &settings,
                                         const QString &systemRules,
                                         int tokenLimit, bool isRetry) {
    QNetworkRequest httpRequest(QUrl(settings.baseUrl + "/api/generate"));
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                         "application/json");
    // 非流式请求在生成完成前不会收到正文；避免窗口一直显示“正在生成”。
    const bool cloudModel = ModelSettingsStore::isOllamaCloudModel(
        settings.modelName);
    httpRequest.setTransferTimeout(cloudModel ? 60'000 : 45'000);
    QJsonObject body;
    body["model"] = settings.modelName;
    body["prompt"] = InlinePrompt::userPrompt(request);
    body["system"] = InlinePrompt::systemPrompt(cloudModel, systemRules);
    body["stream"] = false;
    body["think"] = settings.modelName.startsWith("gpt-oss", Qt::CaseInsensitive)
        ? QJsonValue(QStringLiteral("low")) : QJsonValue(false);
    body["options"] = QJsonObject{{"num_predict", tokenLimit}};
    if (!cloudModel) {
        QJsonObject format;
        format["type"] = "object";
        format["properties"] = QJsonObject{
            {"suggestion", QJsonObject{{"type", "string"},
                                         {"maxLength", InlinePrompt::MaxSuggestionCharacters}}}
        };
        format["required"] = QJsonArray{"suggestion"};
        format["additionalProperties"] = false;
        body["format"] = format;
    }

    const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const quint64 logId = ModelRequestLog::instance().begin(
        settings.modelName,
        QStringLiteral("系统提示词：\n%1\n\n用户提示词：\n%2")
            .arg(body.value("system").toString(), body.value("prompt").toString()),
        json, isRetry);
    QNetworkReply *currentReply = networkAccessManager->post(httpRequest, json);
    reply = currentReply;
    activeLogId_ = logId;
    const quint64 requestId = request.requestId;

    connect(currentReply, &QNetworkReply::finished, this,
            [this, currentReply, request, settings, systemRules, requestId, isRetry,
             cloudModel, logId] {
        // 取消或新请求已经替换了它；旧回包不能产生建议或错误。
        if (reply != currentReply) return;
        reply = nullptr;
        activeLogId_ = 0;

        const QByteArray data = currentReply->isOpen()
            ? currentReply->readAll() : QByteArray{};
        const auto networkError = currentReply->error();
        const QString networkErrorText = currentReply->errorString();
        const int httpStatus = currentReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        currentReply->deleteLater();
        ModelRequestLog::instance().finish(
            logId, data, QStringLiteral("已收到 HTTP %1").arg(httpStatus));

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        const QJsonObject root = document.isObject() ? document.object() : QJsonObject{};
        const QString serverError = root.value("error").toString();

        if (networkError != QNetworkReply::NoError || httpStatus >= 400 ||
            !serverError.isEmpty()) {
            QString error = serverError;
            if (error.isEmpty()) {
                if (networkError == QNetworkReply::TimeoutError)
                    error = cloudModel
                        ? QStringLiteral("Ollama 云端模型 60 秒内没有返回结果")
                        : QStringLiteral("本地模型 45 秒内没有返回结果");
                else if (networkError != QNetworkReply::NoError)
                    error = networkErrorText;
                else
                    error = QStringLiteral("Ollama HTTP 错误：%1").arg(httpStatus);
            }
            ModelRequestLog::instance().finish(logId, data,
                QStringLiteral("请求失败：%1").arg(error));
            emit failed(requestId, error);
            return;
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            ModelRequestLog::instance().finish(logId, data, "响应 JSON 格式错误");
            emit failed(requestId, QStringLiteral("Ollama 返回的不是有效 JSON 对象"));
            return;
        }
        if (!root.value("response").isString()) {
            ModelRequestLog::instance().finish(logId, data, "响应缺少 response");
            emit failed(requestId, QStringLiteral("Ollama 响应缺少 response 文本"));
            return;
        }
        const QString responseText = root.value("response").toString();
        // 推理模型可能把较小的生成预算全部用在 thinking 上，导致 response 为空。
        // 只在这种情况下多给一次预算；用户编辑正文时 cancel() 仍可中止重试。
        if (responseText.trimmed().isEmpty() && !isRetry) {
            ModelRequestLog::instance().finish(logId, data, "空回复 · 正在重试");
            sendRequest(request, settings, systemRules,
                        cloudModel ? 512 : 256, true);
            return;
        }
        if (responseText.trimmed().isEmpty()) {
            ModelRequestLog::instance().finish(logId, data, "空回复");
            emit failed(requestId,
                QStringLiteral("%1 连续两次返回空文本（停止原因：%2，生成 token：%3）。")
                    .arg(settings.modelName,
                         root.value("done_reason").toString("未知"))
                    .arg(root.value("eval_count").toInt()));
            return;
        }
        QJsonParseError suggestionParseError;
        const QJsonDocument suggestionDocument = QJsonDocument::fromJson(
            responseText.toUtf8(), &suggestionParseError);
        if (suggestionParseError.error == QJsonParseError::NoError &&
            suggestionDocument.isObject() &&
            suggestionDocument.object().value("suggestion").isString()) {
            ModelRequestLog::instance().finish(logId, data, "已解析建议");
            emit generated(requestId,
                           suggestionDocument.object().value("suggestion").toString());
            return;
        }
        // 较旧的 Ollama 若忽略 format，仍允许合格的纯文本通过最终过滤器。
        ModelRequestLog::instance().finish(logId, data, "已返回纯文本");
        emit generated(requestId, responseText);
    });
}

void OllamaGenerationService::cancel() {
    QNetworkReply *oldReply = reply;
    reply = nullptr;
    const quint64 logId = activeLogId_;
    activeLogId_ = 0;
    if (!oldReply) return;
    ModelRequestLog::instance().finish(logId, {}, "已取消");
    oldReply->abort();
    oldReply->deleteLater();
}
