#include "data/Data.h"
#include "logformat/LogFormat.h"

LogFormat::LogFormat(QTextStream &stream) :
    QObject(nullptr),
    stream(stream),
    exportedFields(QStringLiteral("*")),
    duplicateQSOFunc(nullptr)
{
    defaults = nullptr;
}

LogFormat::~LogFormat() = default;

void LogFormat::setDefaults(QMap<QString, QString> &defaults)
{
    this->defaults = &defaults;
}

Data::Data(QObject *parent) :
    QObject(parent)
{
}

Data::~Data() = default;

QPair<QString, QString> Data::legacyMode(const QString &)
{
    return {};
}

void Data::invalidateDXCCStatusCache(const QSqlRecord &)
{
}

void Data::invalidateSetOfDXCCStatusCache(const QSet<uint> &)
{
}

void Data::clearDXCCStatusCache()
{
}
