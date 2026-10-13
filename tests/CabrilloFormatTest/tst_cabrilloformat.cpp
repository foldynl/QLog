#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

#include "logformat/CabrilloFormat.h"

class CabrilloFormatTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void exportsLocationHeader_data();
    void exportsLocationHeader();
    void validatesOfftime_data();
    void validatesOfftime();
    void formatsExactFrequency_data();
    void formatsExactFrequency();
    void formatsBandOnlyFrequencyField_data();
    void formatsBandOnlyFrequencyField();
    void rejectsUnsupportedBandFallback();
    void exportsBandOnlyContact_data();
    void exportsBandOnlyContact();
    void exportsExactFrequencyContact_data();
    void exportsExactFrequencyContact();
    void exportsCrossBandContactUsesTransmitFrequency();
};

void CabrilloFormatTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());

    QSqlQuery query;
    QVERIFY2(query.exec("CREATE TABLE cabrillo_templates ("
                        "id INTEGER PRIMARY KEY, name TEXT, "
                        "contest_name_for_header TEXT, default_category_mode TEXT)"),
             qPrintable(query.lastError().text()));
    QVERIFY2(query.exec("CREATE TABLE cabrillo_template_columns ("
                        "template_id INTEGER, position INTEGER, db_field TEXT, "
                        "width INTEGER, formatter TEXT, label TEXT)"),
             qPrintable(query.lastError().text()));
    QVERIFY(query.exec("INSERT INTO cabrillo_templates VALUES (1, 'Test', 'TEST', 'MIXED')"));
    QVERIFY(query.exec("INSERT INTO cabrillo_template_columns "
                       "VALUES (1, 1, 'freq', 5, 'freq_khz', 'Freq')"));
    QVERIFY2(query.exec("CREATE TABLE bands (name TEXT, start_freq REAL, end_freq REAL, "
                        "enabled INTEGER, sat_designator TEXT)"),
             qPrintable(query.lastError().text()));
    QVERIFY2(query.exec("INSERT INTO bands VALUES "
                        "('6m', 50, 54, 1, ''), ('4m', 70, 71, 1, ''), "
                        "('2m', 144, 148, 1, ''), ('1.25m', 222, 225, 0, ''), "
                        "('70cm', 420, 450, 1, ''), ('33cm', 902, 928, 0, ''), "
                        "('23cm', 1240, 1300, 0, ''), ('13cm', 2300, 2450, 0, ''), "
                        "('3cm', 10000, 10500, 0, ''), ('4mm', 75500, 81000, 0, ''), "
                        "('2.5mm', 119980, 120020, 0, ''), ('2mm', 142000, 149000, 0, ''), "
                        "('1mm', 241000, 250000, 0, ''), ('submm', 300000, 7500000, 0, '')"),
             qPrintable(query.lastError().text()));
}

