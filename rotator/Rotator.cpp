#include "rotator/Rotator.h"
#include "core/debug.h"
#include "rotator/drivers/HamlibRotDrv.h"
#include "rotator/drivers/PSTRotDrv.h"

MODULE_IDENTIFICATION("qlog.rotator.rotator");

#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Rot mutex"; \
                        QMutexLocker locker(&rotLock); \
                        qCDebug(runtime) << "Using Rot"


#define TIME_PERIOD 1000

Rotator::Rotator(QObject *parent) :
    QObject{parent},
    rotDriver(nullptr),
    connected(false),
    cacheAzimuth(0.0),
    cacheElevation(0.0)
{
    FCT_IDENTIFICATION;

    drvMapping[HAMLIB_DRIVER] = DrvParams(HAMLIB_DRIVER,
                                          "Hamlib",
                                          &HamlibRotDrv::getModelList,
                                          &HamlibRotDrv::getCaps);
    drvMapping[PSTROTATOR_DRIVER] = DrvParams(PSTROTATOR_DRIVER,
                                          "PSTRotator",
                                          &PSTRotDrv::getModelList,
                                          &PSTRotDrv::getCaps);
}

Rotator::~Rotator()
{
    FCT_IDENTIFICATION;

    __closeRot();
}

double Rotator::getAzimuth()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    return cacheAzimuth;
}

double Rotator::getElevation()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    return cacheElevation;
}

bool Rotator::isRotConnected()
{
    FCT_IDENTIFICATION;

    return connected;
}

const QList<QPair<int, QString> > Rotator::getModelList(const DriverID &id) const
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    if ( drvMapping.contains(id)
         && drvMapping.value(id).getModeslListFunction != nullptr )
    {
        ret = (drvMapping.value(id).getModeslListFunction)();
    }
    return ret;
}

const QList<QPair<int, QString> > Rotator::getDriverList() const
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    const QList<int> &keys = drvMapping.keys();

    for ( const int &key : keys )
    {
        ret << QPair<int, QString>(key, drvMapping[key].driverName);
    }

    return ret;
}

const RotCaps Rotator::getRotCaps(const DriverID &id, int model) const
{
    FCT_IDENTIFICATION;

    if ( drvMapping.contains(id)
         && drvMapping.value(id).getCapsFunction != nullptr)
    {
        return (drvMapping.value(id).getCapsFunction)(model);
    }
    return RotCaps();
}

void Rotator::stopTimer()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    bool check = QMetaObject::invokeMethod(Rotator::instance(),
                                           &Rotator::stopTimerImplt,
                                           Qt::QueuedConnection);
    Q_ASSERT( check );
}

void Rotator::stopTimerImplt()
{
    FCT_IDENTIFICATION;

    if ( rotDriver )
        rotDriver->stopTimers();
}

void Rotator::start()
{
    FCT_IDENTIFICATION;
}

void Rotator::open()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &Rotator::openImpl, Qt::QueuedConnection);
}

void Rotator::openImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    __openRot();
}

void Rotator::sendState()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "sendStateImpl", Qt::QueuedConnection);
}

void Rotator::sendStateImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rotDriver )
        return;

    rotDriver->sendState();
}

void Rotator::__openRot()
{
    FCT_IDENTIFICATION;

    // if rot is active then close it
    __closeRot();

    RotProfile newRotProfile = RotProfilesManager::instance()->getCurProfile1();

    if ( newRotProfile == RotProfile() )
    {
        emit rotErrorPresent(tr("No Rotator Profile selected"),
                             QString());
        return;
    }

    qCDebug(runtime) << "Opening profile name: " << newRotProfile.profileName;

    rotDriver = getDriver(newRotProfile);

    if ( !rotDriver )
    {
        // initialization failed
        emit rotErrorPresent(tr("Initialization Error"),
                             tr("Internal Error"));
        return;
    }

    connect(rotDriver, &GenericRotDrv::positioningChanged, this, [this](double a, double b)
    {
        cacheAzimuth = a;
        cacheElevation = b;
        emit positionChanged(a, b);
    });

    connect(rotDriver, &GenericRotDrv::errorOccurred, this, [this](const QString &a,
                                                                const QString &b)
    {
        close();
        emit rotErrorPresent(a, b);
    });

    connect(rotDriver, &GenericRotDrv::rotIsReady, this, [this, newRotProfile]()
    {
        connected = true;

        emit rotConnected();

        sendState();
    });

    if ( !rotDriver->open() )
    {
        emit rotErrorPresent(tr("Cannot open Rotator"),
                             rotDriver->lastError());
        qWarning() << rotDriver->lastError();
        __closeRot();
        return;
    }
}

GenericRotDrv *Rotator::getDriver(const RotProfile &profile)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << profile.driver;

    switch ( profile.driver )
    {
    case Rotator::HAMLIB_DRIVER:
        return new HamlibRotDrv(profile, this);
        break;
    case Rotator::PSTROTATOR_DRIVER:
        return new PSTRotDrv(profile, this);
        break;
    default:
        qWarning() << "Unsupported Rotator Driver " << profile.driver;
    }

    return nullptr;
}

void Rotator::close()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &Rotator::closeImpl, Qt::QueuedConnection);
}


void Rotator::closeImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    __closeRot();
}

void Rotator::__closeRot()
{
    FCT_IDENTIFICATION;

    if ( !rotDriver )
    {
        qCDebug(runtime) << "Driver is not active";
        return;
    }

    delete rotDriver;
    rotDriver = nullptr;
    connected = false;
    emit rotDisconnected();
}


void Rotator::setPosition(double azimuth, double elevation)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setPositionImpl", Qt::QueuedConnection,
                              Q_ARG(double,azimuth), Q_ARG(double,elevation));
}


void Rotator::setPositionImpl(double azimuth, double elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<azimuth<< " " << elevation;

    MUTEXLOCKER;

    if ( ! rotDriver )
        return;

    rotDriver->setPosition(azimuth, elevation);
}

#undef MUTEXLOCKER
