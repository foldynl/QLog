#include <QtTest>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlField>
#include <QSqlRecord>

#include "logformat/AdiFormat.h"
#include "service/GenericQSOUploader.h"
#include "service/lotw/Lotw.h"

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

class TestLotwUploader : public GenericQSOUploader
{
public:
    TestLotwUploader() :
        GenericQSOUploader(LotwUploader::uploadedFields())
    {
    }

    QByteArray payload(const QSqlRecord &record)
    {
        return generateADIF({record});
    }

    void uploadQSOList(const QList<QSqlRecord> &, const QVariantMap &) override
    {
    }

    void abortRequest() override
    {
    }

private:
    void processReply(QNetworkReply *) override
    {
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
    void lotwPayloadKeepsBandOnlyRxBand();
    void writeSqlRecordExportsApplicationTags();
    void writeSqlRecordExportsRawFieldsFiltersInvalidNames();
    void exportContactNormalizesGridAndTerminatesRecord();
    void importNextMapsAdiFieldsAndStoresUnknownFields();
    void importNextNormalizesCallsign_data();
    void importNextNormalizesCallsign();
    void importNextNormalizesStructuredFields_data();
    void importNextNormalizesStructuredFields();
    void importNextValidatesTimestamps_data();
    void importNextValidatesTimestamps();
    void importNextNormalizesConstrainedEnumFields();
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

    QVERIFY(output.startsWith(" <ADIF_VER:5>3.1.8\n"));
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
    appendField(record, QStringLiteral("freq"), QVariant());
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
    QVERIFY(!output.contains("<freq:"));
    QVERIFY(output.contains("<iota:6>EU-001\n"));
    QVERIFY(output.contains("<qsl_rcvd:1>Y\n"));
    QVERIFY(!output.contains("qsl_sent"));
    QVERIFY(output.contains("<notes:12>Line1\r\nLine2\n"));
    QVERIFY(output.contains("<comment:10>Line1Line2\n"));
    QVERIFY(output.contains("<qso_date:8>20260528\n"));
    QVERIFY(output.contains("<time_on:6>123456\n"));
}

void AdiFormatTest::lotwPayloadKeepsBandOnlyRxBand()
{
    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));
    appendField(record, QStringLiteral("band"), QStringLiteral("20m"));
    appendField(record, QStringLiteral("freq"), QVariant());
    appendField(record, QStringLiteral("band_rx"), QStringLiteral("40m"));
    appendField(record, QStringLiteral("freq_rx"), QVariant());
    appendField(record, QStringLiteral("comment"), QStringLiteral("not uploaded"));

    TestLotwUploader uploader;
    const QByteArray output = uploader.payload(record);

    QVERIFY(output.contains("<call:5>OK1AA\n"));
    QVERIFY(output.contains("<band:3>20m\n"));
    QVERIFY(output.contains("<band_rx:3>40m\n"));
    QVERIFY(!output.contains("<freq:"));
    QVERIFY(!output.contains("<freq_rx:"));
    QVERIFY(!output.contains("<comment:"));
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

void AdiFormatTest::writeSqlRecordExportsRawFieldsFiltersInvalidNames()
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

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    format.writeRecordForTest(record);
    stream.flush();

    QVERIFY(output.contains("<UNKNOWN_FIELD:2>ok\n"));
    QVERIFY(output.contains("<APP_QLOG_RAW:12>Line1\r\nLine2\n"));
    QVERIFY(!output.contains("dash"));
    QVERIFY(!output.contains("digit"));
    QVERIFY(!output.contains("xml"));
    QVERIFY(!output.contains("space"));
    QVERIFY(!output.contains("angle"));
    QVERIFY(!output.contains("shortapp"));
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

void AdiFormatTest::importNextNormalizesCallsign_data()
{
    QTest::addColumn<QByteArray>("call");
    QTest::addColumn<QString>("expectedCallsign");

    QTest::newRow("lowercase") << QByteArray("ub8a") << QStringLiteral("UB8A");
    QTest::newRow("trailing-space") << QByteArray("UB8A ") << QStringLiteral("UB8A");
    QTest::newRow("leading-space") << QByteArray(" UB8A") << QStringLiteral("UB8A");
    QTest::newRow("tabs-and-line-breaks") << QByteArray("\tub8a\r\n") << QStringLiteral("UB8A");
    QTest::newRow("portable") << QByteArray(" f/ok1aa/p ") << QStringLiteral("F/OK1AA/P");
    QTest::newRow("whitespace-only") << QByteArray(" \t\r\n") << QString();
    QTest::newRow("empty") << QByteArray("") << QString();
}

