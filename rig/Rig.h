#ifndef RIG_RIG_H
#define RIG_RIG_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include "rig/drivers/GenericRigDrv.h"
#include "RigCaps.h"
#include "data/DxSpot.h"

enum VFOID
{
    VFO1 = 0
};

Q_DECLARE_METATYPE(VFOID)

class Rig : public QObject
{
    Q_OBJECT
public:

    enum DriverID
    {
        UNDEF_DRIVER = 0,
        HAMLIB_DRIVER = 1,
        OMNIRIG_DRIVER = 2,
        OMNIRIGV2_DRIVER = 3,
        TCI_DRIVER = 4
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

    bool isRigConnected();
    bool isMorseOverCatSupported();
    const QStringList getAvailableRawModes();
    const QList<QPair<int, QString>> getModelList(const DriverID &id) const;
    const QList<QPair<QString, QString>> getPTTTypeList(const DriverID &id) const;
    const QList<QPair<int, QString>> getDriverList() const;
    const RigCaps getRigCaps(const DriverID &, int) const;

public slots:
    void start();
    void update();
    void open();
    void close();
    void stopTimer();

    void setFrequency(double);
    void setRawMode(const QString &rawMode);
    void setMode(const QString &, const QString &);
    void setPTT(bool);
    void setKeySpeed(qint16 wpm);
    void syncKeySpeed(qint16 wpm);
    void sendMorse(const QString &text);
    void stopMorse();
    void sendState();
    void sendDXSpot(DxSpot spot);

signals:
    void frequencyChanged(VFOID, double, double, double);
    void modeChanged(VFOID, QString, QString, QString, qint32);
    void powerChanged(VFOID, double);
    void keySpeedChanged(VFOID, unsigned int);
    void vfoChanged(VFOID, QString);
    void ritChanged(VFOID, double);
    void xitChanged(VFOID, double);
    void pttChanged(VFOID, bool);
    void rigCWKeyOpenRequest(QString);
    void rigCWKeyCloseRequest(QString);
    void rigDisconnected();
    void rigConnected();
    void rigErrorPresent(QString, QString);

private slots:
    void stopTimerImplt();
    void openImpl();
    void closeImpl();

    void setFrequencyImpl(double);
    void setRawModeImpl(const QString&);
    void setModeImpl(const QString &, const QString &);
    void setPTTImpl(bool);
    void setKeySpeedImpl(qint16 wpm);
    void syncKeySpeedImpl(qint16 wpm);
    void sendMorseImpl(const QString &text);
    void stopMorseImpl();
    void sendStateImpl();
    void sendDXSpotImpl(const DxSpot &spot);

private:
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

    void __closeRig();
    void __openRig();
    GenericRigDrv *getDriver(const RigProfile &profile);

private:
    QTimer* timer;
    GenericRigDrv *rigDriver;
    QMutex rigLock;
    bool connected;
};

#endif // RIG_RIG_H
