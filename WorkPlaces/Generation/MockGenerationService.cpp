#include "MockGenerationService.h"
MockGenerationService::MockGenerationService(QObject *parent)
    : GenerationService(parent) {
    timer_.setSingleShot(true);
    timer_.setInterval(500);
    connect(&timer_, &QTimer::timeout, this, [this] {
        const quint64 completedId = pendingRequestId_;
        pendingRequestId_ = 0;
        if (completedId != 0)
            emit generated(completedId, QStringLiteral("微微一沉"));
    });
}

void MockGenerationService::start(const GenerationRequest &request) {
    // 再次启动会重新计时，先前尚未完成的请求不会产生结果。
    pendingRequestId_ = request.requestId;
    timer_.start();
}

void MockGenerationService::cancel() {
    timer_.stop();
    pendingRequestId_ = 0;
}
