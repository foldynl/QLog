#include <QtTest>
#include <QThread>
#include <limits>

#include "mcpbridge/RuntimeContext.h"
#include "mcpbridge/RuntimeProvider.h"
#include "rig/RigRuntimeProvider.h"
#include "rotator/RotatorRuntimeProvider.h"

class RuntimeWorker : public QThread
{
public:
    explicit RuntimeWorker(QObject *owner)
    {
        owner->moveToThread(this);
        connect(this, &QThread::finished, owner, &QObject::deleteLater);
    }

    ~RuntimeWorker()
    {
        if ( !isRunning() && !isFinished() )
            start();
        quit();
        wait();
    }
};

class RuntimeContextTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rigSchemaAndSnapshot();
    void disconnectedRigDoesNotExposeOldValues();
    void invalidFrequencyIsUnavailable();
    void simplexTxFrequencyWithoutCatOffsets();
    void adjustedFrequenciesNeedMainVfoReport();
    void extendedRigStateTracksAvailabilityAndChanges();
    void extendedRigStateRejectsInvalidValues_data();
    void extendedRigStateRejectsInvalidValues();
    void extendedRigStateInWorkerThread();
    void rotatorStateTracksAvailability();
    void rotatorStateInWorkerThread();
    void readsInOwnerThread();
    void addingFieldUsesSameCollectionPath();
    void fieldWithoutDescriptionCannotBeRegistered();
    void registeredFieldsCannotChange();
    void readsAllFieldsBeforeQueuedMutation();
    void invalidValuesAreUnavailable();
    void invalidFieldDefinitionsAreRejected();
    void timeoutKeepsOtherProviderValues();
    void lateReplyCannotCompleteAnotherRequest();
    void providerDestructionCompletesPendingRequest();
    void workerShutdownRemovesProvider();
    void contextDestructionBeforeReply();
    void duplicateFieldsAreRejected();
    void overlappingRequestIsBusy();
    void sourceMetadataAndSelectedSchema();
    void invalidSourceRegistration_data();
    void invalidSourceRegistration();
    void selectionLimitsBusyAndTimeoutIssues();
    void selectionCanBeRequestedFromAnotherThread();
};

void RuntimeContextTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void RuntimeContextTest::rigSchemaAndSnapshot()
{
    QObject owner;
    Rig::Status status;
    status.isConnected = true;
    status.profile = "Home";
    status.modelName = "Yaesu FT-950 (20230104.0)";
    status.freq = 14.074;
    status.mode = "MFSK";
    status.submode = "FT8";
    auto provider = createRigRuntimeProvider(&owner, status);
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    const QJsonObject fields = context.schema().value("fields").toObject();
    QCOMPARE(fields.size(), 17);
    for ( const QJsonValue &field : fields )
    {
        const QJsonObject metadata = field.toObject();
        QVERIFY(metadata.value("type").isString());
        QVERIFY(metadata.value("description").isString());
        QVERIFY(!metadata.value("description").toString().isEmpty());
        QVERIFY(metadata.value("nullable").isBool());
        QCOMPARE(metadata.value("nullable").toBool(), true);
    }
    QCOMPARE(fields.value("rig_connected").toObject().value("type").toString(), QString("boolean"));
    QCOMPARE(fields.value("rig_profile").toObject().value("type").toString(), QString("string"));
    QCOMPARE(fields.value("rig_frequency").toObject().value("type").toString(), QString("number"));
    QCOMPARE(fields.value("rig_frequency").toObject().value("unit").toString(), QString("Hz"));
    QCOMPARE(fields.value("rig_backend").toObject().value("type").toString(), QString("string"));
    QCOMPARE(fields.value("rig_model").toObject().value("type").toString(), QString("string"));
    QCOMPARE(fields.value("rig_vfo").toObject().value("type").toString(), QString("string"));
    QCOMPARE(fields.value("rig_split_enabled").toObject().value("type").toString(), QString("boolean"));
    QCOMPARE(fields.value("rig_power").toObject().value("unit").toString(), QString("W"));
    QCOMPARE(fields.value("rig_key_speed").toObject().value("unit").toString(), QString("WPM"));
    for ( const QString &name : {QString("rig_rit"), QString("rig_xit"),
                                 QString("rig_bandwidth"), QString("rig_tx_frequency"),
                                 QString("rig_rx_frequency"), QString("rig_rx_offset"),
                                 QString("rig_tx_offset")} )
        QCOMPARE(fields.value(name).toObject().value("unit").toString(), QString("Hz"));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);

    context.collect(7);
    QCOMPARE(ready.count(), 0);
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(7));
    const QJsonObject result = ready.first().at(1).toJsonObject();
    const QJsonObject values = result.value("values").toObject();
    QCOMPARE(values.value("rig_connected").toBool(), true);
    QCOMPARE(values.value("rig_profile").toString(), QString("Home"));
    QCOMPARE(values.value("rig_model").toString(), status.modelName);
    QCOMPARE(values.value("rig_frequency").toDouble(), 14074000.0);
    QCOMPARE(values.value("rig_mode").toString(), QString("FT8"));
    QVERIFY(result.value("issues").toObject().isEmpty());

    ready.clear();
    status.submode.clear();
    status.mode = "CW";
    context.collect(8);
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(1).toJsonObject().value("values").toObject()
             .value("rig_mode").toString(), QString("CW"));
}

