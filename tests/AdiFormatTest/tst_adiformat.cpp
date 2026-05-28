#include <QtTest>
#include <QBuffer>
#include <QSqlField>
#include <QSqlRecord>

#include "logformat/AdiFormat.h"

class TestAdiFormat : public AdiFormat
{
public:
    explicit TestAdiFormat(QTextStream &stream) :
        AdiFormat(stream)
    {
    }

    bool readContactForTest(QVariantMap &contact)
    {
        return readContact(contact);
    }

    void writeFieldForTest(const QString &name,
                           bool presenceCondition,
                           const QString &value,
                           const QString &type = QString())
    {
        writeField(name, presenceCondition, value, type);
    }

    void writeRecordForTest(const QSqlRecord &record,
                            QMap<QString, QString> *applTags = nullptr)
    {
        writeSQLRecord(record, applTags);
    }
};

class AdiFormatTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void constructorDisablesDeviceTextMode();
    void parserSkipsHeaderAndReadsTypedFields();
    void parserSkipsMixedCaseHeaderTerminator();
    void parserPreservesLengthDelimitedCRLFData();
    void parserReadsMultipleContactsAndMixedCaseEor();
    void writeFieldFormatsTagLengthAndType();
    void writeFieldNormalizesLineBreaksByFieldType();
    void exportStartWritesAdifHeader();
    void writeSqlRecordMapsKnownFields();
    void writeSqlRecordExportsApplicationTags();
    void exportContactNormalizesGridAndTerminatesRecord();
    void importNextMapsAdiFieldsAndStoresUnknownFields();
    void importNextStoresZeroLengthFields();
    void importNextAcceptsLeadingZeroLengthSpecifier();
    void importNextStoresUserDefinedFieldsFromHeader();
    void importNextAppliesDefaultsForMissingFields();
    void importNextKeepsImportedValuesOverDefaults();
    void importNextFillsIntlCompanionFields();
    void importNextAppliesDefaultsBeforeIntlCompanionFields();

private:
    static QByteArray tag(const QByteArray &name,
                          const QByteArray &value,
                          const QByteArray &type = QByteArray());
    static QByteArray writeField(const QString &name,
                                 bool presenceCondition,
                                 const QString &value,
                                 const QString &type = QString());
    static void appendField(QSqlRecord &record,
                            const QString &name,
                            const QVariant &value);
};

void AdiFormatTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

QByteArray AdiFormatTest::tag(const QByteArray &name,
                              const QByteArray &value,
                              const QByteArray &type)
{
    QByteArray output("<");
    output += name;
    output += ':';
    output += QByteArray::number(value.size());
    if ( !type.isEmpty() )
    {
        output += ':';
        output += type;
    }
    output += '>';
    output += value;
    return output;
}

QByteArray AdiFormatTest::writeField(const QString &name,
                                     bool presenceCondition,
                                     const QString &value,
                                     const QString &type)
{
    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);

    QTextStream stream(&buffer);
    TestAdiFormat format(stream);
    format.writeFieldForTest(name, presenceCondition, value, type);
    stream.flush();

    return output;
}

void AdiFormatTest::appendField(QSqlRecord &record,
                                const QString &name,
                                const QVariant &value)
{
    QSqlField field(name, value.type());
    field.setValue(value);
    record.append(field);
}

void AdiFormatTest::constructorDisablesDeviceTextMode()
{
    QByteArray input;
    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(buffer.isTextModeEnabled());

    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QVERIFY(!buffer.isTextModeEnabled());
}

