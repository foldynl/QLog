#include <QJsonParseError>
#include <QJsonArray>
#include <QtNumeric>

#include "core/debug.h"
#include "PotaQE.h"

MODULE_IDENTIFICATION("qlog.core.potaqe");

PotaQE::PotaQE(QObject *parent)
    : QObject{parent}
{
    FCT_IDENTIFICATION;

    refreshTimer.setInterval(REFRESHPERIOD);

    spotUpdater.moveToThread(&spotUpdaterThread);

    connect(&spotUpdaterThread, &QThread::started, &spotUpdater, &PotaAppActivatorDownloader::updateActivators);
    connect(&spotUpdaterThread, &QThread::started, &refreshTimer, QOverload<>::of(&QTimer::start));
    connect(&spotUpdaterThread, &QThread::finished, &spotUpdater, &QObject::deleteLater);

    connect(&spotUpdater, &PotaAppActivatorDownloader::activatorsUpdated, this, &PotaQE::updateSpot);

    connect(&refreshTimer, &QTimer::timeout, &spotUpdater, &PotaAppActivatorDownloader::updateActivators);

    spotUpdaterThread.start();
}

PotaQE::~PotaQE()
{
    FCT_IDENTIFICATION;

    if ( spotUpdaterThread.isRunning() )
    {
        spotUpdaterThread.quit();
        spotUpdaterThread.wait();
    }
}

// find park reference by callsign + freq +-5 kHz + mode.
// Matching:
//    callsign ignores prefixes and suffixes (/P,/M,/QRP,...) (Based Callsign is used)
//    freq +- 5KHz
const POTASpot PotaQE::findReferenceId(const Callsign &callsign, double freq)
{
    FCT_IDENTIFICATION;

    const QString &baseCallsign = callsign.getBase();

    if ( !callsign.isValid() || baseCallsign.isEmpty() ) return {};

    QMutexLocker locker(&activatorLock);

    auto i = activatorSpots.constFind(baseCallsign);

    while ( i != activatorSpots.cend() && i.key() == baseCallsign )
    {
        const POTASpot &spot = i.value();
        if ( qAbs(spot.frequency - freq) <= FREQTOL ) return spot;
        i++;
    }

    return {};
}

void PotaQE::updateSpot()
{
    FCT_IDENTIFICATION;

    QMutexLocker locker(&activatorLock);
    spotUpdater.swapActivators(activatorSpots);
}