void RuntimeContextTest::disconnectedRigDoesNotExposeOldValues()
{
    QObject owner;
    Rig::Status status;
    status.profile = "Old radio";
    status.freq = 14.074;
    status.mode = "CW";
    status.backend = "Hamlib";
    status.modelName = "Yaesu FT-950 (20230104.0)";
    status.power = 50;
    status.powerKnown = true;
    status.keySpeed = 25;
    status.keySpeedKnown = true;
    status.vfo = "A";
    status.vfoKnown = true;
    status.ritKnown = true;
    status.xitKnown = true;
    status.bandwidth = 2400;
    status.bandwidthKnown = true;
    status.splitEnabled = true;
    status.splitKnown = true;
    status.txFreq = 14.076;
    status.txFrequencyKnown = true;
    status.rxFreq = 14.0745;
    status.mainTxFreq = 14.0735;
    status.rxOffset = 0.0005;
    status.txOffset = -0.0005;
    auto provider = createRigRuntimeProvider(&owner, status);
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    const QJsonObject values = ready.first().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_connected").toBool(), false);
    QVERIFY(values.value("rig_profile").isNull());
    QVERIFY(values.value("rig_frequency").isNull());
    QVERIFY(values.value("rig_mode").isNull());
    for ( auto field = values.constBegin(); field != values.constEnd(); ++field )
        if ( field.key() != "rig_connected" )
            QVERIFY2(field.value().isNull(), qPrintable(field.key()));
}

void RuntimeContextTest::invalidFrequencyIsUnavailable()
{
    QObject owner;
    Rig::Status status;
    status.isConnected = true;
    auto provider = createRigRuntimeProvider(&owner, status);
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    const QList<double> frequencies = {0.0, -1.0, std::numeric_limits<double>::infinity(),
                                       std::numeric_limits<double>::quiet_NaN(),
                                       std::numeric_limits<double>::max()};
    for ( const double frequency : frequencies )
    {
        status.freq = frequency;
        status.rxFreq = frequency;
        status.mainTxFreq = frequency;
        ready.clear();
        context.collect(1);
        QTRY_COMPARE(ready.count(), 1);
        const QJsonObject values = ready.first().at(1).toJsonObject().value("values").toObject();
        QVERIFY(values.value("rig_frequency").isNull());
        QVERIFY(values.value("rig_rx_frequency").isNull());
        QVERIFY(values.value("rig_tx_frequency").isNull());
    }
}

void RuntimeContextTest::simplexTxFrequencyWithoutCatOffsets()
{
    QObject owner;
    Rig::Status status;
    status.isConnected = true;
    status.freq = 14.074;
    status.rxFreq = 14.0745;
    status.mainTxFreq = 14.0735;
    status.rxOffset = 0.0005;
    status.txOffset = -0.0005;
    auto provider = createRigRuntimeProvider(&owner, status);
    QSignalSpy ready(provider, &RuntimeProvider::snapshotReady);
    provider->collect(1);
    QCOMPARE(ready.count(), 1);
    QJsonObject values = ready.first().at(1).toJsonObject();
    QCOMPARE(values.value("rig_frequency"), QJsonValue(14074000));
    QCOMPARE(values.value("rig_rx_frequency"), QJsonValue(14074500));
    QCOMPARE(values.value("rig_tx_frequency"), QJsonValue(14073500));
    QCOMPARE(values.value("rig_rx_offset"), QJsonValue(500));
    QCOMPARE(values.value("rig_tx_offset"), QJsonValue(-500));
    QVERIFY(values.value("rig_rit").isNull());
    QVERIFY(values.value("rig_xit").isNull());
    QVERIFY(values.value("rig_split_enabled").isNull());

    status.splitKnown = true;
    status.ritKnown = true;
    status.xitKnown = true;
    status.rit = 0.000125;
    status.xit = -0.00025;
    provider->collect(2);
    values = ready.last().at(1).toJsonObject();
    QCOMPARE(values.value("rig_split_enabled"), QJsonValue(false));
    QCOMPARE(values.value("rig_rx_frequency"), QJsonValue(14074500));
    QCOMPARE(values.value("rig_tx_frequency"), QJsonValue(14073500));
    QCOMPARE(values.value("rig_rx_offset"), QJsonValue(500));
    QCOMPARE(values.value("rig_tx_offset"), QJsonValue(-500));
    QCOMPARE(values.value("rig_rit"), QJsonValue(125));
    QCOMPARE(values.value("rig_xit"), QJsonValue(-250));

    status.splitEnabled = true;
    provider->collect(3);
    QVERIFY(ready.last().at(1).toJsonObject().value("rig_tx_frequency").isNull());

    status.txFreq = 14.076;
    status.txFrequencyKnown = true;
    status.mainTxFreq = 14.0755;
    provider->collect(4);
    QCOMPARE(ready.last().at(1).toJsonObject().value("rig_tx_frequency"), QJsonValue(14076000));

    status.splitEnabled = false;
    provider->collect(5);
    QCOMPARE(ready.last().at(1).toJsonObject().value("rig_tx_frequency"), QJsonValue(14075500));

    status.clear();
    status.isConnected = true;
    provider->collect(6);
    values = ready.last().at(1).toJsonObject();
    QVERIFY(values.value("rig_rx_frequency").isNull());
    QVERIFY(values.value("rig_tx_frequency").isNull());
    QCOMPARE(values.value("rig_rx_offset"), QJsonValue(0));
    QCOMPARE(values.value("rig_tx_offset"), QJsonValue(0));
}

