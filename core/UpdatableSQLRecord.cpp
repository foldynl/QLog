#include <QSqlField>

#include "UpdatableSQLRecord.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.updatableqslrecord");

UpdatableSQLRecord::UpdatableSQLRecord(int interval, QObject *parent)
    : QObject{parent},
      interval(interval)
{
    FCT_IDENTIFICATION;

    connect(&timer, &QTimer::timeout, this, &UpdatableSQLRecord::emitStoreRecord);
}

UpdatableSQLRecord::~UpdatableSQLRecord()
{
    FCT_IDENTIFICATION;

    timer.stop();
}

void UpdatableSQLRecord::updateRecord(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( internalRecord.isEmpty() )
    {
        internalRecord = record;
        qCDebug(runtime) << "Record is empty, starting timer" << interval;
        timer.start(interval);
        return;
    }
    else if ( !matchQSO(QSOMatchingType, record) )
    {
        qCDebug(runtime) << "Records do not match";
        timer.stop();
        emitStoreRecord();
        internalRecord = record;
    }
    else
    {
        qCDebug(runtime) << "Records match";

        timer.stop();
        for ( int i = 0; i < record.count(); ++i )
        {
            const QString &fieldName = record.fieldName(i);

            if ( !internalRecord.contains(fieldName) )
                internalRecord.append(record.field(i));
            else if ( !record.value(i).toString().isEmpty()
                      && internalRecord.value(fieldName).toString().isEmpty() )
                internalRecord.setValue(fieldName, record.value(i));
        }
    }

    qCDebug(runtime) << "starting timer" << interval;
    timer.start(interval);
}

void UpdatableSQLRecord::emitStoreRecord()
{
    FCT_IDENTIFICATION;

    timer.stop();

    if ( internalRecord.isEmpty() )
        return;

    qCDebug(runtime) << "emitting record";
    emit recordReady(internalRecord);
    internalRecord.clear();
}

bool UpdatableSQLRecord::matchQSO(const MatchingType matchingType,
                                  const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QStringList &fields = matchingFields.value(matchingType);

    for ( const QString &fieldName : fields )
    {
        qCDebug(runtime) << "compare field name " << fieldName
                         << "In value" << internalRecord.value(fieldName)
                         << "New value" << record.value(fieldName);

        if ( internalRecord.value(fieldName) != record.value(fieldName))
            return false;
    }

    return true;
}
