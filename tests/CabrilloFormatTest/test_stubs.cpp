#include "data/Data.h"
#include "logformat/LogFormat.h"

LogFormat::LogFormat(QTextStream &stream) :
    QObject(nullptr),
    stream(stream),
    defaults(nullptr),
    exportedFields(QStringLiteral("*")),
    duplicateQSOFunc(nullptr)
{
}

LogFormat::~LogFormat() = default;

Data::Data(QObject *parent) :
    QObject(parent)
{
}

Data::~Data() = default;

void Data::invalidateDXCCStatusCache(const QSqlRecord &)
{
}

void Data::invalidateSetOfDXCCStatusCache(const QSet<uint> &)
{
}

void Data::clearDXCCStatusCache()
{
}