void RuntimeContextTest::adjustedFrequenciesNeedMainVfoReport()
{
    QObject owner;
    Rig::Status status;
    status.isConnected = true;
    // Offset notifications can precede the first main-VFO frequency report.
    status.rxFreq = 0.0005;
    status.mainTxFreq = 0.00025;
    auto provider = createRigRuntimeProvider(&owner, status);
    QSignalSpy ready(provider, &RuntimeProvider::snapshotReady);
    provider->collect(1);
    QJsonObject values = ready.last().at(1).toJsonObject();
    QVERIFY(values.value("rig_rx_frequency").isNull());
    QVERIFY(values.value("rig_tx_frequency").isNull());

    status.freq = 14.074;
    status.rxFreq = 14.0745;
    status.mainTxFreq = 14.07425;
    provider->collect(2);
    values = ready.last().at(1).toJsonObject();
    QCOMPARE(values.value("rig_rx_frequency"), QJsonValue(14074500));
    QCOMPARE(values.value("rig_tx_frequency"), QJsonValue(14074250));

    status.freq = 0;
    status.splitKnown = true;
    status.splitEnabled = true;
    status.txFrequencyKnown = true;
    status.txFreq = 14.076;
    provider->collect(3);
    values = ready.last().at(1).toJsonObject();
    QVERIFY(values.value("rig_rx_frequency").isNull());
    QCOMPARE(values.value("rig_tx_frequency"), QJsonValue(14076000));
}

