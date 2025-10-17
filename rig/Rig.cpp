#include "Rig.h"
#include "core/debug.h"
#include "rig/drivers/HamlibRigDrv.h"
#ifdef Q_OS_WIN
#include "rig/drivers/OmnirigRigDrv.h"
#include "rig/drivers/Omnirigv2RigDrv.h"
#endif
#include "rig/drivers/TCIRigDrv.h"
#include "rig/drivers/FlrigRigDrv.h"

MODULE_IDENTIFICATION("qlog.rig.rig");

#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Rig mutex"; \
                        QMutexLocker locker(&rigLock); \
                        qCDebug(runtime) << "Using Rig"

Rig::Rig(QObject *parent)
    : QObject{parent},
    rigDriver(nullptr),
    connected(false),
    heartBeatTimer(new QTimer(this))
{
    FCT_IDENTIFICATION;

    drvMapping[HAMLIB_DRIVER] = DrvParams(HAMLIB_DRIVER,
                                          "Hamlib",
                                          &HamlibRigDrv::getModelList,
                                          &HamlibRigDrv::getCaps,
                                          &HamlibRigDrv::getPTTTypeList);
#ifdef Q_OS_WIN
    drvMapping[OMNIRIG_DRIVER] = DrvParams(OMNIRIG_DRIVER,
                                           "Omnirig v1",
                                           &OmnirigRigDrv::getModelList,
                                           &OmnirigRigDrv::getCaps,
                                           nullptr);

    drvMapping[OMNIRIGV2_DRIVER] = DrvParams(OMNIRIGV2_DRIVER,
                                             "Omnirig v2",
                                             &OmnirigV2RigDrv::getModelList,
                                             &OmnirigV2RigDrv::getCaps,
                                             nullptr);
#endif
    drvMapping[TCI_DRIVER] = DrvParams(TCI_DRIVER,
                                       "TCI",
                                       &TCIRigDrv::getModelList,
                                       &TCIRigDrv::getCaps,
                                       nullptr);

    drvMapping[FLRIG_DRIVER] = DrvParams(FLRIG_DRIVER,
                                         "FLRig",
                                         &FlrigRigDrv::getModelList,
                                         &FlrigRigDrv::getCaps,
                                         nullptr);

    connect(heartBeatTimer, &QTimer::timeout, this, &Rig::sendHeartBeat);
}

qint32 Rig::getNormalBandwidth(const QString &mode, const QString &)
{
    FCT_IDENTIFICATION;

    if ( mode == "CW" )
        return 1000;

    if ( mode == "SSB"
         || mode == "PSK"
         || mode == "MFSK"
         || mode == "FT8"
         || mode == "SSTV" )
        return 2500;

    if ( mode == "AM" )
        return 6000;

    if ( mode == "RTTY" )
        return 2400;

    if ( mode == "FM" )
        return 12500;

    return 6000;
}

const QList<QPair<int, QString>> Rig::getModelList(const DriverID &id) const
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

const QList<QPair<QString, QString> > Rig::getPTTTypeList(const DriverID &id) const
{
    FCT_IDENTIFICATION;

    QList<QPair<QString, QString>> ret;

    if ( drvMapping.contains(id)
         && drvMapping.value(id).getPTTTypeListFunction != nullptr )
    {
        ret = (drvMapping.value(id).getPTTTypeListFunction)();
    }
    return ret;
}

const QList<QPair<int, QString>> Rig::getDriverList() const
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

const RigCaps Rig::getRigCaps(const DriverID &id, int model) const
{
    FCT_IDENTIFICATION;

    if ( drvMapping.contains(id)
         && drvMapping.value(id).getCapsFunction != nullptr)
    {
        return (drvMapping.value(id).getCapsFunction)(model);
    }
    return RigCaps();

}

void Rig::stopTimer()
{
    FCT_IDENTIFICATION;
    bool check = QMetaObject::invokeMethod(Rig::instance(), &Rig::stopTimerImplt,
                                           Qt::QueuedConnection);
    Q_ASSERT( check );
}

void Rig::stopTimerImplt()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    heartBeatTimer->stop();

    if ( rigDriver )
        rigDriver->stopTimers();
}

void Rig::start()
{
    FCT_IDENTIFICATION;
}


void Rig::open()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &Rig::openImpl, Qt::QueuedConnection);
}

void Rig::openImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    __openRig();
}