void AdiFormatTest::parserSkipsHeaderAndReadsTypedFields()
{
    QByteArray input("ADIF test header\r\n");
    input += tag("ADIF_VER", "3.1.7");
    input += "<EOH>\r\n";
    input += tag("CALL", "OK1AA");
    input += tag("FREQ", "14.074", "N");
    input += tag("BAND", "20m");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QVariantMap contact;
    QVERIFY(format.readContactForTest(contact));

    QCOMPARE(contact.value(QStringLiteral("call")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(contact.value(QStringLiteral("freq")).toString(), QStringLiteral("14.074"));
    QCOMPARE(contact.value(QStringLiteral("band")).toString(), QStringLiteral("20m"));
    QVERIFY(!contact.contains(QStringLiteral("adif_ver")));
}

void AdiFormatTest::parserSkipsMixedCaseHeaderTerminator()
{
    QByteArray input("ADIF test header\r\n");
    input += tag("ADIF_VER", "3.1.7");
    input += "<eOh>\r\n";
    input += tag("CALL", "OK1AA");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QVariantMap contact;
    QVERIFY(format.readContactForTest(contact));

    QCOMPARE(contact.value(QStringLiteral("call")).toString(), QStringLiteral("OK1AA"));
    QVERIFY(!contact.contains(QStringLiteral("adif_ver")));
}

void AdiFormatTest::parserPreservesLengthDelimitedCRLFData()
{
    const QByteArray address("Line 1\r\nLine 2\r\nLine 3");
    QByteArray input;
    input += tag("ADDRESS", address);
    input += tag("CALL", "OK1AA");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QVariantMap contact;
    QVERIFY(format.readContactForTest(contact));

    QCOMPARE(contact.value(QStringLiteral("address")).toString(),
             QString::fromLatin1(address));
    QCOMPARE(contact.value(QStringLiteral("call")).toString(), QStringLiteral("OK1AA"));
}

void AdiFormatTest::parserReadsMultipleContactsAndMixedCaseEor()
{
    QByteArray input;
    input += tag("CALL", "OK1AA");
    input += "<eOr>\r\n";
    input += tag("CALL", "OK2BB");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QVariantMap contact;
    QVERIFY(format.readContactForTest(contact));
    QCOMPARE(contact.value(QStringLiteral("call")).toString(), QStringLiteral("OK1AA"));

    contact.clear();
    QVERIFY(format.readContactForTest(contact));
    QCOMPARE(contact.value(QStringLiteral("call")).toString(), QStringLiteral("OK2BB"));
}

void AdiFormatTest::writeFieldFormatsTagLengthAndType()
{
    QCOMPARE(writeField(QStringLiteral("FREQ"), true, QStringLiteral("14.074"), QStringLiteral("N")),
             QByteArray("<FREQ:6:N>14.074\n"));
    QCOMPARE(writeField(QStringLiteral("CALL"), true, QString::fromUtf8("\xC5\x98" "eka")),
             QByteArray("<CALL:4>Reka\n"));
    QCOMPARE(writeField(QStringLiteral("CALL"), false, QStringLiteral("OK1AA")),
             QByteArray());
    QCOMPARE(writeField(QStringLiteral("CALL"), true, QString()),
             QByteArray());
}

void AdiFormatTest::writeFieldNormalizesLineBreaksByFieldType()
{
    QCOMPARE(writeField(QStringLiteral("ADDRESS"), true, QStringLiteral("Line1\nLine2")),
             QByteArray("<ADDRESS:12>Line1\r\nLine2\n"));
    QCOMPARE(writeField(QStringLiteral("COMMENT"), true, QStringLiteral("Line1\nLine2")),
             QByteArray("<COMMENT:10>Line1Line2\n"));
    QCOMPARE(writeField(QStringLiteral("APP_QLOG_NOTE"), true, QStringLiteral("Line1\nLine2")),
             QByteArray("<APP_QLOG_NOTE:12>Line1\r\nLine2\n"));
    QCOMPARE(writeField(QStringLiteral("APP_QLOG_NOTE"), true, QStringLiteral("Line1\nLine2"), QStringLiteral("S")),
             QByteArray("<APP_QLOG_NOTE:10:S>Line1Line2\n"));
    QCOMPARE(writeField(QStringLiteral("USERDEF"), true, QStringLiteral("Line1\nLine2"), QStringLiteral("M")),
             QByteArray("<USERDEF:12:M>Line1\r\nLine2\n"));
}

void AdiFormatTest::exportStartWritesAdifHeader()
{
    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream stream(&buffer);
    TestAdiFormat format(stream);
    format.exportStart();
    stream.flush();

    QVERIFY(output.startsWith(" <ADIF_VER:5>3.1.7\n"));
    QVERIFY(output.contains("<PROGRAMID:4>QLog\n"));
    QVERIFY(output.contains("<PROGRAMVERSION:4>test\n"));
    QVERIFY(output.contains("<CREATED_TIMESTAMP:15>"));
    QVERIFY(output.endsWith("<EOH>\n\n"));
}

void AdiFormatTest::writeSqlRecordMapsKnownFields()
{
    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));
    appendField(record, QStringLiteral("band"), QStringLiteral("20M"));
    appendField(record, QStringLiteral("iota"), QStringLiteral("eu-001"));
    appendField(record, QStringLiteral("qsl_sent"), QStringLiteral("N"));
    appendField(record, QStringLiteral("qsl_rcvd"), QStringLiteral("Y"));
    appendField(record, QStringLiteral("notes"), QStringLiteral("Line1\nLine2"));
    appendField(record, QStringLiteral("comment"), QStringLiteral("Line1\nLine2"));
    appendField(record,
                QStringLiteral("start_time"),
                QDateTime(QDate(2026, 5, 28), QTime(12, 34, 56), Qt::UTC));

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    format.writeRecordForTest(record);
    stream.flush();

    QVERIFY(output.contains("<call:5>OK1AA\n"));
    QVERIFY(output.contains("<band:3>20m\n"));
    QVERIFY(output.contains("<iota:6>EU-001\n"));
    QVERIFY(output.contains("<qsl_rcvd:1>Y\n"));
    QVERIFY(!output.contains("qsl_sent"));
    QVERIFY(output.contains("<notes:12>Line1\r\nLine2\n"));
    QVERIFY(output.contains("<comment:10>Line1Line2\n"));
    QVERIFY(output.contains("<qso_date:8>20260528\n"));
    QVERIFY(output.contains("<time_on:6>123456\n"));
}

