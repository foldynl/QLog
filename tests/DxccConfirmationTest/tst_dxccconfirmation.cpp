#include <QtTest>
#include <climits>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "data/DxccConfirmation.h"
#include "data/DxccLogStatusCondition.h"
#include "data/DxSpot.h"
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
    void ftxAndDigitalShareCache();
    void retainsClusterWorkingSet();
    void workedConditions();
    void confirmedConditions();
    void conditionStorage();

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

void DxccConfirmationTest::ftxAndDigitalShareCache()
{
    insertContact(400, 1, QStringLiteral("20m"), QStringLiteral("FT8"), QString(),
                  QStringLiteral("N"), QStringLiteral("Y"), QStringLiteral("N"));
    DxccConfirmation confirmation;
    const DxccConfirmationSources sources = DxccConfirmationSource::LotwConfirmation;
    QCOMPARE(confirmation.status(400, 1, QString(), BandPlan::MODE_GROUP_STRING_FTx,
                                 AlertDxccContext::Mode, sources),
             DxccConfirmationResult::Confirmed);

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("DELETE FROM contacts WHERE dxcc = 400")));
    QCOMPARE(confirmation.status(400, 1, QString(), BandPlan::MODE_GROUP_STRING_DIGITAL,
                                 AlertDxccContext::Mode, sources),
             DxccConfirmationResult::Confirmed);
    confirmation.invalidate(400, 1);
    QCOMPARE(confirmation.status(400, 1, QString(), BandPlan::MODE_GROUP_STRING_DIGITAL,
                                 AlertDxccContext::Mode, sources),
             DxccConfirmationResult::NotConfirmed);
}

