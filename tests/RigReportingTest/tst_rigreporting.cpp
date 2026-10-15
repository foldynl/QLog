#include <QtTest>
#include <QtNetwork>
#include <QtWebSockets>
#include "rig/drivers/GenericRigDrv.h"

#include "rig/drivers/FlrigRigDrv.h"
#include "rig/drivers/TCIRigDrv.h"
#include "rig/drivers/HamlibRigDrv.h"

class RigReportingTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void flrigReportsInitialSplitOff();
    void tciReportsInitialSplitOff();
    void tciRequestsFreshTxReadingAfterSplitOff();
    void tciOffsetReports_data();
    void tciOffsetReports();
    void tciRejectsInvalidKeySpeed_data();
    void tciRejectsInvalidKeySpeed();
    void hamlibDoesNotReportFailedOffsetsAsZero();
    void hamlibReportsFirstZeroPower();
};

void RigReportingTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    rig_set_debug(RIG_DEBUG_NONE);
}

void RigReportingTest::flrigReportsInitialSplitOff()
{
    RigProfile profile;
    profile.getSplitInfo = true;
    profile.pollInterval = 60000;
    FlrigRigDrv driver(profile);
    driver.stopped = true; // Parse reports without starting network requests.
    QSignalSpy split(&driver, &GenericRigDrv::splitChanged);
    driver.rspGET_SPLIT(false);
    QCOMPARE(split.count(), 1);
    QCOMPARE(split.at(0).at(0).toBool(), false);
    driver.rspGET_SPLIT(false);
    QCOMPARE(split.count(), 1);
    driver.sendState();
    driver.rspGET_SPLIT(false);
    QCOMPARE(split.count(), 2);
}

void RigReportingTest::tciReportsInitialSplitOff()
{
    RigProfile profile;
    profile.getSplitInfo = true;
    TCIRigDrv driver(profile);
    QSignalSpy split(&driver, &GenericRigDrv::splitChanged);
    driver.onTextMessageReceived("split_enable:0,false;");
    QCOMPARE(split.count(), 1);
    QCOMPARE(split.at(0).at(0).toBool(), false);
    driver.onTextMessageReceived("split_enable:0,false;");
    QCOMPARE(split.count(), 1);
}

void RigReportingTest::tciRequestsFreshTxReadingAfterSplitOff()
{
    RigProfile profile;
    profile.getSplitInfo = true;
    profile.getFreqInfo = true;
    TCIRigDrv driver(profile);
    QSignalSpy tx(&driver, &GenericRigDrv::txFrequencyChanged);
    driver.onTextMessageReceived("split_enable:0,false;");
    driver.onTextMessageReceived("vfo:0,1,14076000;");
    QCOMPARE(tx.count(), 0);
    driver.onTextMessageReceived("split_enable:0,true;");
    driver.onTextMessageReceived("vfo:0,1,14076000;");
    QCOMPARE(tx.count(), 1);
    QCOMPARE(tx.at(0).at(0).toDouble(), 14.076);
    driver.onTextMessageReceived("split_enable:0,false;");
    driver.onTextMessageReceived("split_enable:0,true;");
    driver.onTextMessageReceived("vfo:0,1,14076000;");
    QCOMPARE(tx.count(), 2);
}

void RigReportingTest::tciOffsetReports_data()
{
    QTest::addColumn<QString>("axis");
    QTest::addColumn<bool>("enableFirst");
    QTest::addColumn<bool>("enabled");
    for ( const QString &axis : {QStringLiteral("rit"), QStringLiteral("xit")} )
        for ( bool enableFirst : {false, true} )
            for ( bool enabled : {false, true} )
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(axis).arg(enableFirst).arg(enabled)))
                    << axis << enableFirst << enabled;
}

void RigReportingTest::tciOffsetReports()
{
    QFETCH(QString, axis);
    QFETCH(bool, enableFirst);
    QFETCH(bool, enabled);
    RigProfile profile;
    profile.getRITInfo = true;
    profile.getXITInfo = true;
    TCIRigDrv driver(profile);
    QSignalSpy reports(&driver, axis == "rit" ? &GenericRigDrv::ritChanged : &GenericRigDrv::xitChanged);
    const QString enable = axis + "_enable:0," + (enabled ? "true;" : "false;");
    const QString offset = axis + "_offset:0,-1250;";
    driver.onTextMessageReceived(enableFirst ? enable : offset);
    QCOMPARE(reports.count(), enableFirst && !enabled ? 1 : 0);
    driver.onTextMessageReceived(enableFirst ? offset : enable);
    QVERIFY(!reports.isEmpty());
    QCOMPARE(reports.last().at(0).toDouble(), enabled ? -0.00125 : 0.0);
    driver.onTextMessageReceived(axis + "_offset:0,invalid;");
    driver.onTextMessageReceived(enable);
    QCOMPARE(reports.last().at(0).toDouble(), enabled ? -0.00125 : 0.0);
}