void RuntimeContextTest::extendedRigStateTracksAvailabilityAndChanges()
{
    QObject owner;
    Rig::Status status;
    status.isConnected = true;
    status.backend = "Hamlib";
    status.keySpeed = 25;
    status.vfo = "A";
    status.xit = -0.00125;
    status.bandwidth = 500;
    status.txFreq = 14.076;
    auto provider = createRigRuntimeProvider(&owner, status);
    RuntimeContext context;
    QVERIFY(context.addProvider("rig", "Radio state", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    QJsonObject values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_backend").toString(), QString("Hamlib"));
    QVERIFY(values.value("rig_model").isNull());
    const QStringList readings{"rig_power", "rig_key_speed", "rig_vfo", "rig_rit", "rig_xit",
                               "rig_bandwidth", "rig_split_enabled", "rig_tx_frequency"};
    for ( const QString &name : readings )
        QVERIFY2(values.value(name).isNull(), qPrintable(name));

    status.powerKnown = true;
    status.modelName = "Yaesu FT-950 (20230104.0)";
    status.keySpeedKnown = true;
    status.vfoKnown = true;
    status.ritKnown = true;
    status.xitKnown = true;
    status.bandwidthKnown = true;
    status.splitKnown = true;
    status.txFrequencyKnown = true;
    context.collect(2);
    QTRY_COMPARE(ready.count(), 2);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_power"), QJsonValue(0));
    QCOMPARE(values.value("rig_model").toString(), status.modelName);
    QCOMPARE(values.value("rig_key_speed"), QJsonValue(25));
    QCOMPARE(values.value("rig_vfo").toString(), QString("A"));
    QCOMPARE(values.value("rig_rit"), QJsonValue(0));
    QCOMPARE(values.value("rig_xit"), QJsonValue(-1250));
    QCOMPARE(values.value("rig_bandwidth"), QJsonValue(500));
    QCOMPARE(values.value("rig_split_enabled"), QJsonValue(false));
    QVERIFY(values.value("rig_tx_frequency").isNull());

    status.splitEnabled = true;
    status.txFrequencyKnown = false;
    context.collect(3);
    QTRY_COMPARE(ready.count(), 3);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_split_enabled"), QJsonValue(true));
    QVERIFY(values.value("rig_tx_frequency").isNull());

    status.txFrequencyKnown = true;
    status.power = 50;
    status.keySpeed = 18;
    status.vfo = "B";
    status.rit = 0.0005;
    status.xit = 0;
    context.collect(4);
    QTRY_COMPARE(ready.count(), 4);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_power"), QJsonValue(50));
    QCOMPARE(values.value("rig_key_speed"), QJsonValue(18));
    QCOMPARE(values.value("rig_vfo").toString(), QString("B"));
    QCOMPARE(values.value("rig_rit"), QJsonValue(500));
    QCOMPARE(values.value("rig_xit"), QJsonValue(0));
    QCOMPARE(values.value("rig_tx_frequency"), QJsonValue(14076000));

    status.splitEnabled = false;
    context.collect(5);
    QTRY_COMPARE(ready.count(), 5);
    QVERIFY(ready.last().at(1).toJsonObject().value("values").toObject()
            .value("rig_tx_frequency").isNull());

    status.clear();
    status.isConnected = true;
    status.backend = "Omnirig v1";
    context.collect(6);
    QTRY_COMPARE(ready.count(), 6);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_backend").toString(), QString("Omnirig v1"));
    QVERIFY(values.value("rig_model").isNull());
    for ( const QString &name : readings )
        QVERIFY2(values.value(name).isNull(), qPrintable(name));
    status.clear();
    QVERIFY(status.backend.isEmpty());
    QVERIFY(status.modelName.isEmpty());
}

void RuntimeContextTest::extendedRigStateRejectsInvalidValues_data()
{
    QTest::addColumn<double>("number");
    QTest::newRow("negative") << -1.0;
    QTest::newRow("zero") << 0.0;
    QTest::newRow("infinity") << std::numeric_limits<double>::infinity();
    QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN();
    QTest::newRow("hz-overflow") << std::numeric_limits<double>::max();
}

void RuntimeContextTest::extendedRigStateRejectsInvalidValues()
{
    QFETCH(double, number);
    QObject owner;
    Rig::Status status;
    status.isConnected = true;
    status.powerKnown = true;
    status.keySpeedKnown = true;
    status.vfoKnown = true;
    status.ritKnown = true;
    status.xitKnown = true;
    status.bandwidthKnown = true;
    status.splitKnown = true;
    status.splitEnabled = true;
    status.txFrequencyKnown = true;
    status.power = number;
    status.rit = number;
    status.xit = number;
    status.rxOffset = number;
    status.txOffset = number;
    status.txFreq = number;
    status.vfo.clear();
    status.bandwidth = -1;
    auto provider = createRigRuntimeProvider(&owner, status);
    QSignalSpy ready(provider, &RuntimeProvider::snapshotReady);
    provider->collect(1);
    QCOMPARE(ready.count(), 1);
    const QJsonObject values = ready.first().at(1).toJsonObject();
    QVERIFY(values.value("rig_key_speed").isNull());
    QVERIFY(values.value("rig_vfo").isNull());
    QVERIFY(values.value("rig_bandwidth").isNull());
    QVERIFY(values.value("rig_tx_frequency").isNull());
    if ( qIsFinite(number) && number >= 0 )
        QCOMPARE(values.value("rig_power"), QJsonValue(number));
    else
        QVERIFY(values.value("rig_power").isNull());
    if ( qIsFinite(number * 1000000.0) )
    {
        QCOMPARE(values.value("rig_rit"), QJsonValue(number * 1000000.0));
        QCOMPARE(values.value("rig_xit"), QJsonValue(number * 1000000.0));
        QCOMPARE(values.value("rig_rx_offset"), QJsonValue(number * 1000000.0));
        QCOMPARE(values.value("rig_tx_offset"), QJsonValue(number * 1000000.0));
    }
    else
    {
        QVERIFY(values.value("rig_rit").isNull());
        QVERIFY(values.value("rig_xit").isNull());
        QVERIFY(values.value("rig_rx_offset").isNull());
        QVERIFY(values.value("rig_tx_offset").isNull());
    }
}

void RuntimeContextTest::extendedRigStateInWorkerThread()
{
    Rig::Status status;
    auto owner = new QObject;
    auto provider = createRigRuntimeProvider(owner, status);
    RuntimeWorker worker(owner);
    RuntimeContext context;
    QVERIFY(context.addProvider("rig", "Radio state", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    worker.start();
    QVERIFY(QMetaObject::invokeMethod(owner, [&status]()
    {
        status.isConnected = true;
        status.backend = "FLRig";
        status.powerKnown = true;
        status.power = 10;
        status.freq = 7.024;
        status.rxFreq = 7.0245;
        status.mainTxFreq = 7.0255;
        status.rxOffset = -0.0005;
        status.txOffset = 0.0005;
        status.splitKnown = true;
        status.splitEnabled = true;
        status.txFrequencyKnown = true;
        status.txFreq = 7.025;
    }, Qt::QueuedConnection));
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    const QJsonObject values = ready.first().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rig_backend").toString(), QString("FLRig"));
    QVERIFY(values.value("rig_model").isNull());
    QCOMPARE(values.value("rig_power"), QJsonValue(10));
    QCOMPARE(values.value("rig_rx_frequency"), QJsonValue(7024500));
    QCOMPARE(values.value("rig_rx_offset"), QJsonValue(-500));
    QCOMPARE(values.value("rig_tx_offset"), QJsonValue(500));
    QCOMPARE(values.value("rig_tx_frequency"), QJsonValue(7025000));
}

void RuntimeContextTest::rotatorStateTracksAvailability()
{
    Rotator::Status status;
    QObject owner;
    auto provider = createRotatorRuntimeProvider(&owner, status);
    QSignalSpy ready(provider, &RuntimeProvider::snapshotReady);
    status.isConnected = true;
    provider->collect(1);
    QJsonObject values = ready.last().at(1).toJsonObject();
    QVERIFY(values.value("rotator_azimuth").isNull());
    QVERIFY(values.value("rotator_elevation").isNull());
    status.azimuthKnown = true;
    provider->collect(2);
    values = ready.last().at(1).toJsonObject();
    QCOMPARE(values.value("rotator_azimuth"), QJsonValue(0));
    QVERIFY(values.value("rotator_elevation").isNull());
    status.elevationKnown = true;
    status.elevation = -10;
    provider->collect(3);
    QCOMPARE(ready.last().at(1).toJsonObject().value("rotator_elevation"), QJsonValue(-10));
    status.elevation = 120;
    provider->collect(4);
    QCOMPARE(ready.last().at(1).toJsonObject().value("rotator_elevation"), QJsonValue(120));
    for ( const double angle : {-1.0, 360.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()} )
    {
        status.azimuth = angle;
        provider->collect(5);
        QVERIFY(ready.last().at(1).toJsonObject().value("rotator_azimuth").isNull());
    }
    for ( const double angle : {std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()} )
    {
        status.elevation = angle;
        provider->collect(6);
        QVERIFY(ready.last().at(1).toJsonObject().value("rotator_elevation").isNull());
    }
    status.isConnected = false;
    status.azimuth = 90;
    status.elevation = 45;
    status.profile = "Old profile";
    status.backend = "Hamlib";
    status.modelName = "Old model";
    provider->collect(7);
    values = ready.last().at(1).toJsonObject();
    QCOMPARE(values.value("rotator_connected"), QJsonValue(false));
    for ( auto field = values.constBegin(); field != values.constEnd(); ++field )
        if ( field.key() != "rotator_connected" )
            QVERIFY2(field.value().isNull(), qPrintable(field.key()));
}

void RuntimeContextTest::rotatorStateInWorkerThread()
{
    Rotator::Status status;
    auto owner = new QObject;
    auto provider = createRotatorRuntimeProvider(owner, status);
    RuntimeWorker worker(owner);
    RuntimeContext context;
    QVERIFY(context.addProvider("rotator", "Rotator state", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    worker.start();
    QVERIFY(QMetaObject::invokeMethod(owner, [&status]()
    {
        status.isConnected = true;
        status.profile = "Roof";
        status.backend = "PSTRotator";
        status.azimuth = 270;
        status.azimuthKnown = true;
    }, Qt::QueuedConnection));
    context.collect(1, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 1);
    QJsonObject values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_connected"), QJsonValue(true));
    QCOMPARE(values.value("rotator_profile").toString(), QString("Roof"));
    QCOMPARE(values.value("rotator_backend").toString(), QString("PSTRotator"));
    QCOMPARE(values.value("rotator_azimuth"), QJsonValue(270));
    QVERIFY(values.value("rotator_elevation").isNull());
    QVERIFY(QMetaObject::invokeMethod(owner, [&status]()
    {
        status.azimuth = 90;
        status.elevation = 0;
        status.elevationKnown = true;
    }, Qt::QueuedConnection));
    context.collect(2, 1000, {"rotator"});
    QTRY_COMPARE(ready.count(), 2);
    values = ready.last().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("rotator_azimuth"), QJsonValue(90));
    QCOMPARE(values.value("rotator_elevation"), QJsonValue(0));
    QVERIFY(ready.last().at(1).toJsonObject().value("issues").toObject().isEmpty());
}

void RuntimeContextTest::readsInOwnerThread()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addString("rig_profile", "Thread owning the state", [owner]()
    {
        return QJsonValue(QThread::currentThread() == owner->thread() ? "worker" : "wrong thread");
    });
    RuntimeWorker worker(owner);
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    worker.start();
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(1).toJsonObject().value("values").toObject()
             .value("rig_profile").toString(), QString("worker"));
}

void RuntimeContextTest::addingFieldUsesSameCollectionPath()
{
    QObject owner;
    int reads = 0;
    auto provider = new RuntimeProvider(&owner);
    QVERIFY(provider->addNumber("future_field", "A newly registered field", QString(),
                                [&]() { return QJsonValue(++reads); }));
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QVERIFY(context.schema().value("fields").toObject().contains("future_field"));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(1).toJsonObject().value("values").toObject()
             .value("future_field").toInt(), 1);
}

void RuntimeContextTest::fieldWithoutDescriptionCannotBeRegistered()
{
    QObject owner;
    auto provider = new RuntimeProvider(&owner);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot define runtime field.*"));
    QVERIFY(!provider->addNumber("unknown_value", QString(), QString(), []() { return 42; }));
    RuntimeContext context;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Runtime provider has no valid field.*"));
    QVERIFY(!context.addProvider("provider", "Test source provider", provider));
}

void RuntimeContextTest::registeredFieldsCannotChange()
{
    QObject owner;
    auto provider = new RuntimeProvider(&owner);
    QVERIFY(provider->addBoolean("connected", "Ready connection", []() { return true; }));
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot define runtime field.*"));
    QVERIFY(!provider->addString("late_field", "Added too late", []() { return QString("late"); }));
    QVERIFY(!provider->schema().contains("late_field"));
    QVERIFY(!context.schema().value("fields").toObject().contains("late_field"));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    const QJsonObject values = ready.first().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.size(), 1);
    QCOMPARE(values.value("connected").toBool(), true);
}

void RuntimeContextTest::readsAllFieldsBeforeQueuedMutation()
{
    QObject owner;
    int value = 42;
    auto provider = new RuntimeProvider(&owner);
    provider->addNumber("first", "First reading", QString(), [&]()
    {
        QTimer::singleShot(0, &owner, [&]() { value = 17; });
        return value;
    });
    provider->addNumber("second", "Second reading", QString(), [&]() { return value; });
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    const QJsonObject values = ready.first().at(1).toJsonObject().value("values").toObject();
    QCOMPARE(values.value("first").toInt(), 42);
    QCOMPARE(values.value("second").toInt(), 42);
    QTRY_COMPARE(value, 17);
    context.collect(2);
    QTRY_COMPARE(ready.count(), 2);
    QCOMPARE(ready.at(1).at(1).toJsonObject().value("values").toObject().value("second").toInt(), 17);
}

void RuntimeContextTest::invalidValuesAreUnavailable()
{
    QObject owner;
    auto provider = new RuntimeProvider(&owner);
    provider->addBoolean("wrong_type", "Invalid boolean", []() { return QString("true"); });
    provider->addNumber("infinite", "Invalid number", QString(),
                         []() { return std::numeric_limits<double>::infinity(); });
    provider->addString("undefined", "Missing string", []() { return QJsonValue(QJsonValue::Undefined); });
    provider->addString("unavailable", "Unknown string", []() { return QJsonValue(); });
    provider->addNumber("available", "Available number", QString(), []() { return 42; });
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Invalid runtime field value.*wrong_type.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Invalid runtime field value.*infinite.*"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Invalid runtime field value.*undefined.*"));
    context.collect(1);
    QTRY_COMPARE(ready.count(), 1);
    const QJsonObject result = ready.first().at(1).toJsonObject();
    const QJsonObject values = result.value("values").toObject();
    QVERIFY(values.value("wrong_type").isNull());
    QVERIFY(values.value("infinite").isNull());
    QVERIFY(values.value("undefined").isNull());
    QVERIFY(values.value("unavailable").isNull());
    QCOMPARE(values.value("available").toInt(), 42);
    QVERIFY(result.value("issues").toObject().isEmpty());
}

