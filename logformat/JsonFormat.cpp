#include <QJsonDocument>
#include <QSqlRecord>
#include "JsonFormat.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.logformat.jsonformat");

void JsonFormat::exportStart()
{
    FCT_IDENTIFICATION;

    data = QJsonArray();
}

void JsonFormat::exportContact(const QSqlRecord& record, QMap<QString, QString>*applTags)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<record;

    contact = QJsonObject();
    writeSQLRecord(record, applTags);
    data.append(contact);
}

void JsonFormat::exportEnd()
{
    FCT_IDENTIFICATION;

    QJsonObject msg;
    QJsonObject headerData;
    headerData["adif_ver"] = ADIF_VERSION_STRING;
    headerData["programid"] = PROGRAMID_STRING;
    headerData["programversion"] = VERSION;
    headerData["created_timestamp"] = QDateTime::currentDateTimeUtc().toString("yyyyMMdd hhmmss");
    msg["header"] = headerData;
    msg["records"] = data;
    QJsonDocument doc(msg);
    stream << doc.toJson();
}

void JsonFormat::writeField(const QString &name,
                            bool presenceCondition,
                            const QString &value,
                            const QString &type)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<< name
                                << presenceCondition
                                << value
                                << type;

    const QString outputValue(normalizeLineBreaks(value,
                                                  preserveFieldLineBreaks(name, type),
                                                  QStringLiteral("\n")));

    if (outputValue.isEmpty() || !presenceCondition) return;

    contact[name] = outputValue;
}

bool JsonFormat::importNext(QSqlRecord&)
{
    return false;
}