void AdiFormatTest::importNextNormalizesCallsign()
{
    QFETCH(QByteArray, call);
    QFETCH(QString, expectedCallsign);

    QByteArray input = tag("CALL", call) + "<EOR>";
    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QString());

    QVERIFY(format.importNext(record));
    QCOMPARE(record.value(QStringLiteral("callsign")).toString(), expectedCallsign);
}

void AdiFormatTest::importNextNormalizesStructuredFields_data()
{
    QTest::addColumn<QByteArray>("adifField");
    QTest::addColumn<QString>("column");
    QTest::addColumn<QByteArray>("value");
    QTest::addColumn<QString>("expected");

    QTest::newRow("band") << QByteArray("BAND") << QStringLiteral("band")
                         << QByteArray(" 40M\t") << QStringLiteral("40m");
    QTest::newRow("mode") << QByteArray("MODE") << QStringLiteral("mode")
                         << QByteArray(" ft8\t") << QStringLiteral("FT8");
    QTest::newRow("submode") << QByteArray("SUBMODE") << QStringLiteral("submode")
                            << QByteArray(" ft4\t") << QStringLiteral("FT4");
    QTest::newRow("satellite") << QByteArray("SAT_NAME") << QStringLiteral("sat_name")
                              << QByteArray(" Ao-73\t") << QStringLiteral("Ao-73");
    QTest::newRow("empty-satellite") << QByteArray("SAT_NAME") << QStringLiteral("sat_name")
                                    << QByteArray(" \t") << QString();
    QTest::newRow("station") << QByteArray("STATION_CALLSIGN") << QStringLiteral("station_callsign")
                            << QByteArray(" ok1aa/p\t") << QStringLiteral("OK1AA/P");
    QTest::newRow("operator") << QByteArray("OPERATOR") << QStringLiteral("operator")
                             << QByteArray(" ok1aa\t") << QStringLiteral("OK1AA");
    QTest::newRow("propagation") << QByteArray("PROP_MODE") << QStringLiteral("prop_mode")
                                << QByteArray(" sat\t") << QStringLiteral("SAT");
    QTest::newRow("receive-band") << QByteArray("BAND_RX") << QStringLiteral("band_rx")
        << QByteArray(" 2M\t") << QStringLiteral("2m");
    QTest::newRow("continent") << QByteArray("CONT") << QStringLiteral("cont")
        << QByteArray(" eu\t") << QStringLiteral("EU");
    QTest::newRow("prefix") << QByteArray("PFX") << QStringLiteral("pfx")
        << QByteArray(" ok\t") << QStringLiteral("OK");
    QTest::newRow("arrl-section") << QByteArray("ARRL_SECT") << QStringLiteral("arrl_sect")
        << QByteArray(" EMA\t") << QStringLiteral("EMA");
    QTest::newRow("own-arrl-section") << QByteArray("MY_ARRL_SECT") << QStringLiteral("my_arrl_sect")
        << QByteArray(" EMA\t") << QStringLiteral("EMA");
    QTest::newRow("state") << QByteArray("STATE") << QStringLiteral("state")
        << QByteArray(" CA\t") << QStringLiteral("CA");
    QTest::newRow("own-state") << QByteArray("MY_STATE") << QStringLiteral("my_state")
        << QByteArray(" CA\t") << QStringLiteral("CA");
    QTest::newRow("county") << QByteArray("CNTY") << QStringLiteral("cnty")
        << QByteArray(" CA,San Francisco\t") << QStringLiteral("CA,San Francisco");
    QTest::newRow("own-county") << QByteArray("MY_CNTY") << QStringLiteral("my_cnty")
        << QByteArray(" CA,San Francisco\t") << QStringLiteral("CA,San Francisco");
    QTest::newRow("dok") << QByteArray("DARC_DOK") << QStringLiteral("darc_dok")
        << QByteArray(" a01\t") << QStringLiteral("A01");
    QTest::newRow("own-dok") << QByteArray("MY_DARC_DOK") << QStringLiteral("my_darc_dok")
        << QByteArray(" a01\t") << QStringLiteral("A01");
    QTest::newRow("region") << QByteArray("REGION") << QStringLiteral("region")
        << QByteArray(" ET\t") << QStringLiteral("ET");
    QTest::newRow("province") << QByteArray("VE_PROV") << QStringLiteral("ve_prov")
        << QByteArray(" ON\t") << QStringLiteral("ON");
    QTest::newRow("contest-id") << QByteArray("CONTEST_ID") << QStringLiteral("contest_id")
        << QByteArray(" CQ-WW-CW\t") << QStringLiteral("CQ-WW-CW");
    QTest::newRow("class") << QByteArray("CLASS") << QStringLiteral("class")
        << QByteArray(" 1A\t") << QStringLiteral("1A");
    QTest::newRow("precedence") << QByteArray("PRECEDENCE") << QStringLiteral("precedence")
        << QByteArray(" A\t") << QStringLiteral("A");
    QTest::newRow("satellite-mode") << QByteArray("SAT_MODE") << QStringLiteral("sat_mode")
        << QByteArray(" U/V\t") << QStringLiteral("U/V");
    QTest::newRow("owner") << QByteArray("OWNER_CALLSIGN") << QStringLiteral("owner_callsign")
        << QByteArray(" ok1aa/p\t") << QStringLiteral("OK1AA/P");
    QTest::newRow("contacted-operator") << QByteArray("CONTACTED_OP") << QStringLiteral("contacted_op")
        << QByteArray(" ok1aa\t") << QStringLiteral("OK1AA");
    QTest::newRow("guest-operator") << QByteArray("GUEST_OP") << QStringLiteral("guest_op")
        << QByteArray(" ok1aa\t") << QStringLiteral("OK1AA");
    QTest::newRow("eqsl-owner") << QByteArray("EQ_CALL") << QStringLiteral("eq_call")
        << QByteArray(" ok1aa\t") << QStringLiteral("OK1AA");
    QTest::newRow("iota") << QByteArray("IOTA") << QStringLiteral("iota")
        << QByteArray(" eu-005\t") << QStringLiteral("EU-005");
    QTest::newRow("own-iota") << QByteArray("MY_IOTA") << QStringLiteral("my_iota")
        << QByteArray(" eu-005\t") << QStringLiteral("EU-005");
    QTest::newRow("iota-island") << QByteArray("IOTA_ISLAND_ID") << QStringLiteral("iota_island_id")
        << QByteArray(" 00001234\t") << QStringLiteral("00001234");
    QTest::newRow("own-iota-island") << QByteArray("MY_IOTA_ISLAND_ID") << QStringLiteral("my_iota_island_id")
        << QByteArray(" 00001234\t") << QStringLiteral("00001234");
    QTest::newRow("sota") << QByteArray("SOTA_REF") << QStringLiteral("sota_ref")
        << QByteArray(" ok/jc-001\t") << QStringLiteral("OK/JC-001");
    QTest::newRow("own-sota") << QByteArray("MY_SOTA_REF") << QStringLiteral("my_sota_ref")
        << QByteArray(" ok/jc-001\t") << QStringLiteral("OK/JC-001");
    QTest::newRow("wwff") << QByteArray("WWFF_REF") << QStringLiteral("wwff_ref")
        << QByteArray(" okff-0001\t") << QStringLiteral("OKFF-0001");
    QTest::newRow("own-wwff") << QByteArray("MY_WWFF_REF") << QStringLiteral("my_wwff_ref")
        << QByteArray(" okff-0001\t") << QStringLiteral("OKFF-0001");
    QTest::newRow("empty-grid") << QByteArray("GRIDSQUARE") << QStringLiteral("gridsquare")
        << QByteArray(" \t") << QStringLiteral("");
    QTest::newRow("grid-extension") << QByteArray("GRIDSQUARE_EXT") << QStringLiteral("gridsquare_ext")
        << QByteArray(" bb\t") << QStringLiteral("BB");
    QTest::newRow("own-grid") << QByteArray("MY_GRIDSQUARE") << QStringLiteral("my_gridsquare")
        << QByteArray(" jo70aa\t") << QStringLiteral("JO70AA");
    QTest::newRow("own-grid-extension") << QByteArray("MY_GRIDSQUARE_EXT") << QStringLiteral("my_gridsquare_ext")
        << QByteArray(" bb\t") << QStringLiteral("BB");
    QTest::newRow("pota-list") << QByteArray("POTA_REF") << QStringLiteral("pota_ref")
        << QByteArray(" k-4562@us-ca , k-0002\t") << QStringLiteral(" K-4562@US-CA , K-0002\t");
    QTest::newRow("own-pota-list") << QByteArray("MY_POTA_REF") << QStringLiteral("my_pota_ref")
        << QByteArray(" k-0001 , k-0002\t") << QStringLiteral(" K-0001 , K-0002\t");
    QTest::newRow("vucc-list") << QByteArray("VUCC_GRIDS") << QStringLiteral("vucc_grids")
        << QByteArray(" jo70aa , jo71aa\t") << QStringLiteral(" JO70AA , JO71AA\t");
    QTest::newRow("own-vucc-list") << QByteArray("MY_VUCC_GRIDS") << QStringLiteral("my_vucc_grids")
        << QByteArray(" jo70 , jo71\t") << QStringLiteral(" JO70 , JO71\t");
    QTest::newRow("county-list") << QByteArray("USACA_COUNTIES") << QStringLiteral("usaca_counties")
        << QByteArray(" CA,San Francisco : CA,San Mateo\t") << QStringLiteral(" CA,San Francisco : CA,San Mateo\t");
    QTest::newRow("own-county-list") << QByteArray("MY_USACA_COUNTIES") << QStringLiteral("my_usaca_counties")
        << QByteArray(" CA,San Francisco : CA,San Mateo\t") << QStringLiteral(" CA,San Francisco : CA,San Mateo\t");
    QTest::newRow("alternative-counties") << QByteArray("CNTY_ALT") << QStringLiteral("cnty_alt")
        << QByteArray(" NZ_Regions:Hawkes Bay/Wairoa ; NZ_Islands:North Island\t") << QStringLiteral(" NZ_Regions:Hawkes Bay/Wairoa ; NZ_Islands:North Island\t");
    QTest::newRow("own-alternative-counties") << QByteArray("MY_CNTY_ALT") << QStringLiteral("my_cnty_alt")
        << QByteArray(" NZ_Regions:Hawkes Bay/Wairoa ; NZ_Islands:North Island\t") << QStringLiteral(" NZ_Regions:Hawkes Bay/Wairoa ; NZ_Islands:North Island\t");
    QTest::newRow("list-order-and-empty-items") << QByteArray("POTA_REF") << QStringLiteral("pota_ref")
        << QByteArray(" k-0002 , , k-0001 , k-0002 , ") << QStringLiteral(" K-0002 , , K-0001 , K-0002 , ");
    QTest::newRow("blank-list") << QByteArray("VUCC_GRIDS") << QStringLiteral("vucc_grids")
        << QByteArray(" \t") << QStringLiteral(" \t");
    QTest::newRow("qsl-received") << QByteArray("QSL_RCVD") << QStringLiteral("qsl_rcvd")
        << QByteArray(" y\t") << QStringLiteral("Y");
    QTest::newRow("qsl-sent") << QByteArray("QSL_SENT") << QStringLiteral("qsl_sent")
        << QByteArray(" q\t") << QStringLiteral("Q");
    QTest::newRow("legacy-verified") << QByteArray("QSL_RCVD") << QStringLiteral("qsl_rcvd")
        << QByteArray(" v\t") << QStringLiteral("Y");
    QTest::newRow("lotw-received") << QByteArray("LOTW_QSL_RCVD") << QStringLiteral("lotw_qsl_rcvd")
        << QByteArray(" y\t") << QStringLiteral("Y");
    QTest::newRow("eqsl-received") << QByteArray("EQSL_QSL_RCVD") << QStringLiteral("eqsl_qsl_rcvd")
        << QByteArray(" y\t") << QStringLiteral("Y");
    QTest::newRow("dcl-sent") << QByteArray("DCL_QSL_SENT") << QStringLiteral("dcl_qsl_sent")
        << QByteArray(" y\t") << QStringLiteral("Y");
    QTest::newRow("upload-status") << QByteArray("HRDLOG_QSO_UPLOAD_STATUS") << QStringLiteral("hrdlog_qso_upload_status")
        << QByteArray(" m\t") << QStringLiteral("M");
    QTest::newRow("download-status") << QByteArray("QRZCOM_QSO_DOWNLOAD_STATUS") << QStringLiteral("qrzcom_qso_download_status")
        << QByteArray(" i\t") << QStringLiteral("I");
    QTest::newRow("eqsl-ag") << QByteArray("EQSL_AG") << QStringLiteral("eqsl_ag")
        << QByteArray(" y\t") << QStringLiteral("Y");
    QTest::newRow("morse-key") << QByteArray("MORSE_KEY_TYPE") << QStringLiteral("morse_key_type")
        << QByteArray(" sk\t") << QStringLiteral("SK");
    QTest::newRow("own-morse-key") << QByteArray("MY_MORSE_KEY_TYPE") << QStringLiteral("my_morse_key_type")
        << QByteArray(" bug\t") << QStringLiteral("BUG");
    QTest::newRow("blank-qsl") << QByteArray("QSL_RCVD") << QStringLiteral("qsl_rcvd")
        << QByteArray(" \t") << QStringLiteral("N");
    QTest::newRow("blank-upload") << QByteArray("HRDLOG_QSO_UPLOAD_STATUS") << QStringLiteral("hrdlog_qso_upload_status")
        << QByteArray(" \t") << QStringLiteral("");
    QTest::newRow("blank-download") << QByteArray("QRZCOM_QSO_DOWNLOAD_STATUS") << QStringLiteral("qrzcom_qso_download_status")
        << QByteArray(" \t") << QStringLiteral("");
    QTest::newRow("blank-eqsl-ag") << QByteArray("EQSL_AG") << QStringLiteral("eqsl_ag")
        << QByteArray(" \t") << QStringLiteral("");
    QTest::newRow("invalid-qsl") << QByteArray("QSL_SENT") << QStringLiteral("qsl_sent")
        << QByteArray(" unknown\t") << QStringLiteral("N");
    QTest::newRow("legacy-first-character-status") << QByteArray("QSL_SENT") << QStringLiteral("qsl_sent")
        << QByteArray(" ignored\t") << QStringLiteral("I");
}

