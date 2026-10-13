//
// Created by shijie on 2026/10/11.
//

#ifndef WRITE_GENERATIONSERVICE_H
#define WRITE_GENERATIONSERVICE_H
#include <QObject>
#include <QString>
#include "GenerationRequest.h"
class GenerationService :public QObject {
    Q_OBJECT
public:
    GenerationService(QObject *parent);
    virtual void start(const GenerationRequest & ) = 0;
    virtual void cancel() = 0;
signals:
    void generated(quint64 requestId, const QString &text);
    void failed(quint64 requestId, const QString &error);
    void availabilityChanged(const QString &modelKey, const QString &modelName,
                             const QString &state, const QString &details);
};

inline GenerationService::GenerationService(QObject *parent) :QObject(parent){
}


#endif //WRITE_GENERATIONSERVICE_H
