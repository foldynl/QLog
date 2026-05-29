#include <QSqlRecord>
#include <QtXml>
#include "logformat/AdxFormat.h"
#include "logformat/AdiFormat.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.logformat.adxformat");

AdxFormat::AdxFormat(QTextStream &stream) :
    AdiFormat(stream, false),
    writer(nullptr),
    reader(nullptr)
{
    FCT_IDENTIFICATION;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
}

void AdxFormat::importStart()
{
    FCT_IDENTIFICATION;

    reader = new QXmlStreamReader(stream.device());

    while ( reader->readNextStartElement() )
    {
        qCDebug(runtime)<<reader->name();
        if ( reader->name() == QString("ADX") )
        {
            while ( reader->readNextStartElement() )
            {
                qCDebug(runtime)<<reader->name();
                if ( reader->name() == QString("HEADER") )
                {
                    reader->skipCurrentElement();
                }
                else if ( reader->name() == QString("RECORDS") )
                {
                    qCDebug(runtime)<<"records found";
                    /* header is loaded, QLog is currently in Records sections
                       which is loaded by importNext procedure */
                    return;
                }
            }
        }
        else
        {
            reader->skipCurrentElement();
        }
    }
}

void AdxFormat::importEnd()
{
    FCT_IDENTIFICATION;

    if ( reader )
    {
        delete reader;
        reader = nullptr;
    }
}

void AdxFormat::exportStart()
{
    FCT_IDENTIFICATION;

    QString date = QDateTime::currentDateTimeUtc().toString("yyyyMMdd hhmmss");

    writer = new QXmlStreamWriter(stream.device());

    writer->setAutoFormatting(true);

    writer->writeStartDocument();
    writer->writeStartElement("ADX");

    writer->writeStartElement("HEADER");
    writer->writeTextElement("ADIF_VER", ADIF_VERSION_STRING);
    writer->writeTextElement("PROGRAMID", PROGRAMID_STRING);
    writer->writeTextElement("PROGRAMVERSION", VERSION);
    writer->writeTextElement("CREATED_TIMESTAMP", date);
    writer->writeEndElement();

    writer->writeStartElement("RECORDS");
}

void AdxFormat::exportEnd()
{
    FCT_IDENTIFICATION;

    if ( writer )
    {
        writer->writeEndDocument();
        delete writer;
        writer = nullptr;
    }
}

void AdxFormat::exportContact(const QSqlRecord& record, QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<record;

    if ( ! writer )
    {
        qWarning() << "ADX Writer is not ready";
        return;
    }

    writer->writeStartElement("RECORD");

    QSqlRecord exportRecord(record);
    normalizeGridFields(exportRecord);

    writeSQLRecord(exportRecord, applTags);

    writer->writeEndElement();
}

QString AdxFormat::applicationFieldKey(const QString &programId,
                                       const QString &fieldName)
{
    return QStringLiteral("app_%1_%2").arg(programId, fieldName).toLower();
}

QString AdxFormat::attributeValue(const QXmlStreamAttributes &attributes,
                                  const QString &name)
{
    for (const QXmlStreamAttribute &attribute : attributes)
    {
        if ( attribute.name().compare(name, Qt::CaseInsensitive) == 0 )
            return attribute.value().toString();
    }

    return QString();
}

bool AdxFormat::splitApplicationFieldName(const QString &name,
                                          QString &programId,
                                          QString &fieldName)
{
    const QString prefix(QStringLiteral("app_"));
    if ( !name.startsWith(prefix, Qt::CaseInsensitive) )
        return false;

    const int fieldStart = name.indexOf(QLatin1Char('_'), prefix.length());
    if ( fieldStart <= prefix.length() )
        return false;

    programId = name.mid(prefix.length(), fieldStart - prefix.length());
    fieldName = name.mid(fieldStart + 1);

    return !programId.isEmpty() && !fieldName.isEmpty();
}

bool AdxFormat::writeApplicationField(const QString &name,
                                      bool presenceCondition,
                                      const QString &value,
                                      const QString &type)
{
    QString programId;
    QString fieldName;
    if ( !splitApplicationFieldName(name, programId, fieldName) )
        return false;

    if ( !presenceCondition )
        return true;

    const QString outputType = type.isEmpty() ? QStringLiteral("M") : type.toUpper();
    const QString outputValue(normalizeLineBreaks(value,
                                                  preserveFieldLineBreaks(name, outputType),
                                                  QStringLiteral("\n")));

    if ( outputValue.isEmpty() )
        return true;

    writer->writeStartElement("APP");
    writer->writeAttribute("PROGRAMID", programId);
    writer->writeAttribute("FIELDNAME", fieldName);
    writer->writeAttribute("TYPE", outputType);
    writer->writeCharacters(outputValue);
    writer->writeEndElement();

    return true;
}

void AdxFormat::writeField(const QString &name,
                           bool presenceCondition,
                           const QString &value,
                           const QString &type)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< name
                                << presenceCondition
                                << value
                                << type;

    if ( writeApplicationField(name, presenceCondition, value, type) )
        return;

    const QString outputValue(normalizeLineBreaks(value,
                                                  preserveFieldLineBreaks(name, type),
                                                  QStringLiteral("\n")));

    if (outputValue.isEmpty() || !presenceCondition ) return;

    writer->writeTextElement(name.toUpper(), outputValue);
}

void AdxFormat::writeSQLRecord(const QSqlRecord &record, QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    AdiFormat::writeSQLRecord(record, applTags);

    // Add _INTL fields

    const QStringList &fieldMappingList = fieldname2INTLNameMapping.values();
    for ( const QString& value :  fieldMappingList )
    {
        const QVariant &tmp = record.value(value);
        writeField(value, tmp.isValid(), tmp.toString());
    }
}

bool AdxFormat::readContact(QVariantMap & contact)
{
    FCT_IDENTIFICATION;

    while ( !reader->atEnd() )
    {
        reader->readNextStartElement();

        qCDebug(runtime)<<reader->name();

        if ( reader->name() == QString("RECORDS") && reader->isEndElement() )
        {
            qCDebug(runtime)<<"End Records Element";
            return false;
        }
        if ( reader->name() == QString("RECORD") )
        {
            while (reader->readNextStartElement() )
            {
                qCDebug(runtime)<<"adding element " << reader->name();
                if ( reader->name().compare(QStringLiteral("APP"), Qt::CaseInsensitive) == 0 )
                {
                    const QXmlStreamAttributes attributes = reader->attributes();
                    const QString programId = attributeValue(attributes, QStringLiteral("PROGRAMID"));
                    const QString fieldName = attributeValue(attributes, QStringLiteral("FIELDNAME"));
                    const QString type = attributeValue(attributes, QStringLiteral("TYPE"));
                    const QString value = reader->readElementText();

                    if ( !programId.isEmpty() && !fieldName.isEmpty() )
                    {
                        QVariantMap appField;
                        appField.insert(QStringLiteral("value"), value);
                        if ( !type.isEmpty() )
                            appField.insert(QStringLiteral("type"), type.toUpper());
                        contact[applicationFieldKey(programId, fieldName)] = appField;
                    }
                    else
                    {
                        contact[reader->name().toLatin1().toLower()] = QVariant(value);
                    }
                }
                else
                {
                    contact[reader->name().toLatin1().toLower()] = QVariant(reader->readElementText());
                }
            }
            return true;
        }
        else
        {
            reader->skipCurrentElement();
        }
    }
    return false;
}
