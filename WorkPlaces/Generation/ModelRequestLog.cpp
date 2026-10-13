#include "ModelRequestLog.h"

ModelRequestLog &ModelRequestLog::instance() {
    static ModelRequestLog log;
    return log;
}

quint64 ModelRequestLog::begin(const QString &modelName,
                              const QString &prompt,
                              const QByteArray &requestJson, bool retry) {
    ModelRequestEntry entry;
    entry.id = ++nextId_;
    entry.startedAt = QDateTime::currentDateTime();
    entry.modelName = modelName;
    entry.prompt = prompt;
    entry.requestJson = requestJson;
    entry.status = retry ? QStringLiteral("重试中") : QStringLiteral("请求中");
    entries_.prepend(entry);
    if (entries_.size() > 30) entries_.removeLast();
    emit changed();
    return entry.id;
}

void ModelRequestLog::finish(quint64 id, const QByteArray &responseJson,
                             const QString &status) {
    for (ModelRequestEntry &entry : entries_) {
        if (entry.id != id) continue;
        entry.responseJson = responseJson;
        entry.status = status;
        emit changed();
        return;
    }
}

void ModelRequestLog::clear() {
    entries_.clear();
    emit changed();
}