void AdiFormatTest::writeSqlRecordExportsApplicationTags()
{
    QSqlRecord record;
    QMap<QString, QString> applTags;
    applTags.insert(QStringLiteral("APP_QLOG_NOTE"), QStringLiteral("Line1\nLine2"));
    applTags.insert(QStringLiteral("APP_QLOG_FLAG"), QStringLiteral("Y"));

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    format.writeRecordForTest(record, &applTags);
    stream.flush();

    QVERIFY(output.contains("<APP_QLOG_NOTE:12>Line1\r\nLine2\n"));
    QVERIFY(output.contains("<APP_QLOG_FLAG:1>Y\n"));
}

void AdiFormatTest::exportContactNormalizesGridAndTerminatesRecord()
{
    QSqlRecord record;
    appendField(record, QStringLiteral("gridsquare"), QStringLiteral("jo70aa12bb"));
    appendField(record, QStringLiteral("gridsquare_ext"), QString());

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    format.exportContact(record);
    stream.flush();

    QVERIFY(output.contains("<gridsquare:8>JO70AA12\n"));
    QVERIFY(output.contains("<gridsquare_ext:2>BB\n"));
    QVERIFY(output.endsWith("<eor>\n\n"));
}

void AdiFormatTest::importNextMapsAdiFieldsAndStoresUnknownFields()
{
    QByteArray input;
    input += tag("CALL", "ok1aa");
    input += tag("BAND", "20M");
    input += tag("CONT", "eu");
    input += tag("GRIDSQUARE", "jo70aa12bb");
    input += tag("QSO_DATE", "20260528");
    input += tag("TIME_ON", "123456");
    input += tag("TIME_OFF", "123500");
    input += tag("QSL_RCVD", "y");
    input += tag("QSL_SENT", "q");
    input += tag("ANT_PATH", "s");
    input += tag("APP_QLOG_TEST", "abc");
    input += tag("UNKNOWN_FIELD", "xyz");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

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
    QCOMPARE(fields.value(QStringLiteral("app_qlog_test")).toString(), QStringLiteral("abc"));
    QCOMPARE(fields.value(QStringLiteral("unknown_field")).toString(), QStringLiteral("xyz"));
}

void AdiFormatTest::importNextStoresZeroLengthFields()
{
    QByteArray input;
    input += tag("CALL", "OK1AA");
    input += tag("APP_QLOG_EMPTY", QByteArray());
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("fields"), QString());

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));

    const QJsonObject fields = QJsonDocument::fromJson(record.value(QStringLiteral("fields")).toByteArray()).object();
    QVERIFY(fields.contains(QStringLiteral("app_qlog_empty")));
    QCOMPARE(fields.value(QStringLiteral("app_qlog_empty")).toString(), QString());
}

void AdiFormatTest::importNextAcceptsLeadingZeroLengthSpecifier()
{
    QByteArray input;
    input += "<CALL:005>OK1AA";
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
}

void AdiFormatTest::importNextStoresUserDefinedFieldsFromHeader()
{
    QByteArray input("ADIF user-defined field header\r\n");
    input += tag("USERDEF1", "SweaterSize,{S,M,L}", "E");
    input += "<EOH>\r\n";
    input += tag("CALL", "OK1AA");
    input += tag("SweaterSize", "M");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("fields"), QString());

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));

    const QJsonObject fields = QJsonDocument::fromJson(record.value(QStringLiteral("fields")).toByteArray()).object();
    QCOMPARE(fields.value(QStringLiteral("sweatersize")).toString(), QStringLiteral("M"));
    QVERIFY(!fields.contains(QStringLiteral("userdef1")));
}

