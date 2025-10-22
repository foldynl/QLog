#include <QRegularExpression>
#include <cmath>

#include "HamlibRotDrv.h"
#include "core/debug.h"
#include "data/SerialPort.h"
#include "data/AntProfile.h"

#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Rot Drv mutex"; \
                        QMutexLocker locker(&drvLock); \
                        qCDebug(runtime) << "Using Rot Drv"

#ifndef HAMLIB_FILPATHLEN
#define HAMLIB_FILPATHLEN FILPATHLEN
#endif

#ifndef RIG_IS_SOFT_ERRCODE
#define RIG_IS_SOFT_ERRCODE(errcode) (errcode == RIG_EINVAL || errcode == RIG_ENIMPL || errcode == RIG_ERJCTED \
    || errcode == RIG_ETRUNC || errcode == RIG_ENAVAIL || errcode == RIG_ENTARGET \
    || errcode == RIG_EVFO || errcode == RIG_EDOM)

#endif

MODULE_IDENTIFICATION("qlog.rotator.driver.hamlibdrv");

QList<QPair<int, QString> > HamlibRotDrv::getModelList()
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    rot_load_all_backends();
    rot_list_foreach(addRig, &ret);

    return ret;
}

RotCaps HamlibRotDrv::getCaps(int model)
{
    FCT_IDENTIFICATION;

    const struct rot_caps *caps = rot_get_caps(model);
    RotCaps ret;

    ret.isNetworkOnly = (model == RIG_MODEL_NETRIGCTL);

    ret.serialDataBits = caps->serial_data_bits;
    ret.serialStopBits = caps->serial_stop_bits;

    return ret;
}

int HamlibRotDrv::addRig(const rot_caps *caps, void *data)
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> *list = static_cast<QList<QPair<int, QString>>*>(data);

    QString name = QString("%1 %2 (%3)").arg(caps->mfg_name,
                                             caps->model_name,
                                             caps->version);

    list->append(QPair<int, QString>(caps->rot_model, name));
    return -1;
}

HamlibRotDrv::HamlibRotDrv(const RotProfile &profile,
                     QObject *parent)
    : GenericRotDrv{profile, parent},
      rot(nullptr),
      forceSendState(false)
{
    FCT_IDENTIFICATION;

    rot_load_all_backends();

    rot = rot_init(rotProfile.model);

    if ( !rot )
    {
        // initialization failed
        qCDebug(runtime) << "Cannot allocate Rotator structure";
        lastErrorText = tr("Initialization Error");
    }

    rig_set_debug(RIG_DEBUG_BUG);

    connect(&errorTimer, &QTimer::timeout,
            this, &HamlibRotDrv::checkErrorCounter);

}

HamlibRotDrv::~HamlibRotDrv()
{
    FCT_IDENTIFICATION;

    if ( !drvLock.tryLock(200) )
    {
        qCDebug(runtime) << "Waited too long";
        // better to make a memory leak
        return;
    }

    if ( rot )
    {
        rot_close(rot);
        rot_cleanup(rot);
        rot = nullptr;
    }
    drvLock.unlock();
}

bool HamlibRotDrv::open()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( !rot )
    {
        // initialization failed
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Rot is not initialized";
        return false;
    }

    RotProfile::rotPortType portType = rotProfile.getPortType();

    if ( portType == RotProfile::NETWORK_ATTACHED )
    {
        //handling Network Radio
        const QString portString = rotProfile.hostname + ":" + QString::number(rotProfile.netport);
        strncpy(rot->state.rotport.pathname, portString.toLocal8Bit().constData(), HAMLIB_FILPATHLEN - 1);
    }
    else if ( portType == RotProfile::SERIAL_ATTACHED )
    {
        //handling Serial Port Radio
        strncpy(rot->state.rotport.pathname, rotProfile.portPath.toLocal8Bit().constData(), HAMLIB_FILPATHLEN - 1);
        rot->state.rotport.parm.serial.rate = rotProfile.baudrate;
        rot->state.rotport.parm.serial.data_bits = rotProfile.databits;
        rot->state.rotport.parm.serial.stop_bits = rotProfile.stopbits;
        rot->state.rotport.parm.serial.handshake = stringToHamlibFlowControl(rotProfile.flowcontrol);
        rot->state.rotport.parm.serial.parity = stringToHamlibParity(rotProfile.parity);
    }
    else
    {
        lastErrorText = tr("Unsupported Rotator Driver");
        qCDebug(runtime) << "Rot Open Error" << lastErrorText;
        return false;
    }

    int status = rot_open(rot);

    if ( !isRotRespOK(status, tr("Rot Open Error"), false) )
        return false;

    qCDebug(runtime) << "Rot Open - OK";

    opened = true;

    connect(&timer, &QTimer::timeout, this, &HamlibRotDrv::checkRotStateChange);
    timer.start(POLL_INTERVAL);
    emit rotIsReady();
    return true;
}

