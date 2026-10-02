#include <QtTest>
#include <QtNetwork>
#include "data/AntProfile.h"
#include "rotator/drivers/GenericRotDrv.h"
#include "mcpbridge/RuntimeContext.h"
#include "mcpbridge/RuntimeProvider.h"

#define private public
#include "rotator/drivers/HamlibRotDrv.h"
#include "rotator/drivers/PSTRotDrv.h"
#include "rotator/Rotator.h"
#undef private

class RotatorReportingTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rotatorSnapshotTracksConnectionAndReports();
    void pstSnapshotKeepsUnreportedAxisUnknown();
    void hamlibReportsFirstZeroPosition();
    void pstReportsFirstZeroOnEachAxis();
    void pstRejectsInvalidPosition_data();
    void pstRejectsInvalidPosition();
};

void RotatorReportingTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    rig_set_debug(RIG_DEBUG_NONE);
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(":memory:");
    QVERIFY(database.open());
    QSqlQuery query;
    QVERIFY(query.exec("CREATE TABLE ant_profiles (profile_name TEXT, selected INTEGER, "
                       "desc TEXT, azimuth_beamwidth REAL, azimuth_offset REAL, bands TEXT, rot_profile TEXT)"));
    QVERIFY(query.exec("INSERT INTO ant_profiles VALUES ('Test antenna', 1, '', 0, 0, '[]', '')"));
}

void RotatorReportingTest::rotatorSnapshotTracksConnectionAndReports()
{
    Rotator rotator;
    auto provider = rotator.findChild<RuntimeProvider *>();
    QVERIFY(provider);
    RuntimeContext context;
    QVERIFY(context.addProvider("rotator", "Rotator position and connection", provider));
    QObject radioOwner;
    RuntimeProvider radio(&radioOwner);
    radio.addBoolean("rig_connected", "Radio connection", []() { return true; });
    QVERIFY(context.addProvider("rig", "Radio state", &radio));
    const QJsonObject fields = context.schema({"rotator"}).value("fields").toObject();
    QCOMPARE(fields.size(), 6);
    QVERIFY(!fields.contains("rig_connected"));
    QCOMPARE(fields.value("rotator_azimuth").toObject().value("unit").toString(), QString("deg"));
    QCOMPARE(fields.value("rotator_elevation").toObject().value("unit").toString(), QString("deg"));
    for ( const QJsonValue &field : fields )
    {
        const QJsonObject metadata = field.toObject();
        QVERIFY(metadata.value("type").isString());
        QVERIFY(!metadata.value("description").toString().isEmpty());
        QCOMPARE(metadata.value("nullable"), QJsonValue(true));
        QCOMPARE(metadata.value("source").toString(), QString("rotator"));
    }
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 1);
    QJsonObject values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_connected"), QJsonValue(false));
    for ( auto field = values.constBegin(); field != values.constEnd(); ++field )
        if ( field.key() != "rotator_connected" )
            QVERIFY(field.value().isNull());

    RotProfile profile;
    profile.profileName = "Dummy rotator";
    profile.driver = Rotator::HAMLIB_DRIVER;
    profile.model = ROT_MODEL_DUMMY;
    rotator.__openRot(profile);
    QVERIFY(rotator.isRotConnected());
    auto driver = static_cast<HamlibRotDrv *>(rotator.rotDriver);
    driver->stopTimers();
    context.collect(2, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 2);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_connected"), QJsonValue(true));
    QCOMPARE(values.value("rotator_profile").toString(), QString("Dummy rotator"));
    QCOMPARE(values.value("rotator_backend").toString(), QString("Hamlib"));
    QVERIFY(values.value("rotator_model").toString().contains("Dummy"));
    QVERIFY(values.value("rotator_azimuth").isNull());
    QVERIFY(values.value("rotator_elevation").isNull());

    driver->checkAzEl();
    context.collect(3, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 3);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_azimuth"), QJsonValue(0));
    QCOMPARE(values.value("rotator_elevation"), QJsonValue(0));
    // Dummy simulates slow motion. Replace only the hardware read with a fixed report.
    auto originalCaps = driver->rot->caps;
    rot_caps caps = *originalCaps;
    caps.get_position = [](ROT *, azimuth_t *azimuth, elevation_t *elevation) -> int
    {
        *azimuth = 135;
        *elevation = 20;
        return RIG_OK;
    };
    driver->rot->caps = &caps;
    driver->checkAzEl();
    driver->rot->caps = originalCaps;
    context.collect(4, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 4);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_azimuth").toDouble(), 135.0);
    QCOMPARE(values.value("rotator_elevation").toDouble(), 20.0);
    QVERIFY(ready.last().at(1).toJsonObject().value("issues").toObject().isEmpty());

    rotator.__closeRot();
    context.collect(5, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 5);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_connected"), QJsonValue(false));
    QVERIFY(values.value("rotator_azimuth").isNull());
    QVERIFY(values.value("rotator_elevation").isNull());
    QVERIFY(values.value("rotator_profile").isNull());
    QVERIFY(values.value("rotator_backend").isNull());
    QVERIFY(values.value("rotator_model").isNull());
    rotator.__openRot(profile);
    rotator.rotDriver->stopTimers();
    context.collect(6, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 6);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QVERIFY(values.value("rotator_azimuth").isNull());
    QVERIFY(values.value("rotator_elevation").isNull());
}

