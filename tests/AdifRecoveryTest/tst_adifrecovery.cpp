#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

#include "core/AdifRecovery.h"

class AdifRecoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void configSerializationRoundTrip();
    void configDeserializationDefaults();
    void stateSerializationRoundTrip();
    void invalidStateDeserializationReturnsDefault();
    void firstReadInitializesAtEnd();
    void offsetAtEndDoesNothing();
    void appendedCompleteRecordsAreReturned();
    void uppercaseEorIsAccepted();
    void incompleteTrailingRecordIsLeftUnread();
    void offsetAfterTagStartStillReadsRecord();
    void garbageAfterOffsetDoesNotAdvance();
    void missingFileReportsProblem();
    void emptyPathReportsProblem();
    void truncatedFileResetsLoadPoint();
    void tooManyRecordsMovesLoadPointToEnd();

private:
    QString writeFile(const QByteArray &content);
    AdifRecoveryScanResult scan(const QString &path, qint64 offset, int maxContacts = 0);

    QTemporaryDir tempDir;
};

void AdifRecoveryTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    qRegisterMetaType<AdifRecoveryScanResult>("AdifRecoveryScanResult");
    QVERIFY(tempDir.isValid());
}

void AdifRecoveryTest::configSerializationRoundTrip()
{
    AdifRecoveryConfig skipped;
    skipped.enabled = true;
    skipped.stationProfileName = QStringLiteral("ignored");
    skipped.path = QStringLiteral("   ");

    AdifRecoveryConfig custom;
    custom.enabled = false;
    custom.stationProfileName = QStringLiteral("Home");
    custom.qslSentStatusDefault = QStringLiteral("custom");
    custom.path = QStringLiteral("/tmp/home.adi");

    AdifRecoveryConfig defaultStatus;
    defaultStatus.enabled = true;
    defaultStatus.stationProfileName = QStringLiteral("Portable");
    defaultStatus.qslSentStatusDefault = QString();
    defaultStatus.path = QStringLiteral("/tmp/portable.adi");

    const QList<AdifRecoveryConfig> configs = AdifRecovery::deserializeConfigList(
        AdifRecovery::serializeConfigList({skipped, custom, defaultStatus}));

    QCOMPARE(configs.size(), 2);
    QCOMPARE(configs.at(0).enabled, false);
    QCOMPARE(configs.at(0).stationProfileName, QStringLiteral("Home"));
    QCOMPARE(configs.at(0).qslSentStatusDefault, QStringLiteral("custom"));
    QCOMPARE(configs.at(0).path, QStringLiteral("/tmp/home.adi"));
    QCOMPARE(configs.at(1).enabled, true);
    QCOMPARE(configs.at(1).stationProfileName, QStringLiteral("Portable"));
    QCOMPARE(configs.at(1).qslSentStatusDefault, QStringLiteral("Q"));
    QCOMPARE(configs.at(1).path, QStringLiteral("/tmp/portable.adi"));
}

void AdifRecoveryTest::configDeserializationDefaults()
{
    const QString json = QStringLiteral(R"json([
        { "path": "/tmp/defaults.adi", "stationProfileName": "Default" },
        { "path": "  " },
        { "path": "/tmp/custom.adi", "enabled": false, "qslSentStatusDefault": "I" }
    ])json");

    const QList<AdifRecoveryConfig> configs = AdifRecovery::deserializeConfigList(json);

    QCOMPARE(configs.size(), 2);
    QCOMPARE(configs.at(0).enabled, true);
    QCOMPARE(configs.at(0).stationProfileName, QStringLiteral("Default"));
    QCOMPARE(configs.at(0).qslSentStatusDefault, QStringLiteral("Q"));
    QCOMPARE(configs.at(0).path, QStringLiteral("/tmp/defaults.adi"));
    QCOMPARE(configs.at(1).enabled, false);
    QCOMPARE(configs.at(1).qslSentStatusDefault, QStringLiteral("I"));
    QCOMPARE(configs.at(1).path, QStringLiteral("/tmp/custom.adi"));
}