void Rig::__openRig()
{
    FCT_IDENTIFICATION;

    // if rig is active then close it
    __closeRig();

    RigProfile newRigProfile = RigProfilesManager::instance()->getCurProfile1();

    if ( newRigProfile == RigProfile() )
    {
        emit rigErrorPresent(tr("No Rig Profile selected"),
                             QString());
        return;
    }

    qCDebug(runtime) << "Opening profile name: " << newRigProfile.profileName;

    rigDriver = getDriver(newRigProfile);

    if ( !rigDriver )
    {
        // initialization failed
        emit rigErrorPresent(tr("Initialization Error"),
                             tr("Internal Error"));
        return;
    }

    connect( rigDriver, &GenericRigDrv::frequencyChanged, this, [this](double a, double b, double c)
    {
        rigStatus.freq = a;
        emit frequencyChanged(VFO1, a, b, c);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::pttChanged, this, [this](bool a)
    {
        rigStatus.ptt = (a) ? 1 : 0;
        emit pttChanged(VFO1, a);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::modeChanged, this, [this](const QString &a,
                                                                  const QString &b,
                                                                  const QString &c,
                                                                  qint32 d)
    {
        rigStatus.rawmode = a;
        rigStatus.mode = b;
        rigStatus.submode = c;
        rigStatus.bandwidth = d;
        emit modeChanged(VFO1, a, b, c, d);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::vfoChanged, this, [this](const QString &a)
    {
        rigStatus.vfo = a;
        emit vfoChanged(VFO1, a);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::powerChanged, this, [this](double a)
    {
        rigStatus.power = a;
        emit powerChanged(VFO1, a);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::ritChanged, this, [this](double a)
    {
        rigStatus.rit = a;
        emit ritChanged(VFO1, a);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::xitChanged, this, [this](double a)
    {
        rigStatus.xit = a;
        emit xitChanged(VFO1, a);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::keySpeedChanged, this, [this](unsigned int a)
    {
        rigStatus.keySpeed = a;
        emit keySpeedChanged(VFO1, a);
        emitRigStatusChanged();
    });

    connect( rigDriver, &GenericRigDrv::errorOccurred, this, [this](const QString &a,
                                                                const QString &b)
    {
        close();
        emit rigErrorPresent(a, b);
    });

    connect( rigDriver, &GenericRigDrv::rigIsReady, this, [this, newRigProfile]()
    {
        connected = true;

        // Change Assigned CW Key
        if ( !newRigProfile.assignedCWKey.isEmpty()
             || newRigProfile.assignedCWKey != " ")
        {
            emit rigCWKeyOpenRequest(newRigProfile.assignedCWKey);
        }

        rigStatus.profile = newRigProfile.profileName;
        rigStatus.isConnected = true;
        heartBeatTimer->start(HEARTBEATPERIOD);
        emit rigConnected();

        sendState();
    });

    if ( !rigDriver->open() )
    {
        emit rigErrorPresent(tr("Cannot open Rig"),
                             rigDriver->lastError());
        qWarning() << rigDriver->lastError();
        __closeRig();
        return;
    }
}

void Rig::close()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &Rig::closeImpl, Qt::QueuedConnection);
}

void Rig::closeImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    __closeRig();
}

void Rig::__closeRig()
{
    FCT_IDENTIFICATION;

    if ( !rigDriver )
    {
        qCDebug(runtime) << "Driver is not active";
        return;
    }

    const RigProfile &connectedRigProfile = rigDriver->getCurrRigProfile();

    // Change Assigned CW Key
    if ( !connectedRigProfile.assignedCWKey.isEmpty()
         || connectedRigProfile.assignedCWKey != " ")
    {
        emit rigCWKeyCloseRequest(connectedRigProfile.assignedCWKey);
    }

    rigStatus.isConnected = false;
    heartBeatTimer->stop();
    emitRigStatusChanged();
    rigStatus.clear();

    delete rigDriver;
    rigDriver = nullptr;
    connected = false;
    emit rigDisconnected();
}

bool Rig::isRigConnected()
{
    FCT_IDENTIFICATION;

    return connected;
}

bool Rig::isMorseOverCatSupported()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return false;

    return rigDriver->isMorseOverCatSupported();
}

void Rig::setFrequency(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newFreq;

    if ( newFreq > 0.0 )
    {
        QMetaObject::invokeMethod(this, "setFrequencyImpl",
                                  Qt::QueuedConnection, Q_ARG(double,newFreq));
    }
}

void Rig::setFrequencyImpl(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newFreq;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->setFrequency(newFreq);
}

void Rig::setRawMode(const QString& rawMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    if ( rawMode.isEmpty() )
        return;

    QMetaObject::invokeMethod(this, "setRawModeImpl",
                              Qt::QueuedConnection, Q_ARG(QString, rawMode));
}

void Rig::setRawModeImpl(const QString &rawMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->setRawMode(rawMode);
}

void Rig::setMode(const QString &newMode, const QString &newSubMode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newMode << newSubMode << digiVariant;

    if ( newMode.isEmpty()
         && newSubMode.isEmpty() )
        return;

    QMetaObject::invokeMethod(this, "setModeImpl",
                              Qt::QueuedConnection, Q_ARG(QString, newMode), Q_ARG(QString, newSubMode), Q_ARG(bool, digiVariant));
}

void Rig::setModeImpl(const QString &newMode, const QString &newSubMode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newMode << newSubMode << digiVariant;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->setMode(newMode, newSubMode, digiVariant);
}

void Rig::setPTT(bool active)
{
    FCT_IDENTIFICATION;
    QMetaObject::invokeMethod(this, "setPTTImpl", Qt::QueuedConnection,
                              Q_ARG(bool, active));

}

void Rig::setPTTImpl(bool active)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << active;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->setPTT(active);
}

void Rig::setKeySpeed(qint16 wpm)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setKeySpeedImpl", Qt::QueuedConnection,
                              Q_ARG(qint16, wpm));
}

