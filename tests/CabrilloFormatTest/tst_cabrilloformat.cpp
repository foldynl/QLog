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
    void formatsExactFrequency();
    void formatsBandOnlyFrequencyField_data();
    void formatsBandOnlyFrequencyField();
    void rejectsUnsupportedBandFallback();
    void exportsBandOnlyContact();
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

void CabrilloFormatTest::formatsExactFrequency()
{
    QCOMPARE(CabrilloFormat::formatFrequencyOrBand(QStringLiteral("14.074"),
                                                   QStringLiteral("40m"), 5),
             QStringLiteral("14074"));
}

void CabrilloFormatTest::formatsBandOnlyFrequencyField_data()
{
    QTest::addColumn<QString>("band");
    QTest::addColumn<QString>("expected");

    QTest::newRow("hf") << QStringLiteral("20m") << QStringLiteral("14000");
    QTest::newRow("vhf") << QStringLiteral("2m") << QStringLiteral("144  ");
    QTest::newRow("uhf") << QStringLiteral("70cm") << QStringLiteral("432  ");
    QTest::newRow("microwave") << QStringLiteral("23cm") << QStringLiteral("1.2G ");
    QTest::newRow("light") << QStringLiteral("submm") << QStringLiteral("LIGHT");
    QTest::newRow("case-insensitive") << QStringLiteral("20M") << QStringLiteral("14000");
}

void CabrilloFormatTest::formatsBandOnlyFrequencyField()
{
    QFETCH(QString, band);
    QFETCH(QString, expected);

    QCOMPARE(CabrilloFormat::formatFrequencyOrBand(QString(), band, 5), expected);
}

void CabrilloFormatTest::rejectsUnsupportedBandFallback()
{
    QVERIFY(CabrilloFormat::bandToFrequencyField(QStringLiteral("30m")).isEmpty());
    QCOMPARE(CabrilloFormat::formatFrequencyOrBand(QString(), QStringLiteral("30m"), 5),
             QString(5, QLatin1Char(' ')));
}

void CabrilloFormatTest::exportsBandOnlyContact()
{
    QString output;
    QTextStream stream(&output);
    CabrilloFormat format(stream);
    format.setTemplateId(1);
    format.exportStart();

    QSqlRecord record;
    record.append(QSqlField(QStringLiteral("freq")));
    record.setValue(QStringLiteral("freq"), QVariant());
    record.append(QSqlField(QStringLiteral("band")));
    record.setValue(QStringLiteral("band"), QStringLiteral("20m"));
    format.exportContact(record);
    format.exportEnd();
    stream.flush();

    QVERIFY(output.contains(QStringLiteral("QSO: 14000\n")));
}

QTEST_APPLESS_MAIN(CabrilloFormatTest)

#include "tst_cabrilloformat.moc"
