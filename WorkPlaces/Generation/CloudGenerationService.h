#ifndef WRITE_CLOUDGENERATIONSERVICE_H
#define WRITE_CLOUDGENERATIONSERVICE_H

#include "GenerationService.h"
#include "../../storage/CloudProfileStore.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

class CloudGenerationService : public GenerationService {
    Q_OBJECT
public:
    explicit CloudGenerationService(QObject *parent = nullptr);
    void start(const GenerationRequest &request) override;
    void cancel() override;

private:
    void sendRequest(const GenerationRequest &request,
                     const CloudProfile &profile, const QString &apiKey,
                     const QString &systemRules, int tokenLimit, bool retry);
    QNetworkAccessManager network_;
    QNetworkReply *reply_ = nullptr;
    quint64 activeLogId_ = 0;
};

#endif // WRITE_CLOUDGENERATIONSERVICE_H