void AdiFormatTest::importNextAppliesDefaultsForMissingFields()
{
    QByteArray input;
    input += tag("CALL", "OK1AA");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QMap<QString, QString> defaults;
    defaults.insert(QStringLiteral("band"), QStringLiteral("20M"));
    defaults.insert(QStringLiteral("qsl_sent"), QStringLiteral("Q"));
    defaults.insert(QStringLiteral("lotw_qsl_sent"), QStringLiteral("Y"));
    defaults.insert(QStringLiteral("eqsl_qsl_sent"), QStringLiteral("R"));
    defaults.insert(QStringLiteral("dcl_qsl_sent"), QStringLiteral("I"));
    defaults.insert(QStringLiteral("my_rig_intl"), QString::fromUtf8("R\xC3\xA1" "dio"));
    format.setDefaults(defaults);

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

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("band")).toString(), QStringLiteral("20m"));
    QCOMPARE(record.value(QStringLiteral("qsl_sent")).toString(), QStringLiteral("Q"));
    QCOMPARE(record.value(QStringLiteral("lotw_qsl_sent")).toString(), QStringLiteral("Y"));
    QCOMPARE(record.value(QStringLiteral("eqsl_qsl_sent")).toString(), QStringLiteral("R"));
    QCOMPARE(record.value(QStringLiteral("dcl_qsl_sent")).toString(), QStringLiteral("I"));
    QCOMPARE(record.value(QStringLiteral("my_rig")).toString(), QStringLiteral("Radio"));
    QCOMPARE(record.value(QStringLiteral("my_rig_intl")).toString(), QString::fromUtf8("R\xC3\xA1" "dio"));
}

void AdiFormatTest::importNextKeepsImportedValuesOverDefaults()
{
    QByteArray input;
    input += tag("CALL", "OK1AA");
    input += tag("BAND", "40M");
    input += tag("QSL_SENT", "Y");
    input += tag("LOTW_QSL_SENT", "N");
    input += tag("MY_RIG", "Imported rig");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QMap<QString, QString> defaults;
    defaults.insert(QStringLiteral("band"), QStringLiteral("20M"));
    defaults.insert(QStringLiteral("qsl_sent"), QStringLiteral("Q"));
    defaults.insert(QStringLiteral("lotw_qsl_sent"), QStringLiteral("Y"));
    defaults.insert(QStringLiteral("my_rig_intl"), QStringLiteral("Default rig"));
    format.setDefaults(defaults);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("band"), QString());
    appendField(record, QStringLiteral("qsl_sent"), QString());
    appendField(record, QStringLiteral("lotw_qsl_sent"), QString());
    appendField(record, QStringLiteral("my_rig"), QString());
    appendField(record, QStringLiteral("my_rig_intl"), QString());

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("band")).toString(), QStringLiteral("40m"));
    QCOMPARE(record.value(QStringLiteral("qsl_sent")).toString(), QStringLiteral("Y"));
    QCOMPARE(record.value(QStringLiteral("lotw_qsl_sent")).toString(), QStringLiteral("N"));
    QCOMPARE(record.value(QStringLiteral("my_rig")).toString(), QStringLiteral("Imported rig"));
    QCOMPARE(record.value(QStringLiteral("my_rig_intl")).toString(), QStringLiteral("Imported rig"));
}

void AdiFormatTest::importNextFillsIntlCompanionFields()
{
    const QByteArray address("Line 1\r\nLine 2");
    const QByteArray notesIntl = QByteArray("Caf") + static_cast<char>(0xe9) + "\r\nQTH";

    QByteArray input;
    input += tag("ADDRESS", address);
    input += tag("COMMENT", "Plain comment");
    input += tag("NOTES_INTL", notesIntl);
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, QStringLiteral("address"), QString());
    appendField(record, QStringLiteral("address_intl"), QString());
    appendField(record, QStringLiteral("comment"), QString());
    appendField(record, QStringLiteral("comment_intl"), QString());
    appendField(record, QStringLiteral("notes"), QString());
    appendField(record, QStringLiteral("notes_intl"), QString());

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("address")).toString(), QString::fromLatin1(address));
    QCOMPARE(record.value(QStringLiteral("address_intl")).toString(), QString::fromLatin1(address));
    QCOMPARE(record.value(QStringLiteral("comment")).toString(), QStringLiteral("Plain comment"));
    QCOMPARE(record.value(QStringLiteral("comment_intl")).toString(), QStringLiteral("Plain comment"));
    QCOMPARE(record.value(QStringLiteral("notes")).toString(), QStringLiteral("Cafe\r\nQTH"));
    QCOMPARE(record.value(QStringLiteral("notes_intl")).toString(), QString::fromLatin1(notesIntl));
}

void AdiFormatTest::importNextAppliesDefaultsBeforeIntlCompanionFields()
{
    QByteArray input;
    input += tag("CALL", "OK1AA");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QMap<QString, QString> defaults;
    defaults.insert(QStringLiteral("notes_intl"), QString::fromUtf8("Caf\xc3\xa9\nQTH"));
    format.setDefaults(defaults);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());
    appendField(record, QStringLiteral("notes"), QString());
    appendField(record, QStringLiteral("notes_intl"), QString());

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), QStringLiteral("OK1AA"));
    QCOMPARE(record.value(QStringLiteral("notes")).toString(), QStringLiteral("Cafe\nQTH"));
    QCOMPARE(record.value(QStringLiteral("notes_intl")).toString(), QString::fromUtf8("Caf\xc3\xa9\nQTH"));
}

QTEST_APPLESS_MAIN(AdiFormatTest)

#include "tst_adiformat.moc"
