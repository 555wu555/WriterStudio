#include "RoutedGenerationService.h"
#include "CloudGenerationService.h"
#include "OllamaGenerationService/OllamaGenerationService.h"

RoutedGenerationService::RoutedGenerationService(QObject *parent)
    : GenerationService(parent) {
    ollama_ = new OllamaGenerationService(this);
    cloud_ = new CloudGenerationService(this);
    for (GenerationService *service : {ollama_, cloud_}) {
        connect(service, &GenerationService::generated,
                this, [this](quint64 requestId, const QString &text) {
            if (requestId != activeRequestId_) return;
            activeRequestId_ = 0;
            emit availabilityChanged(activeIdentity_.key, activeIdentity_.displayName,
                                     "available", "最近一次模型请求成功。");
            emit generated(requestId, text);
        });
        connect(service, &GenerationService::failed,
                this, [this](quint64 requestId, const QString &error) {
            if (requestId != activeRequestId_) return;
            activeRequestId_ = 0;
            emit availabilityChanged(activeIdentity_.key, activeIdentity_.displayName,
                                     "error", error);
            emit failed(requestId, error);
        });
    }
}

void RoutedGenerationService::start(const GenerationRequest &request) {
    cancel();
    QString error;
    if (!ModelServiceIdentityStore::loadActive(activeIdentity_, error)) {
        emit failed(request.requestId, error);
        return;
    }
    activeRequestId_ = request.requestId;
    emit availabilityChanged(activeIdentity_.key, activeIdentity_.displayName,
                             "pending", "正在请求模型建议…");
    (activeIdentity_.service == ActiveModelService::Cloud ? cloud_ : ollama_)
        ->start(request);
}

void RoutedGenerationService::cancel() {
    if (activeRequestId_ != 0)
        emit availabilityChanged(activeIdentity_.key, activeIdentity_.displayName,
                                 "unknown", "请求已取消，尚未完成检测。");
    activeRequestId_ = 0;
    ollama_->cancel();
    cloud_->cancel();
}
