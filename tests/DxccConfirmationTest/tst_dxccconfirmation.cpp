#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "data/DxccConfirmation.h"
#include "data/BandPlan.h"

class DxccConfirmationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void contextsAndSources();
    void satelliteIsExcluded();
    void cacheInvalidation();
    void normalizesFtxMode();

private:
    void insertContact(int dxcc,
                       int myDxcc,
                       const QString &band,
                       const QString &mode,
                       const QString &propMode,
                       const QString &qsl,
                       const QString &lotw,
                       const QString &eqsl);
};

void DxccConfirmationTest::initTestCase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE contacts ("
        "dxcc INTEGER, my_dxcc INTEGER, band TEXT, mode TEXT, prop_mode TEXT, "
        "qsl_rcvd TEXT, lotw_qsl_rcvd TEXT, eqsl_qsl_rcvd TEXT)")));
    QVERIFY(query.exec(QStringLiteral("CREATE INDEX contacts_dxcc_idx ON contacts(dxcc)")));
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE modes (name TEXT PRIMARY KEY, dxcc TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO modes(name, dxcc) VALUES "
        "('FT8', 'DIGITAL'), ('FT4', 'DIGITAL'), ('CW', 'CW'), ('SSB', 'PHONE')")));
}

void DxccConfirmationTest::cleanupTestCase()
{
    {
        QSqlDatabase db = QSqlDatabase::database();
        db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void DxccConfirmationTest::cleanup()
{
    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("DELETE FROM contacts")));
}

void DxccConfirmationTest::contextsAndSources()
{
    insertContact(100, 1, QStringLiteral("20m"), QStringLiteral("FT8"), QString(),
                  QStringLiteral("N"), QStringLiteral("Y"), QStringLiteral("N"));
    insertContact(100, 1, QStringLiteral("40m"), QStringLiteral("CW"), QString(),
                  QStringLiteral("Y"), QStringLiteral("N"), QStringLiteral("N"));
    insertContact(100, 2, QStringLiteral("15m"), QStringLiteral("SSB"), QString(),
                  QStringLiteral("N"), QStringLiteral("N"), QStringLiteral("Y"));

    DxccConfirmation confirmation;
    QCOMPARE(confirmation.status(100, 1, QString(), QString(), AlertDxccContext::Anywhere,
                                 DxccConfirmationSource::LotwConfirmation),
             DxccConfirmationResult::Confirmed);
    QCOMPARE(confirmation.status(100, 1, QStringLiteral("20m"), QString(), AlertDxccContext::Band,
                                 DxccConfirmationSource::LotwConfirmation),
             DxccConfirmationResult::Confirmed);
    QCOMPARE(confirmation.status(100, 1, QStringLiteral("40m"), QString(), AlertDxccContext::Band,
                                 DxccConfirmationSource::LotwConfirmation),
             DxccConfirmationResult::NotConfirmed);
    QCOMPARE(confirmation.status(100, 1, QString(), BandPlan::MODE_GROUP_STRING_FTx,
                                 AlertDxccContext::Mode,
                                 DxccConfirmationSource::LotwConfirmation),
             DxccConfirmationResult::Confirmed);
    QCOMPARE(confirmation.status(100, 1, QString(), QStringLiteral("CW"), AlertDxccContext::Mode,
                                 DxccConfirmationSource::PaperConfirmation),
             DxccConfirmationResult::Confirmed);
    QCOMPARE(confirmation.status(100, 1, QString(), QString(), AlertDxccContext::Anywhere,
                                 DxccConfirmationSource::EqslConfirmation),
             DxccConfirmationResult::NotConfirmed);
    QCOMPARE(confirmation.status(100, 0, QString(), QString(), AlertDxccContext::Anywhere,
                                 DxccConfirmationSource::EqslConfirmation),
             DxccConfirmationResult::Confirmed);
    QCOMPARE(confirmation.status(100, 0, QString(), QString(), AlertDxccContext::Anywhere,
                                 DxccConfirmationSource::NoConfirmationSource),
             DxccConfirmationResult::NotConfirmed);
}

void DxccConfirmationTest::satelliteIsExcluded()
{
    insertContact(200, 1, QStringLiteral("2m"), QStringLiteral("FT8"), QStringLiteral("SAT"),
                  QStringLiteral("Y"), QStringLiteral("Y"), QStringLiteral("Y"));

    DxccConfirmation confirmation;
    QCOMPARE(confirmation.status(200, 1, QString(), QString(), AlertDxccContext::Anywhere,
                                 DxccConfirmationSource::LotwConfirmation
                                     | DxccConfirmationSource::PaperConfirmation
                                     | DxccConfirmationSource::EqslConfirmation),
             DxccConfirmationResult::NotConfirmed);
}

void DxccConfirmationTest::cacheInvalidation()
{
    DxccConfirmation confirmation;
    const DxccConfirmationSources sources = DxccConfirmationSource::LotwConfirmation;

    QCOMPARE(confirmation.status(300, 1, QString(), QString(), AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::NotConfirmed);

    insertContact(300, 1, QStringLiteral("20m"), QStringLiteral("FT8"), QString(),
                  QStringLiteral("N"), QStringLiteral("Y"), QStringLiteral("N"));

    QCOMPARE(confirmation.status(300, 1, QString(), QString(), AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::NotConfirmed);

    confirmation.invalidate(300, 1);
    QCOMPARE(confirmation.status(300, 1, QString(), QString(), AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::Confirmed);

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("DELETE FROM contacts WHERE dxcc = 300")));
    QCOMPARE(confirmation.status(300, 1, QString(), QString(), AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::Confirmed);

    confirmation.clear();
    QCOMPARE(confirmation.status(300, 1, QString(), QString(), AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::NotConfirmed);
}

void DxccConfirmationTest::normalizesFtxMode()
{
    QCOMPARE(DxccConfirmation::normalizedMode(BandPlan::MODE_GROUP_STRING_FTx),
             BandPlan::MODE_GROUP_STRING_DIGITAL);
    QCOMPARE(DxccConfirmation::normalizedMode(BandPlan::MODE_GROUP_STRING_CW),
             BandPlan::MODE_GROUP_STRING_CW);
}

void DxccConfirmationTest::insertContact(int dxcc,
                                         int myDxcc,
                                         const QString &band,
                                         const QString &mode,
                                         const QString &propMode,
                                         const QString &qsl,
                                         const QString &lotw,
                                         const QString &eqsl)
{
    QSqlQuery query;
    QVERIFY(query.prepare(QStringLiteral(
        "INSERT INTO contacts(dxcc, my_dxcc, band, mode, prop_mode, qsl_rcvd, lotw_qsl_rcvd, eqsl_qsl_rcvd) "
        "VALUES (:dxcc, :myDxcc, :band, :mode, :propMode, :qsl, :lotw, :eqsl)")));
    query.bindValue(QStringLiteral(":dxcc"), dxcc);
    query.bindValue(QStringLiteral(":myDxcc"), myDxcc);
    query.bindValue(QStringLiteral(":band"), band);
    query.bindValue(QStringLiteral(":mode"), mode);
    query.bindValue(QStringLiteral(":propMode"), propMode);
    query.bindValue(QStringLiteral(":qsl"), qsl);
    query.bindValue(QStringLiteral(":lotw"), lotw);
    query.bindValue(QStringLiteral(":eqsl"), eqsl);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

QTEST_GUILESS_MAIN(DxccConfirmationTest)

#include "tst_dxccconfirmation.moc"