void Rig::setKeySpeedImpl(qint16 wpm)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << wpm;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->setKeySpeed(wpm);
}

void Rig::syncKeySpeed(qint16 wpm)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "syncKeySpeedImpl", Qt::QueuedConnection,
                              Q_ARG(qint16, wpm));
}

void Rig::syncKeySpeedImpl(qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->syncKeySpeed(wpm);
}

void Rig::sendMorse(const QString &text)
{
    FCT_IDENTIFICATION;

    if ( text.isEmpty() )
        return;

    QMetaObject::invokeMethod(this, "sendMorseImpl", Qt::QueuedConnection,
                              Q_ARG(QString, text));
}

void Rig::sendMorseImpl(const QString &text)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << text;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->sendMorse(text);
}

void Rig::stopMorse()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "stopMorseImpl", Qt::QueuedConnection);
}

void Rig::stopMorseImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->stopMorse();
}

void Rig::sendState()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "sendStateImpl", Qt::QueuedConnection);
}

void Rig::sendStateImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->sendState();
}

void Rig::sendDXSpot(DxSpot spot)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "sendDXSpotImpl", Qt::QueuedConnection,
                              Q_ARG(DxSpot, spot));
}

void Rig::sendDXSpotImpl(const DxSpot &spot)
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return;

    rigDriver->sendDXSpot(spot);
}

GenericRigDrv *Rig::getDriver( const RigProfile &profile )
{
    FCT_IDENTIFICATION;

    //(if) for testing purpose
#if 1
    qCDebug(runtime) << profile.driver;

    switch ( profile.driver )
    {
    case Rig::HAMLIB_DRIVER:
        return new HamlibRigDrv(profile, this);
        break;
#ifdef Q_OS_WIN
    case Rig::OMNIRIG_DRIVER:
        return new OmnirigRigDrv(profile, this);
        break;
    case Rig::OMNIRIGV2_DRIVER:
        return new OmnirigV2RigDrv(profile, this);
        break;
#endif
    case Rig::TCI_DRIVER:
        return new TCIRigDrv(profile, this);
        break;

    case Rig::FLRIG_DRIVER:
        return new FlrigRigDrv(profile, this);
        break;

    default:
        qWarning() << "Unsupported Rig Driver " << profile.driver;
        return nullptr;
    }
#else
#ifdef Q_OS_WIN
    return new OmnirigV2Drv(profile, this);
#endif
    return new HamlibDrv(profile, this);
#endif
}

void Rig::emitRigStatusChanged()
{
    FCT_IDENTIFICATION;

    // it can happen especially with TCI that Rig received information,
    // emit signals but rig is not connected.
    if ( rigStatus.profile.isEmpty() )
        return;

    emit rigStatusChanged(rigStatus);
}

const QStringList Rig::getAvailableRawModes()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rigDriver )
        return QStringList();

    return rigDriver->getAvailableModes();;
}

Rig::~Rig()
{
    FCT_IDENTIFICATION;

    __closeRig();
}

void Rig::sendHeartBeat()
{
    FCT_IDENTIFICATION;

    // The functionality is similar to emitRigStatusChanged, but emits
    // a different signal to distinguish these two cases for future use.

    if ( !rigStatus.isConnected || rigStatus.profile.isEmpty() )
        return;

    emit rigStatusHeartBeat(rigStatus);
}
#undef MUTEXLOCKER
