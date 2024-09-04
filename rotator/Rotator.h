#ifndef QLOG_ROTATOR_ROTATOR_H
#define QLOG_ROTATOR_ROTATOR_H
#include <QTimer>

#include "data/RotProfile.h"
#include "rotator/drivers/GenericRotDrv.h"
#include "RotCaps.h"

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

    static Rotator* instance();
    double getAzimuth();
    double getElevation();
    bool isRotConnected();

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
    void update();
    void open();
    void close();
    void stopTimer();

    void sendState();
    void setPosition(double azimuth, double elevation);

private slots:
    void setPositionImpl(double azimuth, double elevation);
    void stopTimerImplt();
    void openImpl();
    void closeImpl();
    void sendStateImpl();

private:
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

    void __closeRot();
    void __openRot();

    GenericRotDrv *getDriver(const RotProfile &profile);

private:
    QTimer* timer;
    GenericRotDrv *rotDriver;
    QMutex rotLock;
    bool connected;
    double cacheAzimuth;
    double cacheElevation;
};

#endif // QLOG_ROTATOR_ROTATOR_H