void RigReportingTest::tciRejectsInvalidKeySpeed_data()
{
    QTest::addColumn<QString>("value");
    QTest::newRow("negative") << QString("-1");
    QTest::newRow("invalid") << QString("invalid");
}

void RigReportingTest::tciRejectsInvalidKeySpeed()
{
    QFETCH(QString, value);
    RigProfile profile;
    TCIRigDrv driver(profile);
    QSignalSpy speed(&driver, &GenericRigDrv::keySpeedChanged);
    driver.onTextMessageReceived("cw_macros_speed:" + value + ";");
    QCOMPARE(speed.count(), 0);
    driver.onTextMessageReceived("cw_macros_speed:25;");
    QCOMPARE(speed.count(), 1);
    QCOMPARE(speed.at(0).at(0).toUInt(), 25U);
}

void RigReportingTest::hamlibDoesNotReportFailedOffsetsAsZero()
{
    RigProfile profile;
    profile.model = RIG_MODEL_DUMMY;
    profile.getRITInfo = true;
    profile.getXITInfo = true;
    HamlibRigDrv driver(profile, profile.model);
    QVERIFY(driver.rig);
    QCOMPARE(rig_open(driver.rig), RIG_OK);
    auto originalCaps = driver.rig->caps;
    struct rig_caps caps = *originalCaps;
    caps.has_get_func |= RIG_FUNC_RIT | RIG_FUNC_XIT;
    caps.get_func = [](RIG *, vfo_t, setting_t, int *enabled) -> int { *enabled = 1; return RIG_OK; };
    caps.get_rit = [](RIG *, vfo_t, shortfreq_t *) { return -RIG_EIO; };
    caps.get_xit = [](RIG *, vfo_t, shortfreq_t *) { return -RIG_EIO; };
    driver.rig->caps = &caps;
    driver.forceSendState = true;
    QSignalSpy rit(&driver, &GenericRigDrv::ritChanged);
    QSignalSpy xit(&driver, &GenericRigDrv::xitChanged);
    driver.checkRITChange();
    driver.checkXITChange();
    const int failedRitReports = rit.count();
    const int failedXitReports = xit.count();
    // A valid first zero following a failed initial read must be emitted.
    caps.get_rit = [](RIG *, vfo_t, shortfreq_t *value) { *value = 0; return 0; };
    caps.get_xit = [](RIG *, vfo_t, shortfreq_t *value) { *value = 0; return 0; };
    driver.forceSendState = false;
    driver.checkRITChange();
    driver.checkXITChange();
    driver.rig->caps = originalCaps;
    QCOMPARE(failedRitReports, 0);
    QCOMPARE(failedXitReports, 0);
    QCOMPARE(rit.count(), 1);
    QCOMPARE(xit.count(), 1);
    QCOMPARE(rit.last().at(0).toDouble(), 0.0);
    QCOMPARE(xit.last().at(0).toDouble(), 0.0);
}

void RigReportingTest::hamlibReportsFirstZeroPower()
{
    RigProfile profile;
    profile.model = RIG_MODEL_DUMMY;
    profile.getPWRInfo = true;
    HamlibRigDrv driver(profile, profile.model);
    QVERIFY(driver.rig);
    QCOMPARE(rig_open(driver.rig), RIG_OK);
    auto originalCaps = driver.rig->caps;
    struct rig_caps caps = *originalCaps;
    caps.has_get_level |= RIG_LEVEL_RFPOWER;
    caps.get_level = [](RIG *, vfo_t, setting_t, value_t *value) { value->f = 0; return 0; };
    caps.power2mW = [](RIG *, unsigned int *value, float, freq_t, rmode_t) { *value = 0; return 0; };
    driver.rig->caps = &caps;
    QSignalSpy power(&driver, &GenericRigDrv::powerChanged);
    driver.checkPWRChange();
    driver.checkPWRChange();
    driver.rig->caps = originalCaps;
    QCOMPARE(power.count(), 1);
    QCOMPARE(power.at(0).at(0).toDouble(), 0.0);
}

QTEST_GUILESS_MAIN(RigReportingTest)
#include "tst_rigreporting.moc"
