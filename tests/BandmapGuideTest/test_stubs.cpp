#include <QString>

#include "core/LogDatabase.h"

QString LogDatabase::currentPlatformId()
{
    return QStringLiteral("TestPlatform");
}