void AdifRecoveryTest::stateSerializationRoundTrip()
{
    AdifRecoveryState state;
    state.path = QStringLiteral("/tmp/recovery.adi");
    state.offset = 123456789;
    state.lastRecoveryAt = QDateTime(QDate(2026, 5, 22), QTime(8, 30, 15), Qt::UTC);
    state.lastMessage = QStringLiteral("done");

    const AdifRecoveryState restored = AdifRecovery::deserializeState(AdifRecovery::serializeState(state));

    QCOMPARE(restored.path, state.path);
    QCOMPARE(restored.offset, state.offset);
    QCOMPARE(restored.lastRecoveryAt, state.lastRecoveryAt);
    QCOMPARE(restored.lastMessage, state.lastMessage);
}

void AdifRecoveryTest::invalidStateDeserializationReturnsDefault()
{
    const AdifRecoveryState state = AdifRecovery::deserializeState(QStringLiteral("not json"));

    QCOMPARE(state.path, QString());
    QCOMPARE(state.offset, static_cast<qint64>(-1));
    QVERIFY(!state.lastRecoveryAt.isValid());
    QCOMPARE(state.lastMessage, QString());
}

QString AdifRecoveryTest::writeFile(const QByteArray &content)
{
    const QString path = tempDir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".adi"));
    QFile file(path);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        qFatal("Cannot open temporary ADIF file for writing");
    if ( file.write(content) != static_cast<qint64>(content.size()) )
        qFatal("Cannot write complete temporary ADIF file");
    file.close();
    return path;
}

AdifRecoveryScanResult AdifRecoveryTest::scan(const QString &path, qint64 offset, int maxContacts)
{
    AdifRecoveryConfig config;
    config.path = path;
    config.stationProfileName = QStringLiteral("default");

    AdifRecoveryState state;
    state.path = path;
    state.offset = offset;

    AdifRecoveryReaderWorker worker;
    QSignalSpy spy(&worker, &AdifRecoveryReaderWorker::scanFinished);
    if ( !spy.isValid() )
        qFatal("Cannot create scanFinished signal spy");

    worker.readTail(config, state, maxContacts);

    if ( spy.count() != 1 )
        qFatal("AdifRecoveryReaderWorker did not emit exactly one scanFinished signal");
    return spy.takeFirst().at(0).value<AdifRecoveryScanResult>();
}

void AdifRecoveryTest::firstReadInitializesAtEnd()
{
    const QByteArray content("<call:5>AA1AA<eor>");
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, -1);

    QCOMPARE(result.previousOffset, static_cast<qint64>(-1));
    QCOMPARE(result.fileSize, static_cast<qint64>(content.size()));
    QCOMPARE(result.nextOffset, static_cast<qint64>(content.size()));
    QVERIFY(result.adifText.isEmpty());
    QVERIFY(!result.message.isEmpty());
    QVERIFY(!result.reset);
    QVERIFY(!result.tooMany);
}

void AdifRecoveryTest::offsetAtEndDoesNothing()
{
    const QByteArray content("<call:5>AA1AA<eor>");
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, content.size());

    QCOMPARE(result.previousOffset, static_cast<qint64>(content.size()));
    QCOMPARE(result.nextOffset, static_cast<qint64>(content.size()));
    QCOMPARE(result.fileSize, static_cast<qint64>(content.size()));
    QVERIFY(result.adifText.isEmpty());
    QCOMPARE(result.contactCount, 0);
    QVERIFY(result.message.isEmpty());
    QVERIFY(!result.reset);
    QVERIFY(!result.tooMany);
}

void AdifRecoveryTest::appendedCompleteRecordsAreReturned()
{
    const QByteArray initial("<call:5>AA1AA<eor>");
    const QByteArray records("<call:5>BB2BB<eor><call:5>CC3CC<eor>");
    const QByteArray content = initial + "\n" + records + "tail";
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, initial.size());

    QCOMPARE(result.contactCount, 2);
    QCOMPARE(result.adifText, QString::fromLatin1(records));
    QCOMPARE(result.nextOffset, static_cast<qint64>(initial.size() + 1 + records.size()));
    QVERIFY(result.message.isEmpty());
    QVERIFY(!result.reset);
    QVERIFY(!result.tooMany);
}

