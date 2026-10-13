#ifndef WRITE_ROUTEDGENERATIONSERVICE_H
#define WRITE_ROUTEDGENERATIONSERVICE_H

#include "GenerationService.h"
#include "../../storage/ModelServiceIdentity.h"

class RoutedGenerationService : public GenerationService {
    Q_OBJECT
public:
    explicit RoutedGenerationService(QObject *parent = nullptr);
    void start(const GenerationRequest &request) override;
    void cancel() override;

private:
    GenerationService *ollama_ = nullptr;
    GenerationService *cloud_ = nullptr;
    quint64 activeRequestId_ = 0;
    ModelServiceIdentity activeIdentity_;
};

#endif // WRITE_ROUTEDGENERATIONSERVICE_H
