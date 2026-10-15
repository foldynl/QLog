#ifndef QLOG_ROTATOR_ROTATOR_H
#define QLOG_ROTATOR_ROTATOR_H
#include <atomic>
#include <QTimer>

#include "data/RotProfile.h"
#include "rotator/drivers/GenericRotDrv.h"
#include "RotCaps.h"

class RuntimeProvider;

class Rotator : public QObject
{
    Q_OBJECT

public:

    enum DriverID
    {
        UNDEF_DRIVER = 0,
        HAMLIB_DRIVER = 1,
        PSTROTATOR_DRIVER = 2
    };

    struct Status
    {
        QString profile;
        QString backend;
        QString modelName;
        bool isConnected = false;
        double azimuth = 0.0;
        double elevation = 0.0;
        // Drivers may report axes separately; defaults are not position readings.
        bool azimuthKnown = false;
        bool elevationKnown = false;
    };

    static Rotator* instance()
    {
        static Rotator instance;
        return &instance;
    };
    bool getPosition(double &azimuth, double &elevation) const;
    bool isRotConnected() const;
    RuntimeProvider *getRuntimeProvider() const;

    const QList<QPair<int, QString>> getModelList(const DriverID &id) const;
    const QList<QPair<int, QString>> getDriverList() const;
    const RotCaps getRotCaps(const DriverID &, int) const;

signals:
    void positionChanged(double azimuth, double elevation);
    void rotErrorPresent(QString, QString);
    void rotDisconnected();
    void rotConnected();

public slots:
    void start();
    void open();
    void close();
    void shutdown();
    void stopTimer();

    void sendState();
    void setPosition(double azimuth, double elevation);

private slots:
    void setPositionImpl(double azimuth, double elevation);
    void stopTimerImplt();
    void closeImpl();
    void shutdownImpl();
    void sendStateImpl();

private:
    friend class RotatorReportingTest;

    static const int SHUTDOWN_TIMEOUT_MS = 5000;

    Rotator(QObject *parent = nullptr);
    ~Rotator();

    class DrvParams
    {
    public:
        DrvParams(const DriverID id,
                  const QString &driverName,
                  QList<QPair<int, QString>> (*getModelfct)(),
                  RotCaps (*getCapsfct)(int)) :
            driverID(id),
            driverName(driverName),
            getModeslListFunction(getModelfct),
            getCapsFunction(getCapsfct)
            {};

        DrvParams() :
            driverID(UNDEF_DRIVER),
            getModeslListFunction(nullptr),
            getCapsFunction(nullptr)
        {};

        DriverID driverID;
        QString driverName;
        QList<QPair<int, QString>> (*getModeslListFunction)();
        RotCaps (*getCapsFunction)(int);
    };

    QMap<int, DrvParams> drvMapping;

    void openImpl(const RotProfile &profile, quint64 requestSequence);
    void __closeRot();
    void __openRot(const RotProfile &profile);
    void driverPositionChanged(double azimuth, double elevation);
    void driverErrorOccurred(const QString &error, const QString &detail);
    void driverReady();

    GenericRotDrv *getDriver(const RotProfile &profile);

private:
    GenericRotDrv *rotDriver;
    QMutex rotLock;
    mutable QMutex stateLock;
    // Written and collected in the owner thread; stateLock protects existing cross-thread readers.
    Status rotStatus;
    RuntimeProvider *statusProvider;
    std::atomic<quint64> openRequestSequence;
};

#endif // QLOG_ROTATOR_ROTATOR_H
