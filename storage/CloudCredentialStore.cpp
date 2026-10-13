#include "CloudCredentialStore.h"
#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#include <string>
#endif

namespace {
#ifdef Q_OS_WIN
std::wstring targetName(const QString &profileId) {
    return (QStringLiteral("WriterStudio/cloud/") + profileId).toStdWString();
}
#endif
}

bool CloudCredentialStore::load(const QString &profileId,
                                QString &apiKey, QString &error) {
    error.clear();
    apiKey.clear();
#ifdef Q_OS_WIN
    const std::wstring target = targetName(profileId);
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        const DWORD code = GetLastError();
        if (code == ERROR_NOT_FOUND) return true;
        error = QStringLiteral("读取 Windows 凭据失败（错误码 %1）。").arg(code);
        return false;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                           int(credential->CredentialBlobSize));
    apiKey = QString::fromUtf8(bytes);
    CredFree(credential);
    return true;
#else
    Q_UNUSED(profileId);
    error = QStringLiteral("当前平台暂不支持保存云端 API Key。");
    return false;
#endif
}

bool CloudCredentialStore::save(const QString &profileId,
                                const QString &apiKey, QString &error) {
    error.clear();
    if (profileId.isEmpty() || apiKey.trimmed().isEmpty()) {
        error = QStringLiteral("配置 ID 和 API Key 不能为空。");
        return false;
    }
#ifdef Q_OS_WIN
    const std::wstring target = targetName(profileId);
    const QByteArray bytes = apiKey.trimmed().toUtf8();
    if (bytes.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        error = QStringLiteral("API Key 超过 Windows 凭据管理器允许的长度。");
        return false;
    }
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize = DWORD(bytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(bytes.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"WriterStudio");
    if (!CredWriteW(&credential, 0)) {
        error = QStringLiteral("保存 Windows 凭据失败（错误码 %1）。")
                    .arg(GetLastError());
        return false;
    }
    return true;
#else
    Q_UNUSED(apiKey);
    error = QStringLiteral("当前平台暂不支持保存云端 API Key。");
    return false;
#endif
}

bool CloudCredentialStore::remove(const QString &profileId, QString &error) {
    error.clear();
#ifdef Q_OS_WIN
    const std::wstring target = targetName(profileId);
    if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) ||
        GetLastError() == ERROR_NOT_FOUND)
        return true;
    error = QStringLiteral("删除 Windows 凭据失败（错误码 %1）。")
                .arg(GetLastError());
    return false;
#else
    Q_UNUSED(profileId);
    error = QStringLiteral("当前平台暂不支持管理云端 API Key。");
    return false;
#endif
}
