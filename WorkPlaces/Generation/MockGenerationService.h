#ifndef WRITE_MOCKGENERATIONSERVICE_H
#define WRITE_MOCKGENERATIONSERVICE_H

#include <QObject>
#include <QTimer>
#include "GenerationRequest.h"
#include "GenerationService.h"
// 用固定文字模拟一次异步生成；稍后可替换为真实服务。
class MockGenerationService : public GenerationService {
    Q_OBJECT
public:
    explicit MockGenerationService(QObject *parent = nullptr);

    void start(const GenerationRequest &request) override ;
    void cancel() override;
private:
    QTimer timer_;
    quint64 pendingRequestId_ = 0;
};

#endif // WRITE_MOCKGENERATIONSERVICE_H