void RuntimeContextTest::invalidFieldDefinitionsAreRejected()
{
    QObject owner;
    auto provider = new RuntimeProvider(&owner);
    QVERIFY(provider->addBoolean("connected", "Ready connection", []() { return true; }));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot define runtime field.*"));
    QVERIFY(!provider->addString("connected", "Duplicate field", []() { return QString("wrong"); }));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot define runtime field.*"));
    QVERIFY(!provider->addBoolean(" ", "Missing name", []() { return true; }));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Cannot define runtime field.*"));
    QVERIFY(!provider->addBoolean("missing_getter", "Missing getter", std::function<QJsonValue()>()));
    QCOMPARE(provider->schema().size(), 1);
    QCOMPARE(provider->schema().value("connected").toObject().value("type").toString(), QString("boolean"));
}

void RuntimeContextTest::timeoutKeepsOtherProviderValues()
{
    auto stalledOwner = new QObject;
    auto stalled = new RuntimeProvider(stalledOwner);
    stalled->addBoolean("rig_connected", "Ready radio connection", []() { return false; });
    RuntimeWorker worker(stalledOwner);
    QObject owner;
    auto available = new RuntimeProvider(&owner);
    available->addNumber("other_state", "Available state", QString(), []() { return 42; });
    RuntimeContext context;
    QVERIFY(context.addProvider("stalled", "Test source stalled", stalled));
    QVERIFY(context.addProvider("available", "Test source available", available));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1, 10);
    QTRY_COMPARE(ready.count(), 1);
    const QJsonObject result = ready.first().at(1).toJsonObject();
    QCOMPARE(result.value("values").toObject().value("other_state").toInt(), 42);
    QVERIFY(result.value("values").toObject().value("rig_connected").isNull());
    QCOMPARE(result.value("issues").toObject().value("rig_connected").toString(),
             QString("provider_timeout"));
}

