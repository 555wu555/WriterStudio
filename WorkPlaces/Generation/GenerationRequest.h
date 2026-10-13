#ifndef WRITE_GENERATIONREQUEST_H
#define WRITE_GENERATIONREQUEST_H

#include <QString>
#include <QtGlobal>

// 一次生成请求的数据；不负责执行请求，也不需要继承 QObject。
struct GenerationRequest {
    quint64 requestId = 0; // 区分先后请求，防止迟到的结果覆盖新建议
    QString instruction; // 用户希望生成服务做什么
    QString context;     // 供生成服务参考的作品内容
};

#endif // WRITE_GENERATIONREQUEST_H
