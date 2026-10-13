//
// Created by shijie on 2026/10/11.
//

#ifndef WRITE_OLLAMAGENERATIONSERVICE_H
#define WRITE_OLLAMAGENERATIONSERVICE_H
#include "../GenerationService.h"
#include "../../../storage/ModelSettingsStore.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
class OllamaGenerationService :public GenerationService{
    Q_OBJECT
public:
    OllamaGenerationService(QObject *parent = nullptr);
   void start(const GenerationRequest &request) override;
    void cancel() override;
private:
    void sendRequest(const GenerationRequest &request,
                     const ModelSettings &settings,
                     const QString &systemRules, int tokenLimit,
                     bool isRetry);
    QNetworkAccessManager *networkAccessManager = nullptr;
    QNetworkReply *reply = nullptr;
    quint64 activeLogId_ = 0;
};


#endif //WRITE_OLLAMAGENERATIONSERVICE_H
