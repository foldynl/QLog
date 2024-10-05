#include <QSqlRecord>
#include "CSVFormat.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.logformat.csvformat");

CSVFormat::CSVFormat(QTextStream &stream) :
    AdxFormat(stream),
    delimiter(',')
{
    FCT_IDENTIFICATION;
}

void CSVFormat::exportStart()
{
    FCT_IDENTIFICATION;
}

void CSVFormat::exportContact(const QSqlRecord &record, QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << record;

    currectRecord.clear();
    writeSQLRecord(record, applTags);
    exportedRecords << currectRecord;
}

void CSVFormat::exportEnd()
{
    FCT_IDENTIFICATION;

    // Print Header - QMap sorts keys automatically
    const QList<QString> headerKeys(header.keys());
    QStringList row;

    for ( const QString& headerField : headerKeys )
    {
        row << headerField;
    }

    stream << row.join(delimiter) << "\n";

    // Normalize and print exported records
    for ( const QHash<QString, QString> &record : static_cast<const QList<QHash<QString, QString>>&>(exportedRecords) )
    {
        row.clear();

        for ( const QString& headerField : headerKeys )
        {
            row << record.value(headerField);
        }
        stream << row.join(delimiter) << "\n";
    }
}

void CSVFormat::setDelimiter(const QChar &inDelimiter)
{
    FCT_IDENTIFICATION;

    delimiter = inDelimiter;
}

void CSVFormat::writeField(const QString &name,
                           bool presenceCondition,
                           const QString &value,
                           const QString &type)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< name
                                << presenceCondition
                                << value
                                << type;

    if ( !presenceCondition ) return;

    header[name] = 0; // using QMap only due to ordering and uniq, number is not used at this moment;
    currectRecord[name] = csvStringValue(value);
}

QString CSVFormat::csvStringValue(const QString &value)
{
    FCT_IDENTIFICATION;

    return ((value.contains(delimiter))? "\"" + value + "\"" : value);
}