void RuntimeContextTest::lateReplyCannotCompleteAnotherRequest()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("rig_connected", "Ready radio connection", []() { return false; });
    RuntimeWorker worker(owner);
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    QSignalSpy snapshots(provider, &RuntimeProvider::snapshotReady);
    context.collect(1, 10);
    QTRY_COMPARE(ready.count(), 1);
    context.collect(2, 10);
    QCOMPARE(ready.count(), 2);
    QCOMPARE(ready.at(1).at(1).toJsonObject().value("issues").toObject()
             .value("rig_connected").toString(), QString("provider_busy"));
    worker.start();
    QTRY_COMPARE(snapshots.count(), 1);
    QTRY_COMPARE(ready.count(), 2);
    context.collect(3);
    QTRY_COMPARE(ready.count(), 3);
    QCOMPARE(ready.at(2).at(0).toULongLong(), quint64(3));
    QCOMPARE(ready.at(2).at(1).toJsonObject().value("values").toObject()
             .value("rig_connected").toBool(), false);
    QTRY_COMPARE(snapshots.count(), 2);
}

void RuntimeContextTest::providerDestructionCompletesPendingRequest()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("rig_connected", "Ready radio connection", []() { return false; });
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    delete owner;
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(1).toJsonObject().value("issues").toObject()
             .value("rig_connected").toString(), QString("provider_unavailable"));
    QVERIFY(context.schema().value("fields").toObject().isEmpty());
}

