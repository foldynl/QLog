#include <QtTest>

#include "models/WsjtxActivity.h"

class WsjtxActivityTest : public QObject
{
    Q_OBJECT

private slots:
    void samePeriodUpdatesExistingSample();
    void missingPeriodsRemainVisible();
    void decodeBeforeMidnightKeepsPreviousDate();
    void nonIntegralSecondPeriodKeepsDecodeTime();
    void signalLevelUsesClearOperatorRanges();
    void decodeClockOffsetDoesNotHideActivity();
    void arrivalWavesShareProtocolPeriod();
};

void WsjtxActivityTest::samePeriodUpdatesExistingSample()
{
    WsjtxActivity activity;
    const QDateTime received(QDate(2026, 9, 30), QTime(12, 0, 13), Qt::UTC);

    activity.update(received, QTime(12, 0, 0), -18, 15000);
    activity.update(received.addMSecs(500), QTime(12, 0, 0), -7, 15000);

    const QVariantList history = activity.history(activity.samples().constLast().decodedAt,
                                                  15000);
    QCOMPARE(history.size(), 8);
    QCOMPARE(history.at(7).toInt(), -7);
    QCOMPARE(activity.samples().size(), 1);
}

void WsjtxActivityTest::missingPeriodsRemainVisible()
{
    WsjtxActivity activity;
    const QDateTime now(QDate(2026, 9, 30), QTime(12, 2, 14), Qt::UTC);

    activity.update(now.addSecs(-105), QTime(12, 0, 15), -20, 15000);
    activity.update(now.addSecs(-16), QTime(12, 1, 45), -12, 15000);
    activity.update(now, QTime(12, 2, 0), -4, 15000);

    const QVariantList history = activity.history(activity.samples().constLast().decodedAt,
                                                  15000);
    QCOMPARE(history.at(0).toInt(), -20);
    QVERIFY(!history.at(1).isValid());
    QVERIFY(!history.at(5).isValid());
    QCOMPARE(history.at(6).toInt(), -12);
    QCOMPARE(history.at(7).toInt(), -4);
}

void WsjtxActivityTest::decodeBeforeMidnightKeepsPreviousDate()
{
    WsjtxActivity activity;
    const QDateTime received(QDate(2026, 10, 1), QTime(0, 0, 1), Qt::UTC);

    activity.update(received, QTime(23, 59, 45), -9, 15000);

    QCOMPARE(activity.samples().constLast().decodedAt,
             QDateTime(QDate(2026, 9, 30), QTime(23, 59, 45), Qt::UTC));
}

void WsjtxActivityTest::nonIntegralSecondPeriodKeepsDecodeTime()
{
    WsjtxActivity activity;
    const QDateTime received(QDate(2026, 9, 30), QTime(12, 0, 3, 700), Qt::UTC);

    activity.update(received, QTime(12, 0, 0), -11, 3800);

    QCOMPARE(activity.samples().constLast().decodedAt,
             QDateTime(QDate(2026, 9, 30), QTime(12, 0, 0), Qt::UTC));
}

void WsjtxActivityTest::signalLevelUsesClearOperatorRanges()
{
    QCOMPARE(WsjtxActivity::signalLevel(-30), 1);
    QCOMPARE(WsjtxActivity::signalLevel(-15), 1);
    QCOMPARE(WsjtxActivity::signalLevel(-14), 2);
    QCOMPARE(WsjtxActivity::signalLevel(-7), 2);
    QCOMPARE(WsjtxActivity::signalLevel(-6), 3);
    QCOMPARE(WsjtxActivity::signalLevel(20), 3);
}

void WsjtxActivityTest::decodeClockOffsetDoesNotHideActivity()
{
    WsjtxActivity activity;
    const QDateTime received(QDate(2026, 9, 30), QTime(12, 0, 13), Qt::UTC);

    activity.update(received, QTime(14, 0, 0), -9, 15000);

    const QVariantList history = activity.history(activity.samples().constLast().decodedAt,
                                                  15000);
    QCOMPARE(history.at(7).toInt(), -9);
    QCOMPARE(activity.latestActivity(), received);
}

void WsjtxActivityTest::arrivalWavesShareProtocolPeriod()
{
    WsjtxActivity earlyArrival;
    WsjtxActivity lateArrival;
    const QTime decodedPeriod(12, 0, 0);
    const QDateTime earlyReceived(QDate(2026, 9, 30), QTime(12, 0, 14, 900), Qt::UTC);
    const QDateTime lateReceived(QDate(2026, 9, 30), QTime(12, 0, 15, 100), Qt::UTC);

    earlyArrival.update(earlyReceived, decodedPeriod, -12, 15000);
    lateArrival.update(lateReceived, decodedPeriod, -8, 15000);
    const QDateTime sourcePeriod = lateArrival.samples().constLast().decodedAt;

    QCOMPARE(earlyArrival.history(sourcePeriod, 15000).at(7).toInt(), -12);
    QCOMPARE(lateArrival.history(sourcePeriod, 15000).at(7).toInt(), -8);
}

QTEST_APPLESS_MAIN(WsjtxActivityTest)

#include "tst_wsjtxactivity.moc"
