#ifndef QLOG_CORE_POTAQE_H
#define QLOG_CORE_POTAQE_H

#include <QObject>
#include <QTimer>
#include <QThread>

#include "data/Callsign.h"
#include "service/potaapp/PotaApp.h"

class PotaQE : public QObject
{
    Q_OBJECT

public:
    static PotaQE *instance()
    {
        static PotaQE instance;
        return &instance;
    };

    const POTASpot findReferenceId(const Callsign &callsign, double freq);

private slots:
    void updateSpot();

private:
    QThread spotUpdaterThread;
    PotaAppActivatorDownloader spotUpdater;
    QTimer refreshTimer;
    QMutex activatorLock;

    const double FREQTOL = 0.005; // in MHz
    const int REFRESHPERIOD = 60 * 1000;
    PotaAppActivatorDownloader::ActivatorStorage activatorSpots;

    explicit PotaQE(QObject *parent = nullptr);
    ~PotaQE();
};

#endif // QLOG_CORE_POTAQE_H