void DxccConfirmationTest::retainsClusterWorkingSet()
{
    DxccConfirmation confirmation;
    const DxccConfirmationSources sources = DxccConfirmationSource::LotwConfirmation;
    for ( int dxcc = 1; dxcc <= 300; ++dxcc )
        QCOMPARE(confirmation.status(dxcc, 1, QString(), QString(),
                                     AlertDxccContext::Anywhere, sources),
                 DxccConfirmationResult::NotConfirmed);

    insertContact(1, 1, QStringLiteral("20m"), QStringLiteral("FT8"), QString(),
                  QStringLiteral("N"), QStringLiteral("Y"), QStringLiteral("N"));
    // A cache hit must still return the old value until the existing invalidation path runs.
    QCOMPARE(confirmation.status(1, 1, QString(), QString(),
                                 AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::NotConfirmed);
    confirmation.invalidate(1, 1);
    QCOMPARE(confirmation.status(1, 1, QString(), QString(),
                                 AlertDxccContext::Anywhere, sources),
             DxccConfirmationResult::Confirmed);
}

void DxccConfirmationTest::workedConditions()
{
    const QList<DxccStatus> statuses = {NewEntity, NewBand, NewMode, NewBandMode,
                                       NewSlot, Worked, Confirmed, UnknownStatus};
    const QList<QSet<int>> expected = {{NewEntity},
                                      {NewEntity, NewBand, NewBandMode},
                                      {NewEntity, NewMode, NewBandMode},
                                      {NewEntity, NewBand, NewMode, NewBandMode, NewSlot}};
    DxccLogStatusCondition condition;
    condition.goal = static_cast<int>(AlertDxccGoal::Worked);
    condition.goalSet = condition.contextSet = true;
    for ( int context = 0; context < 4; ++context )
    {
        condition.context = context;
        for ( DxccStatus status : statuses )
        {
            DxSpot spot;
            spot.status = status;
            QCOMPARE(condition.match(spot), expected.at(context).contains(status));
            spot.dxccStatusSatellite = true;
            QCOMPARE(condition.match(spot), status == NewEntity);
        }
    }
}

void DxccConfirmationTest::confirmedConditions()
{
    DxSpot spot;
    spot.dxcc.dxcc = 100;
    spot.band = QStringLiteral("20m");
    spot.modeGroupString = BandPlan::MODE_GROUP_STRING_DIGITAL;
    spot.status = Worked;
    DxccLogStatusCondition condition;
    condition.goalSet = condition.contextSet = true;
    int lookups = 0;
    bool argumentsMatch = true;
    DxccConfirmationResult result = DxccConfirmationResult::Confirmed;
    const DxccConfirmationResolver resolver = [&](int dxcc, const QString &band,
                                                   const QString &mode, AlertDxccContext context)
    {
        ++lookups;
        argumentsMatch = argumentsMatch && dxcc == spot.dxcc.dxcc && band == spot.band
                         && mode == spot.modeGroupString
                         && static_cast<int>(context) == condition.context;
        return result;
    };
    for ( int context = 0; context < 4; ++context )
    {
        condition.context = context;
        result = DxccConfirmationResult::Confirmed;
        QCOMPARE(condition.match(spot, resolver), context == 3);
        result = DxccConfirmationResult::NotConfirmed;
        QVERIFY(condition.match(spot, resolver));
        result = DxccConfirmationResult::Error;
        QCOMPARE(condition.match(spot, resolver), context == 3);
    }
    QCOMPARE(lookups, 9);
    QVERIFY(argumentsMatch);
    condition.context = 0;
    QVERIFY(!condition.match(spot));
    spot.status = NewEntity;
    QVERIFY(condition.match(spot));
    spot.status = UnknownStatus;
    QVERIFY(!condition.match(spot, resolver));
    spot.dxccStatusSatellite = true;
    QVERIFY(!condition.match(spot, resolver));
    spot.status = Worked;
    QVERIFY(condition.match(spot));
    spot.status = Confirmed;
    QVERIFY(!condition.match(spot));
    condition.goalSet = false;
    QVERIFY(!condition.match(spot, resolver));
    QCOMPARE(lookups, 9);
    QVERIFY(DxccLogStatusCondition(DxccStatus::All).match(spot));
    // NewBandMode is a composite value: preserve legacy bit-mask matching.
    spot.status = NewBandMode;
    QVERIFY(DxccLogStatusCondition(NewMode).match(spot));
}

void DxccConfirmationTest::conditionStorage()
{
    for ( int mask = 0; mask <= DxccStatus::All; ++mask )
    {
        const DxccLogStatusCondition original(mask);
        const DxccLogStatusCondition stored = DxccLogStatusCondition::fromJson(original.toJson());
        QCOMPARE(stored.mask, mask);
        QVERIFY(!stored.goalSet);
        QVERIFY(!stored.contextSet);
    }
    const QList<int> rawMasks = {-1, 128, 255, INT_MIN, INT_MAX};
    for ( int mask : rawMasks )
    {
        const DxccLogStatusCondition original(mask);
        const DxccLogStatusCondition stored = DxccLogStatusCondition::fromJson(original.toJson());
        QCOMPARE(stored.mask, mask);
        QVERIFY(!stored.isInvalid());
    }
    DxccLogStatusCondition original(NewMode);
    original.goalSet = original.contextSet = true;
    original.goal = static_cast<int>(AlertDxccGoal::Worked);
    original.context = static_cast<int>(AlertDxccContext::Mode);
    const DxccLogStatusCondition stored = DxccLogStatusCondition::fromJson(original.toJson());
    QCOMPARE(stored.mask, original.mask);
    QCOMPARE(stored.goal, original.goal);
    QCOMPARE(stored.context, original.context);
    QVERIFY(stored.hasCondition());

    const QStringList invalid = {QStringLiteral("not json"), QStringLiteral("{}"),
        QStringLiteral("{\"mask\":127,\"goal\":1}"),
        QStringLiteral("{\"mask\":127,\"goal\":999,\"context\":0}"),
        QStringLiteral("{\"mask\":127,\"goal\":1.5,\"context\":0}"),
        QStringLiteral("{\"mask\":127,\"goal\":1,\"context\":0.5}"),
        QStringLiteral("{\"mask\":1.5}"), QStringLiteral("{\"mask\":\"invalid\"}"),
        QStringLiteral("{\"mask\":2147483648}")};
    DxSpot spot;
    spot.status = NewEntity;
    for ( const QString &value : invalid )
    {
        const DxccLogStatusCondition condition = DxccLogStatusCondition::fromJson(value);
        QVERIFY2(condition.isInvalid(), qPrintable(value));
        QVERIFY(!condition.match(spot));
        QVERIFY(DxccLogStatusCondition::fromJson(condition.toJson()).isInvalid());
    }
    original.contextSet = false;
    const DxccLogStatusCondition partial = DxccLogStatusCondition::fromJson(original.toJson());
    QVERIFY(partial.goalSet);
    QVERIFY(!partial.contextSet);
    QCOMPARE(partial.goal, original.goal);
    QCOMPARE(partial.mask, original.mask);
    QVERIFY(partial.isInvalid());
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
