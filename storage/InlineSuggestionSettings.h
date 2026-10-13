#ifndef WRITE_INLINESUGGESTIONSETTINGS_H
#define WRITE_INLINESUGGESTIONSETTINGS_H

#include <QString>

namespace InlineSuggestionSettings {
    bool load(bool &enabled, QString &error);
    bool save(bool enabled, QString &error);
}

#endif // WRITE_INLINESUGGESTIONSETTINGS_H