void RotatorReportingTest::pstSnapshotKeepsUnreportedAxisUnknown()
{
    Rotator rotator;
    RotProfile profile;
    profile.profileName = "PST test";
    profile.driver = Rotator::PSTROTATOR_DRIVER;
    auto driver = new PSTRotDrv(profile, &rotator);
    rotator.rotDriver = driver;
    connect(driver, &GenericRotDrv::positioningChanged, &rotator, &Rotator::driverPositionChanged);
    driver->rotatorAddress = QHostAddress::LocalHost;
    QVERIFY(driver->receiveSocket.bind(QHostAddress(QHostAddress::LocalHost), 0));
    rotator.driverReady();
    RuntimeContext context;
    QVERIFY(context.addProvider("rotator", "Rotator state", rotator.getRuntimeProvider()));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    QUdpSocket sender;
    QCOMPARE(sender.writeDatagram("AZ:0", QHostAddress::LocalHost, driver->receiveSocket.localPort()), qint64(4));
    QTRY_VERIFY(driver->receiveSocket.hasPendingDatagrams());
    driver->readPendingDatagrams();
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    QJsonObject values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_backend").toString(), QString("PSTRotator"));
    QVERIFY(values.value("rotator_model").isNull());
    QCOMPARE(values.value("rotator_azimuth"), QJsonValue(0));
    QVERIFY(values.value("rotator_elevation").isNull());
    QCOMPARE(sender.writeDatagram("EL:0", QHostAddress::LocalHost, driver->receiveSocket.localPort()), qint64(4));
    QTRY_VERIFY(driver->receiveSocket.hasPendingDatagrams());
    driver->readPendingDatagrams();
    context.collect(2);
    QTRY_COMPARE(ready.count(), 2);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_elevation"), QJsonValue(0));

    AntProfile antenna = AntProfilesManager::instance()->getCurProfile1();
    antenna.azimuthOffset = 30;
    AntProfilesManager::instance()->addProfile(antenna.profileName, antenna);
    QCOMPARE(sender.writeDatagram("AZ:350", QHostAddress::LocalHost, driver->receiveSocket.localPort()), qint64(6));
    QTRY_VERIFY(driver->receiveSocket.hasPendingDatagrams());
    driver->readPendingDatagrams();
    antenna.azimuthOffset = 0;
    AntProfilesManager::instance()->addProfile(antenna.profileName, antenna);
    context.collect(3);
    QTRY_COMPARE(ready.count(), 3);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_azimuth"), QJsonValue(20));
    QCOMPARE(values.value("rotator_elevation"), QJsonValue(0));
}

void RotatorReportingTest::hamlibReportsFirstZeroPosition()
{
    RotProfile profile;
    profile.model = ROT_MODEL_DUMMY;
    HamlibRotDrv driver(profile);
    QVERIFY(driver.open());
    driver.stopTimers();
    QSignalSpy position(&driver, &GenericRotDrv::positioningChanged);
    driver.checkAzEl();
    QCOMPARE(position.count(), 1);
    QCOMPARE(position.first().at(0).toDouble(), 0.0);
    QCOMPARE(position.first().at(1).toDouble(), 0.0);
    driver.checkAzEl();
    QCOMPARE(position.count(), 1);
}

void RotatorReportingTest::pstReportsFirstZeroOnEachAxis()
{
    RotProfile profile;
    PSTRotDrv driver(profile);
    driver.rotatorAddress = QHostAddress::LocalHost;
    QVERIFY(driver.receiveSocket.bind(QHostAddress(QHostAddress::LocalHost), 0));
    connect(&driver.receiveSocket, &QUdpSocket::readyRead,
            &driver, &PSTRotDrv::readPendingDatagrams);
    QSignalSpy position(&driver, &GenericRotDrv::positioningChanged);
    QUdpSocket sender;
    QCOMPARE(sender.writeDatagram("AZ:0", QHostAddress::LocalHost, driver.receiveSocket.localPort()), qint64(4));
    QTRY_COMPARE(position.count(), 1);
    QCOMPARE(sender.writeDatagram("EL:0", QHostAddress::LocalHost, driver.receiveSocket.localPort()), qint64(4));
    QTRY_COMPARE(position.count(), 2);
    QCOMPARE(position.last().at(0).toDouble(), 0.0);
    QCOMPARE(position.last().at(1).toDouble(), 0.0);
}

void RotatorReportingTest::pstRejectsInvalidPosition_data()
{
    QTest::addColumn<QByteArray>("report");
    QTest::newRow("az-text") << QByteArray("AZ:garbage");
    QTest::newRow("el-text") << QByteArray("EL:garbage");
    QTest::newRow("az-nan") << QByteArray("AZ:nan");
    QTest::newRow("el-infinity") << QByteArray("EL:inf");
}

void RotatorReportingTest::pstRejectsInvalidPosition()
{
    QFETCH(QByteArray, report);
    RotProfile profile;
    PSTRotDrv driver(profile);
    driver.rotatorAddress = QHostAddress::LocalHost;
    QVERIFY(driver.receiveSocket.bind(QHostAddress(QHostAddress::LocalHost), 0));
    QUdpSocket sender;
    QSignalSpy position(&driver, &GenericRotDrv::positioningChanged);
    driver.sendState();
    QCOMPARE(sender.writeDatagram(report, QHostAddress::LocalHost, driver.receiveSocket.localPort()), qint64(report.size()));
    QTRY_VERIFY(driver.receiveSocket.hasPendingDatagrams());
    driver.readPendingDatagrams();
    QCOMPARE(position.count(), 0);
}

QTEST_GUILESS_MAIN(RotatorReportingTest)

#include "tst_rotatorreporting.moc"