void HamlibRotDrv::sendState()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    forceSendState = true;
}

// in_azimuth range is 0 - 360
// in_elevation, not used - always 0
void HamlibRotDrv::setPosition(double in_azimuth, double in_elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth << in_elevation;

    MUTEXLOCKER;

    if ( !rot || !rot->caps )
    {
        qCWarning(runtime) << "Rot is not active";
        return;
    }

    if ( rot->caps->max_az == 0 )
    {
        qCWarning(runtime) << "Rot does not support the Azimuth setting";
        return;
    }

    double newElevation = in_elevation;
    double newAzimuth = in_azimuth - AntProfilesManager::instance()->getCurProfile1().azimuthOffset;
    // offset interval is -180 to 180
    // azimuth input interval is 0 to 360
    // min value is -180
    // max valus is 540
    newAzimuth = fmod(newAzimuth + 360, 360);
    qCDebug(runtime) << "Azimuth (with offset)" << newAzimuth;

    /**********************************/
    /* Rotator specific modifications */
    /**********************************/

    qCDebug(runtime) << "Rotator limits: AZ"
                     << rot->caps->min_az << "to" << rot->caps->max_az
                     << "EL"
                     << rot->caps->min_el << "to" << rot->caps->max_el;

    // recalculate azimuth to interval -180 to 180 for rotators that have this interval
    if ( rot->caps->max_az == 180.0 && rot->caps->min_az == -180.0 )
    {
        if ( newAzimuth > 180.0 )
        {
            qCDebug(runtime) << "Azimuth need to be recalculated to interval -180 to 180";
            newAzimuth -= 360.0;
        }
    }
    else if ( rot->caps->max_az == 359.9 && newAzimuth > 359.9 )
    {
        // exception for Green Heron RT-21
        newAzimuth = 0;
    }

    /************************/
    /* END of modifications */
    /************************/

    // the final value of the azimuth and elevation is known
    qCDebug(runtime) << "Set Az/El position"
                     << static_cast<azimuth_t>(newAzimuth)
                     << static_cast<elevation_t>(newElevation);

    int status = rot_set_position(rot,
                                  static_cast<azimuth_t>(newAzimuth),
                                  static_cast<elevation_t>(newElevation));

    isRotRespOK(status, tr("Set Possition Error"), false);

    // wait a moment because Rotators are slow and they are not possible to set and get
    // mode so quickly (get mode is called in the main thread's update() function
    commandSleep();
}

void HamlibRotDrv::stopTimers()
{
    FCT_IDENTIFICATION;

    timer.stop();
    errorTimer.stop();
}

void HamlibRotDrv::checkRotStateChange()
{
    FCT_IDENTIFICATION;

    if ( !drvLock.tryLock(200) )
    {
        qCDebug(runtime) << "Waited too long";
        return;
    }

    qCDebug(runtime) << "Getting Rot state";

    checkChanges();

    forceSendState = false;

    // restart timer
    timer.start(POLL_INTERVAL);
    drvLock.unlock();
}

void HamlibRotDrv::checkErrorCounter()
{
    FCT_IDENTIFICATION;

    if ( postponedErrors.isEmpty() )
       return;

    qCDebug(runtime) << postponedErrors;

     // emit only one error
    auto it = postponedErrors.constBegin();
    emit errorOccured(it.key(), it.value());
    postponedErrors.clear();
}

void HamlibRotDrv::checkChanges()
{
    FCT_IDENTIFICATION;

    checkAzEl();
}