void AdifRecoveryTest::uppercaseEorIsAccepted()
{
    const QByteArray content("<call:5>AA1AA<EOR><call:5>BB2BB<eOr>");
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, 0);

    QCOMPARE(result.contactCount, 2);
    QCOMPARE(result.adifText, QString::fromLatin1(content));
    QCOMPARE(result.nextOffset, static_cast<qint64>(content.size()));
}

void AdifRecoveryTest::incompleteTrailingRecordIsLeftUnread()
{
    const QByteArray initial("<call:5>AA1AA<eor>");
    const QByteArray complete("<call:5>BB2BB<eor>");
    const QByteArray incomplete("<call:5>CC3CC");
    const QByteArray content = initial + complete + incomplete;
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, initial.size());

    QCOMPARE(result.contactCount, 1);
    QCOMPARE(result.adifText, QString::fromLatin1(complete));
    QCOMPARE(result.nextOffset, static_cast<qint64>(initial.size() + complete.size()));
}

void AdifRecoveryTest::offsetAfterTagStartStillReadsRecord()
{
    const QByteArray content("<call:5>AA1AA<eor>");
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, 1);

    QCOMPARE(result.contactCount, 1);
    QCOMPARE(result.adifText, QString::fromLatin1(content));
    QCOMPARE(result.nextOffset, static_cast<qint64>(content.size()));
}

void AdifRecoveryTest::garbageAfterOffsetDoesNotAdvance()
{
    const QByteArray initial("<call:5>AA1AA<eor>");
    const QByteArray garbage("this is not ADIF<call:5>BB2BB<eor>");
    const QByteArray content = initial + garbage;
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, initial.size());

    QCOMPARE(result.previousOffset, static_cast<qint64>(initial.size()));
    QCOMPARE(result.nextOffset, static_cast<qint64>(initial.size()));
    QVERIFY(result.adifText.isEmpty());
    QCOMPARE(result.contactCount, 0);
    QVERIFY(result.message.isEmpty());
}

void AdifRecoveryTest::missingFileReportsProblem()
{
    const QString path = tempDir.filePath(QStringLiteral("missing.adi"));

    const AdifRecoveryScanResult result = scan(path, 0);

    QCOMPARE(result.previousOffset, static_cast<qint64>(0));
    QCOMPARE(result.nextOffset, static_cast<qint64>(0));
    QCOMPARE(result.fileSize, static_cast<qint64>(-1));
    QVERIFY(result.adifText.isEmpty());
    QVERIFY(!result.message.isEmpty());
    QVERIFY(!result.reset);
    QVERIFY(!result.tooMany);
}

void AdifRecoveryTest::emptyPathReportsProblem()
{
    const AdifRecoveryScanResult result = scan(QString(), 0);

    QCOMPARE(result.previousOffset, static_cast<qint64>(0));
    QCOMPARE(result.nextOffset, static_cast<qint64>(0));
    QCOMPARE(result.fileSize, static_cast<qint64>(-1));
    QVERIFY(result.adifText.isEmpty());
    QVERIFY(!result.message.isEmpty());
    QVERIFY(!result.reset);
    QVERIFY(!result.tooMany);
}

void AdifRecoveryTest::truncatedFileResetsLoadPoint()
{
    const QByteArray content("<call:5>AA1AA<eor>");
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, content.size() + 10);

    QVERIFY(result.reset);
    QVERIFY(result.adifText.isEmpty());
    QCOMPARE(result.nextOffset, static_cast<qint64>(content.size()));
    QVERIFY(!result.message.isEmpty());
}

void AdifRecoveryTest::tooManyRecordsMovesLoadPointToEnd()
{
    const QByteArray first("<call:5>AA1AA<eor>");
    const QByteArray second("<call:5>BB2BB<eor>");
    const QByteArray content = first + second;
    const QString path = writeFile(content);

    const AdifRecoveryScanResult result = scan(path, 0, 1);

    QVERIFY(result.tooMany);
    QCOMPARE(result.contactCount, 2);
    QVERIFY(result.adifText.isEmpty());
    QCOMPARE(result.nextOffset, static_cast<qint64>(content.size()));
    QVERIFY(!result.message.isEmpty());
}

QTEST_APPLESS_MAIN(AdifRecoveryTest)

#include "tst_adifrecovery.moc"