void RuntimeContextTest::workerShutdownRemovesProvider()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("rig_connected", "Ready radio connection", []() { return false; });
    RuntimeWorker worker(owner);
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    worker.start();
    worker.quit();
    QVERIFY(worker.wait(1000));
    QTRY_VERIFY(context.schema().value("fields").toObject().isEmpty());
    QVERIFY(context.sources().value("sources").toObject().isEmpty());
    QVERIFY(!context.hasSource("provider"));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    QCOMPARE(ready.count(), 1);
    QVERIFY(ready.first().at(1).toJsonObject().value("values").toObject().isEmpty());
}

void RuntimeContextTest::contextDestructionBeforeReply()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("rig_connected", "Ready radio connection", []() { return false; });
    RuntimeWorker worker(owner);
    auto context = new RuntimeContext;
    QVERIFY(context->addProvider("provider", "Test source provider", provider));
    QSignalSpy snapshots(provider, &RuntimeProvider::snapshotReady);
    context->collect(1);
    delete context;
    worker.start();
    QTRY_COMPARE(snapshots.count(), 1);
}

void RuntimeContextTest::duplicateFieldsAreRejected()
{
    QObject owner;
    Rig::Status status;
    auto first = createRigRuntimeProvider(&owner, status);
    auto second = createRigRuntimeProvider(&owner, status);
    RuntimeContext context;
    QVERIFY(context.addProvider("first", "Test source first", first));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Duplicate runtime field.*"));
    QVERIFY(!context.addProvider("second", "Test source second", second));
    QCOMPARE(context.schema().value("fields").toObject().size(), 17);
}

void RuntimeContextTest::overlappingRequestIsBusy()
{
    QObject owner;
    auto provider = new RuntimeProvider(&owner);
    provider->addBoolean("rig_connected", "Ready radio connection", []() { return false; });
    RuntimeContext context;
    QVERIFY(context.addProvider("provider", "Test source provider", provider));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1);
    context.collect(2);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(2));
    QCOMPARE(ready.first().at(1).toJsonObject().value("issues").toObject()
             .value("rig_connected").toString(), QString("context_busy"));
    QTRY_COMPARE(ready.count(), 2);
    QCOMPARE(ready.at(1).at(0).toULongLong(), quint64(1));
}

