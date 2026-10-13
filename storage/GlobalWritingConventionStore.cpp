#include "GlobalWritingConventionStore.h"
#include "AppSettings.h"

namespace {
constexpr auto KeyPrefix = "writingConvention/";
}

bool GlobalWritingConventionStore::load(WritingConvention &convention, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    WritingConvention loaded;
    loaded.viewpoint = settings.value(QString::fromLatin1(KeyPrefix) + "viewpoint").toString();
    loaded.tense = settings.value(QString::fromLatin1(KeyPrefix) + "tense").toString();
    loaded.style = settings.value(QString::fromLatin1(KeyPrefix) + "style").toString();
    loaded.rhythm = settings.value(QString::fromLatin1(KeyPrefix) + "rhythm").toString();
    loaded.constraints = settings.value(QString::fromLatin1(KeyPrefix) + "constraints").toString();
    if (settings.status() != QSettings::NoError) {
        error = "读取全局写作约定失败";
        return false;
    }
    convention = loaded;
    return true;
}

bool GlobalWritingConventionStore::save(const WritingConvention &convention, QString &error) {
    error.clear();
    QSettings settings = AppSettings::open();
    settings.setValue(QString::fromLatin1(KeyPrefix) + "viewpoint", convention.viewpoint);
    settings.setValue(QString::fromLatin1(KeyPrefix) + "tense", convention.tense);
    settings.setValue(QString::fromLatin1(KeyPrefix) + "style", convention.style);
    settings.setValue(QString::fromLatin1(KeyPrefix) + "rhythm", convention.rhythm);
    settings.setValue(QString::fromLatin1(KeyPrefix) + "constraints", convention.constraints);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = "保存全局写作约定失败";
        return false;
    }
    return true;
}
