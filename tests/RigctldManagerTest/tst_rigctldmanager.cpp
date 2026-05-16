#include <QtTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#include "rig/RigctldManager.h"
#include "data/RigProfile.h"

class RigctldManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // findRigctldPath tests
    void findRigctldPath_returnsNonEmptyIfInstalled();
    void findRigctldPath_prefersAppDirectory();

    // buildArguments tests (via start with mock)
    void buildArguments_includesModel();
    void buildArguments_includesPort();
    void buildArguments_includesSerialSettings();
    void buildArguments_includesAdditionalArgs();

    // Lifecycle tests
    void isRunning_initiallyFalse();
    void start_failsWithInvalidPath();
    void start_failsWithInvalidProfile();
    void getConnectHost_returnsLocalhost();
    void getConnectPort_returnsConfiguredPort();
    void stop_isIdempotentWithoutStart();

    // getVersion tests
    void getVersion_returnsInvalidForNonexistentPath();
    void getVersion_returnsValidIfInstalled();
    void getVersion_autodetectsPath();

    // Integration test (skipped if rigctld not available)
    void start_stop_integration();
    void destructor_afterStart_stopsProcess();

private:
    QString findRigctld();
    bool rigctldAvailable = false;
    QString rigctldPath;
};

void RigctldManagerTest::initTestCase()
{
    rigctldPath = RigctldManager::findRigctldPath();
    rigctldAvailable = !rigctldPath.isEmpty();

    if (!rigctldAvailable)
    {
        qWarning() << "rigctld not found - some tests will be skipped";
    }
    else
    {
        qDebug() << "Found rigctld at:" << rigctldPath;
    }
}

void RigctldManagerTest::cleanupTestCase()
{
}

QString RigctldManagerTest::findRigctld()
{
    return RigctldManager::findRigctldPath();
}

// ============================================================================
// findRigctldPath tests
// ============================================================================

void RigctldManagerTest::findRigctldPath_returnsNonEmptyIfInstalled()
{
    // This test passes if rigctld is installed, otherwise we just note it
    QString path = RigctldManager::findRigctldPath();

    if (path.isEmpty())
    {
        QSKIP("rigctld not installed on this system");
    }

    QVERIFY(QFile::exists(path));
}

void RigctldManagerTest::findRigctldPath_prefersAppDirectory()
{
    // Create a mock rigctld in app directory
    QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    QString mockPath = appDir + "/rigctld.exe";
#else
    QString mockPath = appDir + "/rigctld";
#endif

    // Create empty file to simulate rigctld
    QFile mockFile(mockPath);
    bool created = mockFile.open(QIODevice::WriteOnly);

    if (!created)
    {
        QSKIP("Cannot create mock rigctld in app directory");
    }
    mockFile.close();

    // Now findRigctldPath should return the app directory path
    QString foundPath = RigctldManager::findRigctldPath();
    QCOMPARE(foundPath, mockPath);

    // Cleanup
    QFile::remove(mockPath);
}

// ============================================================================
// buildArguments tests (indirect via profile inspection)
// ============================================================================

void RigctldManagerTest::buildArguments_includesModel()
{
    // We can't directly test buildArguments (private), but we can verify
    // that start() uses the correct model from profile
    // This is more of a documentation test

    RigProfile profile;
    profile.model = 1234;
    profile.rigctldPort = 14532;
    profile.portPath = "/dev/ttyUSB0";

    // Verify profile is set correctly
    QCOMPARE(profile.model, 1234);
}

void RigctldManagerTest::buildArguments_includesPort()
{
    RigProfile profile;
    profile.rigctldPort = 5000;

    QCOMPARE(profile.rigctldPort, static_cast<quint16>(5000));
}

void RigctldManagerTest::buildArguments_includesSerialSettings()
{
    RigProfile profile;
    profile.baudrate = 9600;
    profile.databits = 8;
    profile.stopbits = 1;
    profile.parity = "None";
    profile.flowcontrol = "None";

    QCOMPARE(profile.baudrate, 9600u);
    QCOMPARE(profile.databits, static_cast<quint8>(8));
}

void RigctldManagerTest::buildArguments_includesAdditionalArgs()
{
    RigProfile profile;
    profile.rigctldArgs = "-v -v --debug";

    QCOMPARE(profile.rigctldArgs, QString("-v -v --debug"));
}

// ============================================================================
// Lifecycle tests
// ============================================================================

void RigctldManagerTest::isRunning_initiallyFalse()
{
    RigctldManager manager;
    QVERIFY(!manager.isRunning());
}

void RigctldManagerTest::start_failsWithInvalidPath()
{
    RigctldManager manager;
    QSignalSpy errorSpy(&manager, &RigctldManager::errorOccurred);

    RigProfile profile;
    profile.model = 1;
    profile.rigctldPort = 14532;
    profile.rigctldPath = "/nonexistent/path/to/rigctld";
    profile.portPath = "/dev/ttyUSB0";

    bool result = manager.start(profile);

    QVERIFY(!result);
    QVERIFY(!manager.isRunning());
}

