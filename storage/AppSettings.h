#ifndef WRITE_APPSETTINGS_H
#define WRITE_APPSETTINGS_H

#include <QSettings>

namespace AppSettings {
inline QSettings open() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     "WriterStudio", "WriterStudio");
}
}

#endif // WRITE_APPSETTINGS_H