void AdiFormatTest::importNextNormalizesStructuredFields()
{
    QFETCH(QByteArray, adifField);
    QFETCH(QString, column);
    QFETCH(QByteArray, value);
    QFETCH(QString, expected);

    QByteArray input = tag(adifField, value) + tag("COMMENT", " Keep  spacing ")
                       + tag("APP_QLOG_RAW", " \r\n raw\t ")
                       + tag("CUSTOM", " custom ") + "<EOR>";
    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, column, QString());
    appendField(record, QStringLiteral("comment"), QString());
    appendField(record, QStringLiteral("fields"), QString());

    QVERIFY(format.importNext(record));
    QCOMPARE(record.value(column).toString(), expected);
    QCOMPARE(record.value(QStringLiteral("comment")).toString(), QStringLiteral(" Keep  spacing "));

    const QJsonObject fields = QJsonDocument::fromJson(record.value("fields").toByteArray()).object();
    QCOMPARE(fields.value("app_qlog_raw").toString(), QStringLiteral(" \r\n raw\t "));
    QCOMPARE(fields.value("custom").toString(), QStringLiteral(" custom "));
}

void AdiFormatTest::importNextValidatesTimestamps_data()
{
    QTest::addColumn<QByteArray>("date");
    QTest::addColumn<QByteArray>("timeOn");
    QTest::addColumn<QByteArray>("timeOff");
    QTest::addColumn<QDateTime>("expectedStart");
    QTest::addColumn<QDateTime>("expectedEnd");

    const QDateTime start(QDate(2026, 10, 17), QTime(14, 3), QTimeZone::utc());
    const QDateTime end(QDate(2026, 10, 17), QTime(14, 4), QTimeZone::utc());
    const QDateTime midnight(QDate(2026, 10, 17), QTime(0, 0), QTimeZone::utc());

    QTest::newRow("padded") << QByteArray(" 20261017\t") << QByteArray(" 1403\t")
                           << QByteArray(" 140400\t") << start << end;
    QTest::newRow("midnight") << QByteArray("20261017") << QByteArray("0000")
                             << QByteArray() << midnight << midnight;
    QTest::newRow("missing-on") << QByteArray("20261017") << QByteArray()
                               << QByteArray("1404") << end << end;
    QTest::newRow("blank-on") << QByteArray("20261017") << QByteArray(" \t")
                             << QByteArray("1404") << end << end;
    QTest::newRow("missing-off") << QByteArray("20261017") << QByteArray("1403")
                                << QByteArray() << start << start;
    QTest::newRow("missing-times") << QByteArray("20261017") << QByteArray()
                                  << QByteArray() << QDateTime() << QDateTime();
    QTest::newRow("invalid-on") << QByteArray("20261017") << QByteArray("25:00")
                               << QByteArray("1404") << end << end;
    QTest::newRow("invalid-on-midnight-off") << QByteArray("20261017") << QByteArray("256000")
                                            << QByteArray("000000") << midnight << midnight;
    QTest::newRow("invalid-off") << QByteArray("20261017") << QByteArray("1403")
                                << QByteArray("2460") << start << QDateTime();
    QTest::newRow("invalid-times") << QByteArray("20261017") << QByteArray("256000")
                                  << QByteArray("2460") << QDateTime() << QDateTime();
    QTest::newRow("invalid-date") << QByteArray("20261340") << QByteArray("1403")
                                 << QByteArray("1404") << QDateTime() << QDateTime();
}

