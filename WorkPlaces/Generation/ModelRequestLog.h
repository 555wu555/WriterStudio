#ifndef WRITE_MODELREQUESTLOG_H
#define WRITE_MODELREQUESTLOG_H

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

struct ModelRequestEntry {
    quint64 id = 0;
    QDateTime startedAt;
    QString modelName;
    QString prompt;
    QByteArray requestJson;
    QByteArray responseJson;
    QString status;
};

// 仅在本次运行的内存中保留最近请求；正文和模型回复不会写入磁盘日志。
class ModelRequestLog : public QObject {
    Q_OBJECT
public:
    static ModelRequestLog &instance();
    quint64 begin(const QString &modelName, const QString &prompt,
                  const QByteArray &requestJson, bool retry);
    void finish(quint64 id, const QByteArray &responseJson,
                const QString &status);
    void clear();
    const QVector<ModelRequestEntry> &entries() const { return entries_; }

signals:
    void changed();

private:
    ModelRequestLog() = default;
    QVector<ModelRequestEntry> entries_;
    quint64 nextId_ = 0;
};

#endif // WRITE_MODELREQUESTLOG_H