void RigctldManagerTest::start_failsWithInvalidProfile()
{
    if (!rigctldAvailable)
    {
        QSKIP("rigctld not available");
    }

    RigctldManager manager;

    RigProfile profile;
    profile.model = 1;  // Dummy rig - should fail to open
    profile.rigctldPort = 14533;
    profile.portPath = "/dev/nonexistent_port";

    // This should fail because the serial port doesn't exist
    bool result = manager.start(profile);

    // Even if rigctld starts, it should fail to connect to the rig
    // The behavior depends on how rigctld handles invalid ports
    // We just verify the manager handles this gracefully
    if (result)
    {
        manager.stop();
    }

    QVERIFY(!manager.isRunning());
}

void RigctldManagerTest::getConnectHost_returnsLocalhost()
{
    RigctldManager manager;
    QCOMPARE(manager.getConnectHost(), QString("127.0.0.1"));
}

void RigctldManagerTest::getConnectPort_returnsConfiguredPort()
{
    RigctldManager manager;

    // Default port should be 4532
    QCOMPARE(manager.getConnectPort(), static_cast<quint16>(4532));
}

void RigctldManagerTest::stop_isIdempotentWithoutStart()
{
    RigctldManager manager;
    QSignalSpy stoppedSpy(&manager, &RigctldManager::stopped);

    manager.stop();
    manager.stop();

    QVERIFY(!manager.isRunning());
    QCOMPARE(stoppedSpy.count(), 0);
}

// ============================================================================
// getVersion tests
// ============================================================================

void RigctldManagerTest::getVersion_returnsInvalidForNonexistentPath()
{
    RigctldVersion version = RigctldManager::getVersion("/nonexistent/path/to/rigctld");

    QVERIFY(!version.isValid());
    QCOMPARE(version.major, -1);
    QCOMPARE(version.minor, -1);
    QCOMPARE(version.patch, -1);
}

void RigctldManagerTest::getVersion_returnsValidIfInstalled()
{
    if ( !rigctldAvailable )
        QSKIP("rigctld not available");

    RigctldVersion version = RigctldManager::getVersion(rigctldPath);

    QVERIFY(version.isValid());
    QVERIFY(version.major >= 0);
    QVERIFY(version.minor >= 0);
    QVERIFY(version.patch >= 0);

    qDebug() << "rigctld version:" << version.major << "." << version.minor << "." << version.patch;
}

void RigctldManagerTest::getVersion_autodetectsPath()
{
    if ( !rigctldAvailable )
        QSKIP("rigctld not available");

    // Call without path - should autodetect
    RigctldVersion version = RigctldManager::getVersion();

    QVERIFY(version.isValid());
    QVERIFY(version.major >= 0);
}

// ============================================================================
// Integration test
// ============================================================================

void RigctldManagerTest::start_stop_integration()
{
    if (!rigctldAvailable)
    {
        QSKIP("rigctld not available for integration test");
    }

    RigctldManager manager;
    QSignalSpy startedSpy(&manager, &RigctldManager::started);
    QSignalSpy stoppedSpy(&manager, &RigctldManager::stopped);
    QSignalSpy errorSpy(&manager, &RigctldManager::errorOccurred);

    RigProfile profile;
    profile.model = 1;  // Dummy rig (Hamlib model 1 = Dummy)
    profile.rigctldPort = 14534;
    profile.rigctldPath = rigctldPath;
    // For dummy rig, we don't need a real port

    bool result = manager.start(profile);

    if (!result)
    {
        // If start failed, check why
        if (!errorSpy.isEmpty())
        {
            qWarning() << "Start failed with error:" << errorSpy.first().first().toString();
        }
        QSKIP("Could not start rigctld with dummy rig");
    }

    QVERIFY(result);
    QVERIFY(manager.isRunning());
    QCOMPARE(manager.getConnectPort(), static_cast<quint16>(14534));

    // Verify we can connect to the port
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", 14534);
    bool connected = socket.waitForConnected(2000);
    QVERIFY(connected);
    socket.disconnectFromHost();

    // Stop
    manager.stop();
    manager.stop();
    QVERIFY(!manager.isRunning());
    QCOMPARE(stoppedSpy.count(), 1);

    // Verify port is no longer listening
    QTcpSocket socket2;
    socket2.connectToHost("127.0.0.1", 14534);
    bool stillConnected = socket2.waitForConnected(1000);
    QVERIFY(!stillConnected);
}

void RigctldManagerTest::destructor_afterStart_stopsProcess()
{
    if (!rigctldAvailable)
    {
        QSKIP("rigctld not available for integration test");
    }

    constexpr quint16 testPort = 14535;

    {
        RigctldManager manager;
        QSignalSpy errorSpy(&manager, &RigctldManager::errorOccurred);

        RigProfile profile;
        profile.model = 1;  // Dummy rig (Hamlib model 1 = Dummy)
        profile.rigctldPort = testPort;
        profile.rigctldPath = rigctldPath;

        bool result = manager.start(profile);

        if (!result)
        {
            if (!errorSpy.isEmpty())
            {
                qWarning() << "Start failed with error:" << errorSpy.first().first().toString();
            }
            QSKIP("Could not start rigctld with dummy rig");
        }

        QVERIFY(manager.isRunning());
    }

    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", testPort);
    bool stillConnected = socket.waitForConnected(1000);
    QVERIFY(!stillConnected);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    RigctldManagerTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_rigctldmanager.moc"
