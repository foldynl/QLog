#include <QtTest>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlField>
#include <QSqlRecord>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "logformat/AdxFormat.h"

class AdxFormatTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exportStartWritesAdxHeader();
    void writeSqlRecordExportsApplicationTags();
    void writeSqlRecordExportsRawFieldsFiltersInvalidNames();
    void exportContactNormalizesGridAndTerminatesRecord();
    void importNextMapsAdxFieldsAndStoresUnknownFields();
    void importNextStoresZeroLengthApplicationFields();
    void importNextAppliesDefaultsForMissingFields();
    void importNextKeepsImportedValuesOverDefaults();
    void importNextFillsIntlCompanionFields();
    void importNextAppliesDefaultsBeforeIntlCompanionFields();

private:
    static void appendField(QSqlRecord &record,
                            const QString &name,
                            const QVariant &value);
    static QByteArray xmlRecord(const QByteArray &recordBody);
    static QByteArray exportRecord(const QSqlRecord &record,
                                   QMap<QString, QString> *applTags = nullptr);
};

void AdxFormatTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void AdxFormatTest::appendField(QSqlRecord &record,
                                const QString &name,
                                const QVariant &value)
{
    QSqlField field(name, value.type());
    field.setValue(value);
    record.append(field);
}

QByteArray AdxFormatTest::xmlRecord(const QByteArray &recordBody)
{
    QByteArray input("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    input += "<ADX><HEADER><ADIF_VER>3.1.7</ADIF_VER></HEADER><RECORDS><RECORD>";
    input += recordBody;
    input += "</RECORD></RECORDS></ADX>";
    return input;
}

QByteArray AdxFormatTest::exportRecord(const QSqlRecord &record,
                                       QMap<QString, QString> *applTags)
{
    QByteArray output;
    QBuffer buffer(&output);
    if ( !buffer.open(QIODevice::WriteOnly | QIODevice::Text) )
        return output;

    QTextStream stream(&buffer);
    AdxFormat format(stream);

    format.exportStart();
    format.exportContact(record, applTags);
    format.exportEnd();
    stream.flush();

    return output;
}

void AdxFormatTest::exportStartWritesAdxHeader()
{
    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream stream(&buffer);
    AdxFormat format(stream);
    format.exportStart();
    format.exportEnd();
    stream.flush();

    QVERIFY(output.startsWith("<?xml"));
    QVERIFY(output.contains("<ADX>"));
    QVERIFY(output.contains("<HEADER>"));
    QVERIFY(output.contains("<RECORDS/>"));

    QMap<QString, QString> headerValues;
    QXmlStreamReader reader(output);
    while ( !reader.atEnd() )
    {
        reader.readNext();
        if ( reader.isStartElement()
             && (reader.name() == QStringLiteral("ADIF_VER")
                 || reader.name() == QStringLiteral("PROGRAMID")
                 || reader.name() == QStringLiteral("PROGRAMVERSION")
                 || reader.name() == QStringLiteral("CREATED_TIMESTAMP")) )
        {
            headerValues.insert(reader.name().toString(), reader.readElementText());
        }
    }
    QVERIFY(!reader.hasError());

    QCOMPARE(headerValues.value(QStringLiteral("ADIF_VER")), QStringLiteral("3.1.7"));
    QCOMPARE(headerValues.value(QStringLiteral("PROGRAMID")), QStringLiteral("QLog"));
    QCOMPARE(headerValues.value(QStringLiteral("PROGRAMVERSION")), QStringLiteral("test"));
    QCOMPARE(headerValues.value(QStringLiteral("CREATED_TIMESTAMP")).length(), 15);
}

void AdxFormatTest::writeSqlRecordExportsApplicationTags()
{
    QSqlRecord record;
    QJsonObject fields;
    QJsonObject lotwModeGroup;
    lotwModeGroup.insert(QStringLiteral("value"), QStringLiteral("DATA"));
    lotwModeGroup.insert(QStringLiteral("type"), QStringLiteral("S"));
    fields.insert(QStringLiteral("APP_LoTW_MODEGROUP"), lotwModeGroup);
    appendField(record,
                QStringLiteral("fields"),
                QString(QJsonDocument(fields).toJson(QJsonDocument::Compact)));

    QMap<QString, QString> applTags;
    applTags.insert(QStringLiteral("APP_QLOG_NOTE"), QStringLiteral("Line1\nLine2"));

    const QByteArray output = exportRecord(record, &applTags);

    QVERIFY(!output.contains("APP_QLOG_NOTE"));
    QVERIFY(!output.contains("APP_LoTW_MODEGROUP"));

    QMap<QString, QString> values;
    QMap<QString, QString> types;
    QXmlStreamReader reader(output);
    while ( !reader.atEnd() )
    {
        reader.readNext();
        if ( reader.isStartElement() && reader.name() == QStringLiteral("APP") )
        {
            const QXmlStreamAttributes attributes = reader.attributes();
            const QString key = attributes.value(QStringLiteral("PROGRAMID")).toString()
                                + QLatin1Char('/')
                                + attributes.value(QStringLiteral("FIELDNAME")).toString();
            types.insert(key, attributes.value(QStringLiteral("TYPE")).toString());
            values.insert(key, reader.readElementText());
        }
    }
    QVERIFY(!reader.hasError());

    QCOMPARE(values.value(QStringLiteral("QLOG/NOTE")), QStringLiteral("Line1\nLine2"));
    QCOMPARE(types.value(QStringLiteral("QLOG/NOTE")), QStringLiteral("M"));
    QCOMPARE(values.value(QStringLiteral("LoTW/MODEGROUP")), QStringLiteral("DATA"));
    QCOMPARE(types.value(QStringLiteral("LoTW/MODEGROUP")), QStringLiteral("S"));
}

void AdxFormatTest::writeSqlRecordExportsRawFieldsFiltersInvalidNames()
{
    QSqlRecord record;
    QJsonObject fields;
    fields.insert(QStringLiteral("UNKNOWN_FIELD"), QStringLiteral("ok"));
    fields.insert(QStringLiteral("APP_QLOG_RAW"), QStringLiteral("Line1\nLine2"));
    fields.insert(QStringLiteral("BAD-NAME"), QStringLiteral("dash"));
    fields.insert(QStringLiteral("1BAD"), QStringLiteral("digit"));
    fields.insert(QStringLiteral("xmlBAD"), QStringLiteral("xml"));
    fields.insert(QStringLiteral("BAD NAME"), QStringLiteral("space"));
    fields.insert(QStringLiteral("BAD<NAME"), QStringLiteral("angle"));
    fields.insert(QStringLiteral("APP_BAD"), QStringLiteral("shortapp"));
    appendField(record,
                QStringLiteral("fields"),
                QString(QJsonDocument(fields).toJson(QJsonDocument::Compact)));

    const QByteArray output = exportRecord(record);

    QXmlStreamReader reader(output);
    QMap<QString, QString> values;
    QMap<QString, QString> appValues;
    while ( !reader.atEnd() )
    {
        reader.readNext();
        if ( reader.isStartElement() && reader.name() == QStringLiteral("UNKNOWN_FIELD") )
        {
            values.insert(reader.name().toString(), reader.readElementText());
        }
        else if ( reader.isStartElement() && reader.name() == QStringLiteral("APP") )
        {
            const QXmlStreamAttributes attributes = reader.attributes();
            const QString key = attributes.value(QStringLiteral("PROGRAMID")).toString()
                                + QLatin1Char('/')
                                + attributes.value(QStringLiteral("FIELDNAME")).toString();
            appValues.insert(key, reader.readElementText());
        }
    }
    QVERIFY(!reader.hasError());

    QCOMPARE(values.value(QStringLiteral("UNKNOWN_FIELD")), QStringLiteral("ok"));
    QCOMPARE(appValues.value(QStringLiteral("QLOG/RAW")), QStringLiteral("Line1\nLine2"));
    QVERIFY(!output.contains("dash"));
    QVERIFY(!output.contains("digit"));
    QVERIFY(!output.contains(">xml<"));
    QVERIFY(!output.contains("space"));
    QVERIFY(!output.contains("angle"));
    QVERIFY(!output.contains("shortapp"));
}

void AdxFormatTest::exportContactNormalizesGridAndTerminatesRecord()
{
    QSqlRecord record;
    appendField(record, QStringLiteral("gridsquare"), QStringLiteral("jo70aa12bb"));
    appendField(record, QStringLiteral("gridsquare_ext"), QString());

    const QByteArray output = exportRecord(record);

    QVERIFY(output.contains("<GRIDSQUARE>JO70AA12</GRIDSQUARE>"));
    QVERIFY(output.contains("<GRIDSQUARE_EXT>BB</GRIDSQUARE_EXT>"));
    QVERIFY(!output.contains("JO70AA12BB"));
    QVERIFY(output.contains("</RECORD>"));
    QVERIFY(output.trimmed().endsWith("</ADX>"));
}

void AdxFormatTest::importNextMapsAdxFieldsAndStoresUnknownFields()
{
    QByteArray input = xmlRecord(
        "<CALL>ok1aa</CALL>"
        "<BAND>20M</BAND>"
        "<CONT>eu</CONT>"
        "<GRIDSQUARE>jo70aa12bb</GRIDSQUARE>"
        "<QSO_DATE>20260528</QSO_DATE>"
        "<TIME_ON>123456</TIME_ON>"
        "<TIME_OFF>123500</TIME_OFF>"
        "<QSL_RCVD>y</QSL_RCVD>"
        "<QSL_SENT>q</QSL_SENT>"
        "<ANT_PATH>s</ANT_PATH>"
        "<APP PROGRAMID=\"QLOG\" FIELDNAME=\"TEST\" TYPE=\"M\">abc</APP>"
        "<UNKNOWN_FIELD>xyz</UNKNOWN_FIELD>");

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    AdxFormat format(stream);
    format.importStart();

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("band"), QString());
    appendField(record, QStringLiteral("cont"), QString());
    appendField(record, QStringLiteral("gridsquare"), QString());
    appendField(record, QStringLiteral("gridsquare_ext"), QString());
    appendField(record, QStringLiteral("start_time"), QDateTime());
    appendField(record, QStringLiteral("end_time"), QDateTime());
    appendField(record, QStringLiteral("qsl_rcvd"), QString());
    appendField(record, QStringLiteral("qsl_sent"), QString());
    appendField(record, QStringLiteral("ant_path"), QString());
    appendField(record, QStringLiteral("fields"), QString());

    QVERIFY(format.importNext(record));
    format.importEnd();

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("band")).toString(), QStringLiteral("20m"));
    QCOMPARE(record.value(QStringLiteral("cont")).toString(), QStringLiteral("EU"));
    QCOMPARE(record.value(QStringLiteral("gridsquare")).toString(), QStringLiteral("JO70AA12"));
    QCOMPARE(record.value(QStringLiteral("gridsquare_ext")).toString(), QStringLiteral("BB"));
    QCOMPARE(record.value(QStringLiteral("qsl_rcvd")).toString(), QStringLiteral("Y"));
    QCOMPARE(record.value(QStringLiteral("qsl_sent")).toString(), QStringLiteral("Q"));
    QCOMPARE(record.value(QStringLiteral("ant_path")).toString(), QStringLiteral("S"));
    QCOMPARE(record.value(QStringLiteral("start_time")).toDateTime(),
             QDateTime(QDate(2026, 5, 28), QTime(12, 34, 56), QTimeZone::utc()));
    QCOMPARE(record.value(QStringLiteral("end_time")).toDateTime(),
             QDateTime(QDate(2026, 5, 28), QTime(12, 35, 0), QTimeZone::utc()));

    const QJsonObject fields = QJsonDocument::fromJson(record.value(QStringLiteral("fields")).toByteArray()).object();
    const QJsonObject appField = fields.value(QStringLiteral("app_qlog_test")).toObject();
    QCOMPARE(appField.value(QStringLiteral("value")).toString(), QStringLiteral("abc"));
    QCOMPARE(appField.value(QStringLiteral("type")).toString(), QStringLiteral("M"));
    QCOMPARE(fields.value(QStringLiteral("unknown_field")).toString(), QStringLiteral("xyz"));
}