void AdiFormatTest::importNextValidatesTimestamps()
{
    QFETCH(QByteArray, date);
    QFETCH(QByteArray, timeOn);
    QFETCH(QByteArray, timeOff);
    QFETCH(QDateTime, expectedStart);
    QFETCH(QDateTime, expectedEnd);

    QByteArray input = tag("QSO_DATE", date) + tag("TIME_ON", timeOn)
                       + tag("TIME_OFF", timeOff) + "<EOR>";
    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    appendField(record, QStringLiteral("start_time"), QDateTime());
    appendField(record, QStringLiteral("end_time"), QDateTime());

    QVERIFY(format.importNext(record));
    QCOMPARE(record.value(QStringLiteral("start_time")).toDateTime(), expectedStart);
    QCOMPARE(record.value(QStringLiteral("end_time")).toDateTime(), expectedEnd);
}

void AdiFormatTest::importNextNormalizesConstrainedEnumFields()
{
    QByteArray input;
    input += tag("QSL_RCVD_VIA", " d ");
    input += tag("QSL_SENT_VIA", "Ignore");
    input += tag("ANT_PATH", "o");
    input += tag("FORCE_INIT", "invalid");
    input += tag("QSO_COMPLETE", "nil");
    input += tag("QSO_RANDOM", "maybe");
    input += tag("SILENT_KEY", "y");
    input += tag("SWL", "unknown");
    input += "<EOR>\r\n";

    QBuffer buffer(&input);
    QVERIFY(buffer.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&buffer);
    TestAdiFormat format(stream);

    QSqlRecord record;
    for ( const QString &field : {QStringLiteral("qsl_rcvd_via"),
                                  QStringLiteral("qsl_sent_via"),
                                  QStringLiteral("ant_path"),
                                  QStringLiteral("force_init"),
                                  QStringLiteral("qso_complete"),
                                  QStringLiteral("qso_random"),
                                  QStringLiteral("silent_key"),
                                  QStringLiteral("swl")} )
    {
        appendField(record, field, QString());
    }

    QVERIFY(format.importNext(record));

    QCOMPARE(record.value(QStringLiteral("qsl_rcvd_via")).toString(), QStringLiteral("D"));
    QVERIFY(record.value(QStringLiteral("qsl_sent_via")).isNull());
    QCOMPARE(record.value(QStringLiteral("ant_path")).toString(), QStringLiteral("O"));
    QVERIFY(record.value(QStringLiteral("force_init")).isNull());
    QCOMPARE(record.value(QStringLiteral("qso_complete")).toString(), QStringLiteral("NIL"));
    QVERIFY(record.value(QStringLiteral("qso_random")).isNull());
    QCOMPARE(record.value(QStringLiteral("silent_key")).toString(), QStringLiteral("Y"));
    QVERIFY(record.value(QStringLiteral("swl")).isNull());
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