void CabrilloFormatTest::cleanupTestCase()
{
    const QString connectionName = QString::fromLatin1(QSqlDatabase::defaultConnection);
    {
        QSqlDatabase db = QSqlDatabase::database();
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CabrilloFormatTest::exportsLocationHeader_data()
{
    QTest::addColumn<QString>("location");
    QTest::addColumn<QString>("expectedHeader");

    QTest::newRow("dx") << QStringLiteral("DX") << QStringLiteral("\nLOCATION: DX\n");
    QTest::newRow("section") << QStringLiteral("EMA") << QStringLiteral("\nLOCATION: EMA\n");
    QTest::newRow("empty") << QString() << QString();
}

void CabrilloFormatTest::exportsLocationHeader()
{
    QFETCH(QString, location);
    QFETCH(QString, expectedHeader);

    QString output;
    QTextStream stream(&output);
    CabrilloFormat format(stream);
    format.setTemplateId(1);

    CabrilloFormat::HeaderData header;
    header.location = location;
    format.setHeaderData(header);
    format.exportStart();
    format.exportEnd();
    stream.flush();

    QCOMPARE(output.count(QStringLiteral("\nLOCATION: ")), expectedHeader.isEmpty() ? 0 : 1);
    if ( !expectedHeader.isEmpty() )
        QVERIFY2(output.contains(expectedHeader), qPrintable(output));
}

void CabrilloFormatTest::validatesOfftime_data()
{
    QTest::addColumn<QString>("offtime");
    QTest::addColumn<bool>("valid");

    QTest::newRow("empty") << QString() << true;
    QTest::newRow("blank") << QStringLiteral(" \t\r\n") << true;
    QTest::newRow("single-interval") << QStringLiteral("2026-10-10 0300 2026-10-10 0400") << true;
    QTest::newRow("outer-whitespace") << QStringLiteral(" 2026-10-10 0300 2026-10-10 0400\r\n") << true;
    QTest::newRow("same-minute") << QStringLiteral("2026-10-10 0300 2026-10-10 0300") << true;
    QTest::newRow("midnight") << QStringLiteral("2026-10-10 2359 2026-10-11 0000") << true;
    QTest::newRow("leap-day") << QStringLiteral("2024-02-29 0300 2024-03-01 0400") << true;
    QTest::newRow("dst-transition") << QStringLiteral("2026-03-29 0200 2026-03-29 0230") << true;
    QTest::newRow("reversed-time") << QStringLiteral("2026-10-10 0400 2026-10-10 0300") << false;
    QTest::newRow("reversed-date") << QStringLiteral("2026-10-11 0000 2026-10-10 2359") << false;
    QTest::newRow("invalid-start-date") << QStringLiteral("2026-02-29 0300 2026-03-01 0400") << false;
    QTest::newRow("invalid-end-date") << QStringLiteral("2026-04-30 0300 2026-04-31 0400") << false;
    QTest::newRow("invalid-start-hour") << QStringLiteral("2026-10-10 2400 2026-10-11 0400") << false;
    QTest::newRow("invalid-end-minute") << QStringLiteral("2026-10-10 0300 2026-10-10 0460") << false;
    QTest::newRow("invalid-year") << QStringLiteral("0000-10-10 0300 2026-10-10 0400") << false;
    QTest::newRow("short-date") << QStringLiteral("2026-1-10 0300 2026-10-10 0400") << false;
    QTest::newRow("short-time") << QStringLiteral("2026-10-10 300 2026-10-10 0400") << false;
    QTest::newRow("time-colon") << QStringLiteral("2026-10-10 03:00 2026-10-10 04:00") << false;
    QTest::newRow("missing-end") << QStringLiteral("2026-10-10 0300") << false;
    QTest::newRow("extra-text") << QStringLiteral("2026-10-10 0300 2026-10-10 0400 UTC") << false;
    QTest::newRow("header-prefix") << QStringLiteral("OFFTIME: 2026-10-10 0300 2026-10-10 0400") << false;
    QTest::newRow("split-interval") << QStringLiteral("2026-10-10 0300\n2026-10-10 0400") << false;
    QTest::newRow("multiple-intervals")
        << QStringLiteral("2026-10-10 0300 2026-10-10 0400\r\n2026-10-10 0600 2026-10-10 0700") << false;
    QTest::newRow("multiple-intervals-one-line")
        << QStringLiteral("2026-10-10 0300 2026-10-10 0400 2026-10-10 0600 2026-10-10 0700") << false;
}

void CabrilloFormatTest::validatesOfftime()
{
    QFETCH(QString, offtime);
    QFETCH(bool, valid);

    QCOMPARE(CabrilloFormat::isValidOfftime(offtime), valid);
}

void CabrilloFormatTest::formatsExactFrequency_data()
{
    QTest::addColumn<QString>("frequency");
    QTest::addColumn<QString>("band");
    QTest::addColumn<QString>("expected");

    QTest::newRow("hf-frequency-precedence") << QStringLiteral("14.074") << QStringLiteral("40m") << QStringLiteral("14074");
    QTest::newRow("hf-upper-edge") << QStringLiteral("29.700") << QStringLiteral("10m") << QStringLiteral("29700");
    QTest::newRow("6m-lower-edge") << QStringLiteral("50.000") << QStringLiteral("6m") << QStringLiteral("50   ");
    QTest::newRow("6m") << QStringLiteral("50.313") << QStringLiteral("6m") << QStringLiteral("50   ");
    QTest::newRow("4m") << QStringLiteral("70.200") << QStringLiteral("4m") << QStringLiteral("70   ");
    QTest::newRow("2m") << QStringLiteral("144.300") << QStringLiteral("2m") << QStringLiteral("144  ");
    QTest::newRow("2m-fm") << QStringLiteral("145.500") << QStringLiteral("2m") << QStringLiteral("144  ");
    QTest::newRow("1.25m") << QStringLiteral("222.100") << QStringLiteral("1.25m") << QStringLiteral("222  ");
    QTest::newRow("70cm") << QStringLiteral("435.100") << QStringLiteral("70cm") << QStringLiteral("432  ");
    QTest::newRow("33cm") << QStringLiteral("903.100") << QStringLiteral("33cm") << QStringLiteral("902  ");
    QTest::newRow("23cm") << QStringLiteral("1296.100") << QStringLiteral("23cm") << QStringLiteral("1.2G ");
    QTest::newRow("13cm") << QStringLiteral("2400.100") << QStringLiteral("13cm") << QStringLiteral("2.3G ");
    QTest::newRow("3cm") << QStringLiteral("10368.100") << QStringLiteral("3cm") << QStringLiteral("10G  ");
    QTest::newRow("4mm") << QStringLiteral("76000.100") << QStringLiteral("4mm") << QStringLiteral("75G  ");
    QTest::newRow("2.5mm") << QStringLiteral("122000.100") << QStringLiteral("2.5mm") << QStringLiteral("122G ");
    QTest::newRow("2mm") << QStringLiteral("134000.100") << QStringLiteral("2mm") << QStringLiteral("134G ");
    QTest::newRow("1mm") << QStringLiteral("241000.100") << QStringLiteral("1mm") << QStringLiteral("241G ");
    QTest::newRow("light") << QStringLiteral("300000.100") << QStringLiteral("submm") << QStringLiteral("LIGHT");
    QTest::newRow("vhf-missing-band") << QStringLiteral("144.300") << QString() << QStringLiteral("144  ");
    QTest::newRow("vhf-frequency-precedence") << QStringLiteral("144.300") << QStringLiteral("70cm") << QStringLiteral("144  ");
    QTest::newRow("vhf-hf-band-conflict") << QStringLiteral("144.300") << QStringLiteral("20m") << QStringLiteral("144  ");
    QTest::newRow("uhf-missing-band") << QStringLiteral("435.100") << QString() << QStringLiteral("432  ");
    QTest::newRow("microwave-missing-band") << QStringLiteral("1296.100") << QString() << QStringLiteral("1.2G ");
    QTest::newRow("microwave-frequency-precedence") << QStringLiteral("1296.100") << QStringLiteral("2m") << QStringLiteral("1.2G ");
    QTest::newRow("6m-missing-band-at-lower-edge") << QStringLiteral("50.000") << QString() << QStringLiteral("50   ");
    QTest::newRow("2m-upper-edge") << QStringLiteral("148.000") << QString() << QStringLiteral("144  ");
}

void CabrilloFormatTest::formatsExactFrequency()
{
    QFETCH(QString, frequency);
    QFETCH(QString, band);
    QFETCH(QString, expected);

    QCOMPARE(CabrilloFormat::formatFrequencyOrBand(frequency, band, 5), expected);
}

void CabrilloFormatTest::formatsBandOnlyFrequencyField_data()
{
    QTest::addColumn<QString>("band");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<QString>("frequency");

    QTest::newRow("hf") << QStringLiteral("20m") << QStringLiteral("14000") << QString();
    QTest::newRow("6m") << QStringLiteral("6m") << QStringLiteral("50   ") << QString();
    QTest::newRow("4m") << QStringLiteral("4m") << QStringLiteral("70   ") << QString();
    QTest::newRow("vhf") << QStringLiteral("2m") << QStringLiteral("144  ") << QString();
    QTest::newRow("uhf") << QStringLiteral("70cm") << QStringLiteral("432  ") << QString();
    QTest::newRow("microwave") << QStringLiteral("23cm") << QStringLiteral("1.2G ") << QString();
    QTest::newRow("10ghz") << QStringLiteral("3cm") << QStringLiteral("10G  ") << QString();
    QTest::newRow("light") << QStringLiteral("submm") << QStringLiteral("LIGHT") << QString();
    QTest::newRow("case-insensitive") << QStringLiteral("20M") << QStringLiteral("14000") << QString();
    QTest::newRow("zero-hf") << QStringLiteral("20m") << QStringLiteral("14000") << QStringLiteral("0");
    QTest::newRow("zero-vhf") << QStringLiteral("2m") << QStringLiteral("144  ") << QStringLiteral("0");
    QTest::newRow("zero-microwave") << QStringLiteral("23cm") << QStringLiteral("1.2G ") << QStringLiteral("0");
}

void CabrilloFormatTest::formatsBandOnlyFrequencyField()
{
    QFETCH(QString, band);
    QFETCH(QString, expected);
    QFETCH(QString, frequency);

    QCOMPARE(CabrilloFormat::formatFrequencyOrBand(frequency, band, 5), expected);
}

void CabrilloFormatTest::rejectsUnsupportedBandFallback()
{
    QVERIFY(CabrilloFormat::bandToFrequencyField(QStringLiteral("30m")).isEmpty());
    QCOMPARE(CabrilloFormat::formatFrequencyOrBand(QString(), QStringLiteral("30m"), 5),
             QString(5, QLatin1Char(' ')));
}

void CabrilloFormatTest::exportsBandOnlyContact_data()
{
    formatsBandOnlyFrequencyField_data();
}

void CabrilloFormatTest::exportsBandOnlyContact()
{
    QFETCH(QString, band);
    QFETCH(QString, expected);
    QFETCH(QString, frequency);

    QString output;
    QTextStream stream(&output);
    CabrilloFormat format(stream);
    format.setTemplateId(1);
    format.exportStart();

    QSqlRecord record;
    record.append(QSqlField(QStringLiteral("freq")));
    record.setValue(QStringLiteral("freq"), frequency.isEmpty()
                                           ? QVariant() : QVariant(frequency.toDouble()));
    record.append(QSqlField(QStringLiteral("band")));
    record.setValue(QStringLiteral("band"), band);
    format.exportContact(record);
    format.exportEnd();
    stream.flush();

    QVERIFY2(output.contains(QStringLiteral("QSO: ") + expected + QLatin1Char('\n')),
             qPrintable(output));
}

void CabrilloFormatTest::exportsCrossBandContactUsesTransmitFrequency()
{
    QString output;
    QTextStream stream(&output);
    CabrilloFormat format(stream);
    format.setTemplateId(1);
    format.exportStart();

    QSqlRecord record;
    record.append(QSqlField(QStringLiteral("freq")));
    record.setValue(QStringLiteral("freq"), 144.300);
    record.append(QSqlField(QStringLiteral("band")));
    record.setValue(QStringLiteral("band"), QStringLiteral("2m"));
    record.append(QSqlField(QStringLiteral("freq_rx")));
    record.setValue(QStringLiteral("freq_rx"), 435.100);
    record.append(QSqlField(QStringLiteral("band_rx")));
    record.setValue(QStringLiteral("band_rx"), QStringLiteral("70cm"));
    format.exportContact(record);
    format.exportEnd();
    stream.flush();

    QVERIFY2(output.contains(QStringLiteral("QSO: 144  \n")), qPrintable(output));
}

void CabrilloFormatTest::exportsExactFrequencyContact_data()
{
    formatsExactFrequency_data();
}

void CabrilloFormatTest::exportsExactFrequencyContact()
{
    QFETCH(QString, frequency);
    QFETCH(QString, band);
    QFETCH(QString, expected);

    QString output;
    QTextStream stream(&output);
    CabrilloFormat format(stream);
    format.setTemplateId(1);
    format.exportStart();

    QSqlRecord record;
    record.append(QSqlField(QStringLiteral("freq")));
    record.setValue(QStringLiteral("freq"), frequency.toDouble());
    record.append(QSqlField(QStringLiteral("band")));
    record.setValue(QStringLiteral("band"), band);
    format.exportContact(record);
    format.exportEnd();
    stream.flush();

    QVERIFY2(output.contains(QStringLiteral("QSO: ") + expected + QLatin1Char('\n')),
             qPrintable(output));
}

QTEST_GUILESS_MAIN(CabrilloFormatTest)

#include "tst_cabrilloformat.moc"