void AdxFormatTest::importNextStoresZeroLengthApplicationFields()
{
    QByteArray input = xmlRecord(
        "<CALL>OK1AA</CALL>"
        "<APP PROGRAMID=\"QLOG\" FIELDNAME=\"EMPTY\" TYPE=\"M\"/>");

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    AdxFormat format(stream);
    format.importStart();

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("fields"), QString());

    QVERIFY(format.importNext(record));
    format.importEnd();

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));

    const QJsonObject fields = QJsonDocument::fromJson(record.value(QStringLiteral("fields")).toByteArray()).object();
    const QJsonObject appField = fields.value(QStringLiteral("app_qlog_empty")).toObject();
    QVERIFY(fields.contains(QStringLiteral("app_qlog_empty")));
    QCOMPARE(appField.value(QStringLiteral("value")).toString(), QString());
    QCOMPARE(appField.value(QStringLiteral("type")).toString(), QStringLiteral("M"));
}

void AdxFormatTest::importNextAppliesDefaultsForMissingFields()
{
    QByteArray input = xmlRecord("<CALL>OK1AA</CALL>");

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    AdxFormat format(stream);

    QMap<QString, QString> defaults;
    defaults.insert(QStringLiteral("band"), QStringLiteral("20M"));
    defaults.insert(QStringLiteral("qsl_sent"), QStringLiteral("Q"));
    defaults.insert(QStringLiteral("lotw_qsl_sent"), QStringLiteral("Y"));
    defaults.insert(QStringLiteral("eqsl_qsl_sent"), QStringLiteral("R"));
    defaults.insert(QStringLiteral("dcl_qsl_sent"), QStringLiteral("I"));
    defaults.insert(QStringLiteral("my_rig_intl"), QStringLiteral("Radio"));
    format.setDefaults(defaults);
    format.importStart();

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("band"), QString());
    appendField(record, QStringLiteral("qsl_sent"), QString());
    appendField(record, QStringLiteral("lotw_qsl_sent"), QString());
    appendField(record, QStringLiteral("eqsl_qsl_sent"), QString());
    appendField(record, QStringLiteral("dcl_qsl_sent"), QString());
    appendField(record, QStringLiteral("my_rig"), QString());
    appendField(record, QStringLiteral("my_rig_intl"), QString());

    QVERIFY(format.importNext(record));
    format.importEnd();

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("band")).toString(), QStringLiteral("20m"));
    QCOMPARE(record.value(QStringLiteral("qsl_sent")).toString(), QStringLiteral("Q"));
    QCOMPARE(record.value(QStringLiteral("lotw_qsl_sent")).toString(), QStringLiteral("Y"));
    QCOMPARE(record.value(QStringLiteral("eqsl_qsl_sent")).toString(), QStringLiteral("R"));
    QCOMPARE(record.value(QStringLiteral("dcl_qsl_sent")).toString(), QStringLiteral("I"));
    QCOMPARE(record.value(QStringLiteral("my_rig")).toString(), QStringLiteral("Radio"));
    QCOMPARE(record.value(QStringLiteral("my_rig_intl")).toString(), QStringLiteral("Radio"));
}