void RuntimeContextTest::sourceMetadataAndSelectedSchema()
{
    QObject owner;
    int reads = 0;
    auto first = new RuntimeProvider(&owner);
    first->addNumber("frequency", "Main VFO frequency in hertz", "Hz", [&]() { return ++reads; });
    auto second = new RuntimeProvider(&owner);
    second->addString("position", "Antenna position", [&]() { ++reads; return QString("north"); });
    RuntimeContext context;
    QVERIFY(context.addProvider("radio", "Connected transceiver and its main VFO", first));
    QVERIFY(context.addProvider("antenna", "Antenna position", second));
    const QJsonObject sources = context.sources().value("sources").toObject();
    QCOMPARE(sources.size(), 2);
    QCOMPARE(sources.value("radio").toObject().value("description").toString(),
             QString("Connected transceiver and its main VFO"));
    const QJsonObject fields = context.schema({"radio"}).value("fields").toObject();
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.value("frequency").toObject().value("source").toString(), QString("radio"));
    QCOMPARE(fields.value("frequency").toObject().value("unit").toString(), QString("Hz"));
    QCOMPARE(context.schema().value("fields").toObject().size(), 2);
    QCOMPARE(reads, 0);
}

void RuntimeContextTest::invalidSourceRegistration_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("description");
    QTest::newRow("empty-name") << QString() << QString("State description");
    QTest::newRow("blank-name") << QString(" ") << QString("State description");
    QTest::newRow("padded-name") << QString(" other") << QString("State description");
    QTest::newRow("empty-description") << QString("other") << QString();
    QTest::newRow("duplicate-name") << QString("existing") << QString("Other state");
}

void RuntimeContextTest::invalidSourceRegistration()
{
    QFETCH(QString, name);
    QFETCH(QString, description);
    QObject owner;
    auto first = new RuntimeProvider(&owner);
    first->addBoolean("first_value", "First state", []() { return true; });
    auto second = new RuntimeProvider(&owner);
    second->addBoolean("second_value", "Second state", []() { return false; });
    RuntimeContext context;
    QVERIFY(context.addProvider("existing", "Existing state", first));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*Invalid or duplicate runtime source.*"));
    QVERIFY(!context.addProvider(name, description, second));
    QCOMPARE(context.sources().value("sources").toObject().size(), 1);
    QVERIFY(!context.schema().value("fields").toObject().contains("second_value"));
}

void RuntimeContextTest::selectionLimitsBusyAndTimeoutIssues()
{
    QObject owner;
    auto first = new RuntimeProvider(&owner);
    first->addNumber("ready", "Ready state", QString(), []() { return 42; });
    auto stalledOwner = new QObject;
    auto stalled = new RuntimeProvider(stalledOwner);
    stalled->addBoolean("slow", "Stalled state", []() { return false; });
    RuntimeWorker worker(stalledOwner);
    RuntimeContext context;
    QVERIFY(context.addProvider("first", "Ready source", first));
    QVERIFY(context.addProvider("stalled", "Stalled source", stalled));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    context.collect(1, 10, {"stalled"});
    context.collect(2, 1000, {"first"});
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(1).toJsonObject().value("issues").toObject(),
             QJsonObject({{"ready", "context_busy"}}));
    QTRY_COMPARE(ready.count(), 2);
    QCOMPARE(ready.at(1).at(1).toJsonObject().value("issues").toObject(),
             QJsonObject({{"slow", "provider_timeout"}}));
    context.collect(3, 1000, {"first"});
    QTRY_COMPARE(ready.count(), 3);
    const QJsonObject result = ready.at(2).at(1).toJsonObject();
    QCOMPARE(result.value("values").toObject(), QJsonObject({{"ready", 42}}));
    QVERIFY(result.value("issues").toObject().isEmpty());
}

void RuntimeContextTest::selectionCanBeRequestedFromAnotherThread()
{
    auto owner = new QObject;
    auto provider = new RuntimeProvider(owner);
    provider->addBoolean("worker_value", "Worker state", [owner]()
    {
        return QThread::currentThread() == owner->thread();
    });
    RuntimeWorker worker(owner);
    RuntimeContext context;
    QVERIFY(context.addProvider("worker", "Worker state", provider));
    QObject otherOwner;
    auto other = new RuntimeProvider(&otherOwner);
    other->addBoolean("other_value", "Unselected state", []() { return false; });
    QVERIFY(context.addProvider("other", "Other state", other));
    QSignalSpy ready(&context, &RuntimeContext::contextReady);
    QTimer::singleShot(0, owner, [&context]() { context.collect(9, 1000, {"worker"}); });
    worker.start();
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(9));
    QCOMPARE(ready.first().at(1).toJsonObject().value("values").toObject(),
             QJsonObject({{"worker_value", true}}));
}

QTEST_GUILESS_MAIN(RuntimeContextTest)

#include "tst_runtimecontext.moc"
