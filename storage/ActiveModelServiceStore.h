#ifndef WRITE_ACTIVEMODELSERVICESTORE_H
#define WRITE_ACTIVEMODELSERVICESTORE_H

#include <QString>

enum class ActiveModelService { Ollama, Cloud };

namespace ActiveModelServiceStore {
bool load(ActiveModelService &service, QString &error);
bool save(ActiveModelService service, QString &error);
}

#endif // WRITE_ACTIVEMODELSERVICESTORE_H