void AdxFormatTest::importNextKeepsImportedValuesOverDefaults()
{
    QByteArray input = xmlRecord(
        "<CALL>OK1AA</CALL>"
        "<BAND>40M</BAND>"
        "<QSL_SENT>Y</QSL_SENT>"
        "<LOTW_QSL_SENT>N</LOTW_QSL_SENT>"
        "<MY_RIG>Imported rig</MY_RIG>");

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    AdxFormat format(stream);

    QMap<QString, QString> defaults;
    defaults.insert(QStringLiteral("band"), QStringLiteral("20M"));
    defaults.insert(QStringLiteral("qsl_sent"), QStringLiteral("Q"));
    defaults.insert(QStringLiteral("lotw_qsl_sent"), QStringLiteral("Y"));
    defaults.insert(QStringLiteral("my_rig_intl"), QStringLiteral("Default rig"));
    format.setDefaults(defaults);
    format.importStart();

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("band"), QString());
    appendField(record, QStringLiteral("qsl_sent"), QString());
    appendField(record, QStringLiteral("lotw_qsl_sent"), QString());
    appendField(record, QStringLiteral("my_rig"), QString());
    appendField(record, QStringLiteral("my_rig_intl"), QString());

    QVERIFY(format.importNext(record));
    format.importEnd();

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("band")).toString(), QStringLiteral("40m"));
    QCOMPARE(record.value(QStringLiteral("qsl_sent")).toString(), QStringLiteral("Y"));
    QCOMPARE(record.value(QStringLiteral("lotw_qsl_sent")).toString(), QStringLiteral("N"));
    QCOMPARE(record.value(QStringLiteral("my_rig")).toString(), QStringLiteral("Imported rig"));
    QCOMPARE(record.value(QStringLiteral("my_rig_intl")).toString(), QStringLiteral("Imported rig"));
}