void HamlibRotDrv::checkAzEl()
{
    FCT_IDENTIFICATION;

    if ( !rot )
    {
        qCWarning(runtime) << "Rot is not active";
        return;
    }

    if ( rot->caps->get_position )
    {
        azimuth_t az;
        elevation_t el;

        int status = rot_get_position(rot, &az, &el);
        if ( isRotRespOK(status, tr("Get Possition Error")) )
        {
            double newAzimuth = az;
            double newElevation = el;
            // Azimuth Normalization (-180,180) -> (0,360) - ADIF defined interval is 0-360
            newAzimuth += AntProfilesManager::instance()->getCurProfile1().azimuthOffset;
            newAzimuth = normalizeAzimuth(newAzimuth);

             qCDebug(runtime) << "Rot Position: " << newAzimuth << newElevation;
             qCDebug(runtime) << "Object Position: "<< azimuth << elevation;

            if ( newAzimuth != azimuth
                 || newElevation != elevation
                 || forceSendState)
            {
                azimuth = newAzimuth;
                elevation = newElevation;
                qCDebug(runtime) << "emitting POSITIONING changed";
                emit positioningChanged(azimuth, elevation);
            }
        }
    }
    else
        qCDebug(runtime) << "Get POSITION is disabled";
}

bool HamlibRotDrv::isRotRespOK(int errorStatus, const QString errorName, bool emitError)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << errorStatus << errorName << emitError;

    if ( errorStatus == RIG_OK ) // there are no special codes for ROT, use RIG codes
    {
        if ( emitError )
            postponedErrors.remove(errorName);
        return true;
    }

    lastErrorText = hamlibErrorString(errorStatus);

    if ( emitError )
    {
        qCDebug(runtime) << "Emit Error detected";

        if ( !RIG_IS_SOFT_ERRCODE(-errorStatus) )
        {
            // hard error, emit error now
            qCDebug(runtime) << "Hard Error";
            emit errorOccured(errorName, lastErrorText);
        }
        else
        {
            // soft error
            postponedErrors[errorName] = lastErrorText;

            if ( !errorTimer.isActive() )
            {
                qCDebug(runtime) << "Starting Error Timer";
                errorTimer.start(15 * 1000); //15s
            }
        }
    }
    return false;
}

serial_handshake_e HamlibRotDrv::stringToHamlibFlowControl(const QString &in_flowcontrol)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_flowcontrol;

    const QString &flowcontrol = in_flowcontrol.toLower();

    if ( flowcontrol == SerialPort::SERIAL_FLOWCONTROL_SOFTWARE )
        return RIG_HANDSHAKE_XONXOFF;
    if ( flowcontrol == SerialPort::SERIAL_FLOWCONTROL_HARDWARE )
        return RIG_HANDSHAKE_HARDWARE;

    return RIG_HANDSHAKE_NONE;
}

serial_parity_e HamlibRotDrv::stringToHamlibParity(const QString &in_parity)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_parity;

    const QString &parity = in_parity.toLower();

    if ( parity == SerialPort::SERIAL_PARITY_EVEN )
        return RIG_PARITY_EVEN;
    if ( parity == SerialPort::SERIAL_PARITY_ODD )
        return RIG_PARITY_ODD;
    if ( parity == SerialPort::SERIAL_PARITY_MARK )
        return RIG_PARITY_MARK;
    if ( parity == SerialPort::SERIAL_PARITY_SPACE )
        return RIG_PARITY_SPACE;

    return RIG_PARITY_NONE;
}

QString HamlibRotDrv::hamlibErrorString(int errorCode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << errorCode;

    QString ret;
    QString detail(rigerror(errorCode));

#if ( HAMLIBVERSION_MAJOR >= 4 && HAMLIBVERSION_MINOR >= 5 )
    // The rigerror has different behavior since 4.5. It contains the stack trace in the first part
    // Need to use rigerror2
    ret = QString(rigerror2(errorCode));
#else
    static QRegularExpression re("[\r\n]");
    QStringList errorList = detail.split(re);

    if ( errorList.size() >= 1 )
        ret = errorList.at(0);
#endif
    qWarning() << "Detail" << detail;
    qCWarning(runtime) << ret;

    return ret;
}

void HamlibRotDrv::commandSleep()
{
    FCT_IDENTIFICATION;

    QThread::msleep(100);
}

#undef MUTEXLOCKER
