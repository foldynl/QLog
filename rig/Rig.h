#ifndef RIG_RIG_H
#define RIG_RIG_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include "rig/drivers/GenericRigDrv.h"
#include "RigCaps.h"
#include "data/DxSpot.h"

class RigctldManager;
class RuntimeProvider;

class Rig : public QObject
{
    Q_OBJECT
public:

    static int DEFAULT_MODEL;

    enum DriverID
    {
        UNDEF_DRIVER = 0,
        HAMLIB_DRIVER = 1,
        OMNIRIG_DRIVER = 2,
        OMNIRIGV2_DRIVER = 3,
        TCI_DRIVER = 4,
        FLRIG_DRIVER = 5,
    };

    struct Status
    {
        QString profile;
        QString backend;
        QString modelName;
        double freq = 0.0;
        // Main-VFO RX/TX values already adjusted by the driver, in MHz.
        double rxFreq = 0.0;
        double mainTxFreq = 0.0;
        // Configured profile offsets, independent of CAT RIT/XIT reports, in MHz.
        double rxOffset = 0.0;
        double txOffset = 0.0;
        QString mode;
        QString submode;
        QString rawmode;
        qint8 ptt = -1;
        double power = 0.0;
        quint16 keySpeed = 0;
        QString vfo = "Curr";
        double rit = 0.0;
        double xit = 0.0;
        qint32 bandwidth = 0;
        bool isConnected = false;
        bool splitEnabled = false;
        // Separate TX VFO report during split, in MHz.
        double txFreq = 0.0;

        // Defaults are not device readings. Set these only on enabled, supported
        // driver reports so runtime getters can distinguish unknown from zero/false.
        bool powerKnown = false;
        bool keySpeedKnown = false;
        bool vfoKnown = false;
        bool ritKnown = false;
        bool xitKnown = false;
        bool bandwidthKnown = false;
        bool splitKnown = false;
        bool txFrequencyKnown = false;

        void clear()
        {
            *this = Status();
        };
    };

    static Rig* instance()
    {
        static Rig instance;
        return &instance;
    };

    static qint32 getNormalBandwidth(const QString &mode,
                                     const QString &subMode);

    explicit Rig(QObject *parent = nullptr);
    ~Rig();

    RuntimeProvider *getRuntimeProvider() const;

    bool isRigConnected();
    bool isMorseOverCatSupported();
    const QStringList getAvailableRawModes();
    const QList<QPair<int, QString>> getModelList(const DriverID &id) const;
    const QList<QPair<QString, QString>> getPTTTypeList(const DriverID &id) const;
    const QList<QPair<int, QString>> getDriverList() const;
    const RigCaps getRigCaps(const DriverID &, int) const;


public slots:
    void start();
    void open();
    void close();
    void shutdown();
    void stopTimer();

    void setFrequency(double);
    void setFrequency(VFOID, double);
    void setSplit(bool);
    void setRawMode(const QString &rawMode);
    void setMode(const QString &, const QString &, bool = false);
    void setPTT(bool);
    void setKeySpeed(qint16 wpm);
    void syncKeySpeed(qint16 wpm);
    void sendMorse(const QString &text);
    void stopMorse();
    void sendState();
    void sendDXSpot(DxSpot spot);

signals:
    void frequencyChanged(VFOID, double, double, double);
    void splitChanged(VFOID, bool);
    void modeChanged(VFOID, QString, QString, QString, qint32);
    void powerChanged(VFOID, double);
    void keySpeedChanged(VFOID, unsigned int);
    void vfoChanged(VFOID, QString);
    void ritChanged(VFOID, double);
    void xitChanged(VFOID, double);
    void pttChanged(VFOID, bool);
    void rigCWKeyOpenRequest(QString, qint32);
    void rigCWKeyCloseRequest(QString);
    void rigDisconnected();
    void rigConnected();
    void rigErrorPresent(QString, QString);
    void rigStatusChanged(Rig::Status);
    void rigStatusHeartBeat(Rig::Status);

private slots:
    void stopTimerImplt();
    void openImpl();
    void closeImpl();
    void shutdownImpl();

    void setVFOFrequencyImpl(VFOID, double);
    void setSplitImpl(bool);
    void setRawModeImpl(const QString&);
    void setModeImpl(const QString &, const QString &, bool);
    void setPTTImpl(bool);
    void setKeySpeedImpl(qint16 wpm);
    void syncKeySpeedImpl(qint16 wpm);
    void sendMorseImpl(const QString &text);
    void stopMorseImpl();
    void sendStateImpl();
    void sendDXSpotImpl(const DxSpot &spot);
    void sendHeartBeat();

private:
    static const int SHUTDOWN_TIMEOUT_MS = 5000;

    class DrvParams
    {
    public:
        DrvParams(const DriverID id,
                  const QString &driverName,
                  QList<QPair<int, QString>> (*getModelfct)(),
                  RigCaps (*getCapsfct)(int),
                  QList<QPair<QString, QString>> (*getPTTTypefct)()) :
            driverID(id),
            driverName(driverName),
            getModeslListFunction(getModelfct),
            getCapsFunction(getCapsfct),
            getPTTTypeListFunction(getPTTTypefct){};

        DrvParams() :
            driverID(UNDEF_DRIVER),
            getModeslListFunction(nullptr),
            getCapsFunction(nullptr),
            getPTTTypeListFunction(nullptr){};

        DriverID driverID;
        QString driverName;
        QList<QPair<int, QString>> (*getModeslListFunction)();
        RigCaps (*getCapsFunction)(int);
        QList<QPair<QString, QString>> (*getPTTTypeListFunction)();
    };

    QMap<int, DrvParams> drvMapping;

    void __closeRig(const QString &preservedCWKeyProfile = QString());
    void __openRig();
    GenericRigDrv *getDriver(const RigProfile &profile, qint32 controlledRigModel);
    void emitRigStatusChanged();

private:
    GenericRigDrv *rigDriver;
    QMutex rigLock;
    Rig::Status rigStatus;
    RuntimeProvider *statusProvider;
    bool connected;
    QTimer *heartBeatTimer;
    const quint16 HEARTBEATPERIOD = 1000; // in ms
    RigctldManager *rigctldManager = nullptr;
};

Q_DECLARE_METATYPE(Rig::Status);

#endif // RIG_RIG_H