void AdxFormatTest::importNextFillsIntlCompanionFields()
{
    QByteArray input = xmlRecord(
        "<ADDRESS>Line 1\nLine 2</ADDRESS>"
        "<COMMENT>Plain comment</COMMENT>"
        "<NOTES_INTL>Caf\xc3\xa9\nQTH</NOTES_INTL>");

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    AdxFormat format(stream);
    format.importStart();

    QSqlRecord record;
    appendField(record, QStringLiteral("address"), QString());
    appendField(record, QStringLiteral("address_intl"), QString());
    appendField(record, QStringLiteral("comment"), QString());
    appendField(record, QStringLiteral("comment_intl"), QString());
    appendField(record, QStringLiteral("notes"), QString());
    appendField(record, QStringLiteral("notes_intl"), QString());

    QVERIFY(format.importNext(record));
    format.importEnd();

    QCOMPARE(record.value(QStringLiteral("address")).toString(), QStringLiteral("Line 1\nLine 2"));
    QCOMPARE(record.value(QStringLiteral("address_intl")).toString(), QStringLiteral("Line 1\nLine 2"));
    QCOMPARE(record.value(QStringLiteral("comment")).toString(), QStringLiteral("Plain comment"));
    QCOMPARE(record.value(QStringLiteral("comment_intl")).toString(), QStringLiteral("Plain comment"));
    QCOMPARE(record.value(QStringLiteral("notes")).toString(), QStringLiteral("Cafe\nQTH"));
    QCOMPARE(record.value(QStringLiteral("notes_intl")).toString(), QString::fromUtf8("Caf\xc3\xa9\nQTH"));
}

void AdxFormatTest::importNextAppliesDefaultsBeforeIntlCompanionFields()
{
    QByteArray input = xmlRecord("<CALL>OK1AA</CALL>");

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    AdxFormat format(stream);

    QMap<QString, QString> defaults;
    defaults.insert(QStringLiteral("notes_intl"), QString::fromUtf8("Caf\xc3\xa9\nQTH"));
    format.setDefaults(defaults);
    format.importStart();

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("notes"), QString());
    appendField(record, QStringLiteral("notes_intl"), QString());

    QVERIFY(format.importNext(record));
    format.importEnd();

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("notes")).toString(), QStringLiteral("Cafe\nQTH"));
    QCOMPARE(record.value(QStringLiteral("notes_intl")).toString(), QString::fromUtf8("Caf\xc3\xa9\nQTH"));
}

QTEST_APPLESS_MAIN(AdxFormatTest)

#include "tst_adxformat.moc"
