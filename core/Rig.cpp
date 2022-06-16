#include <cstring>
#include <qglobal.h>
#include <hamlib/rig.h>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Rig.h"
#include "core/debug.h"
#include "data/RigProfile.h"

MODULE_IDENTIFICATION("qlog.core.rig");

#ifndef HAMLIB_FILPATHLEN
#define HAMLIB_FILPATHLEN FILPATHLEN
#endif

#define STARTING_UPDATE_INTERVAL 500

Rig* Rig::instance() {
    FCT_IDENTIFICATION;

    static Rig instance;
    return &instance;
}

bool Rig::isNetworkRig(const struct rig_caps *caps)
{
    FCT_IDENTIFICATION;

    bool ret = false;

    if ( caps )
    {
        ret = caps->port_type == RIG_PORT_NETWORK
               || caps->port_type == RIG_PORT_UDP_NETWORK;
    }

    qCDebug(runtime) << ret;

    return ret;
}

QString Rig::getModeNormalizedText(rmode_t hamlibMode, QString &submode)
{
    FCT_IDENTIFICATION;

    switch ( hamlibMode )
    {
    case RIG_MODE_AM: return "AM";
    case RIG_MODE_CW: return "CW";
    case RIG_MODE_USB: {submode = "USB"; return "SSB";}
    case RIG_MODE_LSB: {submode = "LSB"; return "SSB";}
    case RIG_MODE_RTTY: return "RTTY";
    case RIG_MODE_FM: return "FM";
    case RIG_MODE_WFM: return "FM";
    case RIG_MODE_CWR: return "CW";
    case RIG_MODE_RTTYR: return "RTTY";
    case RIG_MODE_AMS: return "AM";
    case RIG_MODE_PKTLSB: {submode = "LSB"; return "SSB";}
    case RIG_MODE_PKTUSB: {submode = "USB"; return "SSB";}
    case RIG_MODE_PKTFM: return "FM";
    case RIG_MODE_ECSSUSB: {submode = "USB"; return "SSB";}
    case RIG_MODE_ECSSLSB: {submode = "LSB"; return "SSB";}
    case RIG_MODE_FAX: return "";
    case RIG_MODE_SAM: return "";
    case RIG_MODE_SAL: return "AM";
    case RIG_MODE_SAH: return "AM";
    case RIG_MODE_DSB: return "";
    case RIG_MODE_FMN: return "FM";
    case RIG_MODE_PKTAM: return "AM";
    default :
        submode = QString();
        return QString();
    }
}

void Rig::stopTimer()
{
    FCT_IDENTIFICATION;
    bool check = QMetaObject::invokeMethod(Rig::instance(), &Rig::stopTimerImplt, Qt::QueuedConnection);
    Q_ASSERT( check );
}

void Rig::start() {
    FCT_IDENTIFICATION;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(500);
}

void Rig::update()
{
    FCT_IDENTIFICATION;

    if (!rig)
    {
        /* rig is not connected, slow down */
        timer->start(2000);
        return;
    }

    if (!rigLock.tryLock(200)) return;

    RigProfile currRigProfile = RigProfilesManager::instance()->getCurProfile1();

    /***********************************************************/
    /* Is Opened Profile still the globbaly used Rig Profile ? */
    /* if NO then reconnect it                                 */
    /***********************************************************/
    if ( currRigProfile != connectedRigProfile)
    {
        /* Rig Profile Changed
         * Need to reconnect rig
         */
        qCDebug(runtime) << "Reconnecting to a new RIG - " << currRigProfile.profileName << "; Old - " << connectedRigProfile.profileName;
        __openRig();
        timer->start(STARTING_UPDATE_INTERVAL);
        rigLock.unlock();
        return;
    }

    /***********/
    /* Get PTT */
    /***********/
    if ( connectedRigProfile.getPTTInfo
         && rig->caps->get_ptt
         && ( rig->caps->ptt_type == RIG_PTT_RIG
              || rig->caps->ptt_type == RIG_PTT_RIG_MICDATA)
        )
    {
        ptt_t pttHamlib;

        int status = rig_get_ptt(rig, RIG_VFO_CURR, &pttHamlib);

        if ( status == RIG_OK )
        {
            bool ptt = ( pttHamlib == RIG_PTT_OFF ) ? false : true;

            qCDebug(runtime) << "Current PTT state: "<< ptt;
            qCDebug(runtime) << "Current LO PTT state: "<< LoA.getPTT();

            if ( ptt != LoA.getPTT() )
            {
                LoA.setPTT(ptt);

                qCDebug(runtime) << "PTT changed - emitting: " << LoA.getPTT();

                emit pttChanged(LoA.getID(), LoA.getPTT());
            }
        }
        else
        {
            /* Ignore error */
            /*
        __closeRig();
        emit rigErrorPresent(QString(tr("Get Level Error - ")) + QString(rigerror(status)));
        */
        }
    }
    else
    {
        qCDebug(runtime) << "Get PTT is disabled";
    }


    /************/
    /* Get Freq */
    /************/
    if ( connectedRigProfile.getFreqInfo
         && rig->caps->get_freq )
    {
        freq_t vfo_freq;
        int status = rig_get_freq(rig, RIG_VFO_CURR, &vfo_freq);

        if ( status == RIG_OK )
        {
            qCDebug(runtime) << "Current RIG raw FREQ: "<< QSTRING_FREQ(Hz2MHz(vfo_freq));
            qCDebug(runtime) << "Current LO raw FREQ: "<< QSTRING_FREQ(Hz2MHz(LoA.getFreq()));

            if ( vfo_freq != LoA.getFreq() )
            {
                LoA.setFreq(vfo_freq);

                qCDebug(runtime) << "FREQ changed - emitting: " << QSTRING_FREQ(Hz2MHz(LoA.getFreq()))
                                                                << QSTRING_FREQ(Hz2MHz(LoA.getRITFreq()))
                                                                << QSTRING_FREQ(Hz2MHz(LoA.getXITFreq()));
                emit frequencyChanged(LoA.getID(),
                                      Hz2MHz(LoA.getFreq()),
                                      Hz2MHz(LoA.getRITFreq()),
                                      Hz2MHz(LoA.getXITFreq()));
            }
        }
        else
        {
            __closeRig();
            emit rigErrorPresent(tr("Get Frequency Error"),
                                 hamlibErrorString(status));
            timer->start(STARTING_UPDATE_INTERVAL);
            rigLock.unlock();
            return;
        }
    }
    else
    {
        qCDebug(runtime) << "Get Freq is disabled";
    }

    /************/
    /* Get Mode */
    /************/
    if ( connectedRigProfile.getModeInfo
         && rig->caps->get_mode )
    {
        pbwidth_t pbwidth;
        rmode_t curr_modeId;

        int status = rig_get_mode(rig, RIG_VFO_CURR, &curr_modeId, &pbwidth);

        if ( status == RIG_OK )
        {
            qCDebug(runtime) << "Current RIG raw MODE: "<< curr_modeId;
            qCDebug(runtime) << "Current LO raw MODE: "<< LoA.getMode();

            if ( curr_modeId != LoA.getMode() )
            {
                // mode change
                LoA.setMode(curr_modeId);

                QString submode;
                QString mode = LoA.getModeNormalizedText(submode);

                qCDebug(runtime) << "MODE changed - emitting: " << LoA.getModeText() << mode << submode;
                emit modeChanged(LoA.getID(),
                                 LoA.getModeText(),
                                 mode, submode);
            }
        }
        else
        {
            __closeRig();
            emit rigErrorPresent(tr("Get Mode Error"),
                                 hamlibErrorString(status));
            timer->start(STARTING_UPDATE_INTERVAL);
            rigLock.unlock();
            return;
        }
    }
    else
    {
        qCDebug(runtime) << "Get Mode is disabled";
    }

    /************/
    /* Get VFO  */
    /************/
    if ( connectedRigProfile.getVFOInfo
         && rig->caps->get_vfo )
    {
        vfo_t curr_vfo;

        int status = rig_get_vfo(rig, &curr_vfo);

        if ( status == RIG_OK )
        {
            qCDebug(runtime) << "Current RIG raw VFO: "<< curr_vfo;
            qCDebug(runtime) << "Current LO raw VFO: "<< LoA.getVFO();

            if ( curr_vfo != LoA.getVFO() )
            {
                LoA.setVFO(curr_vfo);

                qCDebug(runtime) << "VFO changed - emitting: " << LoA.getVFOText();

                emit vfoChanged(LoA.getID(), LoA.getVFOText());
            }
        }
        else
        {
            /* Ignore error */
            /*
        __closeRig();
        emit rigErrorPresent(QString(tr("Get Level Error - ")) + QString(rigerror(status)));
        */
        }
    }
    else
    {
        qCDebug(runtime) << "Get VFO is disabled";
    }
    /************/
    /* Get PWR  */
    /************/

    if ( connectedRigProfile.getPWRInfo
         && rig->caps->get_level
         && rig->caps->power2mW )
    {
        value_t rigPowerLevel;
        unsigned int rigPower;

        int status = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &rigPowerLevel);

        if ( status == RIG_OK )
        {
            status = rig_power2mW(rig, &rigPower, rigPowerLevel.f, LoA.getFreq(), LoA.getMode());

            if (  status == RIG_OK )
            {
                qCDebug(runtime) << "Current RIG raw PWR: "<< rigPower;
                qCDebug(runtime) << "Current LO raw PWR: "<< LoA.getPower();

                if (rigPower != LoA.getPower())
                {
                    LoA.setPower(rigPower);

                    qCDebug(runtime) << "PWR changed - emitting: " << mW2W(LoA.getPower());

                    emit powerChanged(LoA.getID(), mW2W(LoA.getPower()));
                }
                else
                {
                    /* Ignore error */
                    /*
            __closeRig();
            emit rigErrorPresent(QString(tr("Get Power Error - ")) + QString(rigerror(status)));
            */
                }
            }
        }
        else
        {
            /* Ignore error */
            /*
        __closeRig();
        emit rigErrorPresent(QString(tr("Get Level Error - ")) + QString(rigerror(status)));
        */
        }
    }
    else
    {
        qCDebug(runtime) << "Get PWR is disabled";
    }

    /************/
    /* Get RIT  */
    /************/
    if ( connectedRigProfile.getRITInfo
         && rig->caps->get_rit
         && rig_has_get_func(rig, RIG_FUNC_RIT) )
    {
        int ritStatus;
        shortfreq_t rit = s_Hz(0);

        if ( rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_RIT, &ritStatus) == RIG_OK )
        {
            if ( ritStatus )
            {
                /* RIT is on */
                if ( rig_get_rit(rig, RIG_VFO_CURR, &rit) != RIG_OK )
                {
                    qWarning() << "Cannot get RIT";
                    rit = s_Hz(0);
                }
            }
            else
            {
                /* RIT is off */
                rit = s_Hz(0);
            }

            qCDebug(runtime) << "Current RIG raw RIT: "<< rit;
            qCDebug(runtime) << "Current LO raw RIT: "<< LoA.getRXOffset();
            qCDebug(runtime) << "Current RIG RIT State: " << ritStatus;

            if ( static_cast<double>(rit) != LoA.getRXOffset() )
            {
                LoA.setRXOffset(rit);

                qCDebug(runtime) << "RIT changed - emitting: " << QSTRING_FREQ(Hz2MHz(LoA.getRXOffset()));
                qCDebug(runtime) << "FREQ changed - emitting: " << QSTRING_FREQ(Hz2MHz(LoA.getFreq()))
                                                                << QSTRING_FREQ(Hz2MHz(LoA.getRITFreq()))
                                                                << QSTRING_FREQ(Hz2MHz(LoA.getXITFreq()));

                emit ritChanged(LoA.getID(), Hz2MHz(LoA.getRXOffset()));
                emit frequencyChanged(LoA.getID(),
                                      Hz2MHz(LoA.getFreq()),
                                      Hz2MHz(LoA.getRITFreq()),
                                      Hz2MHz(LoA.getXITFreq()));
            }
        }
        else
        {
            qWarning() << "Cannot get RIG function RIG_FUNC_RIT";
        }
    }
    else
    {
        qCDebug(runtime) << "Get RIT is disabled";
    }

    /************/
    /* Get XIT  */
    /************/

    if ( connectedRigProfile.getXITInfo
         && rig->caps->get_xit
         && rig_has_get_func(rig, RIG_FUNC_XIT) )
    {
        int xitStatus;
        shortfreq_t xit = s_Hz(0);

        if ( rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_XIT, &xitStatus) == RIG_OK )
        {
            if ( xitStatus )
            {
                /* XIT is on */
                if ( rig_get_xit(rig, RIG_VFO_CURR, &xit) != RIG_OK )
                {
                    qWarning() << "Cannot get XIT";
                    xit = s_Hz(0);
                }
            }
            else
            {
                /* XIT is off */
                xit = s_Hz(0);
            }

            qCDebug(runtime) << "Current RIG raw XIT: "<< xit;
            qCDebug(runtime) << "Current LO raw XIT: "<< LoA.getTXOffset();
            qCDebug(runtime) << "Current RIG XIT State: " << xitStatus;

            if ( static_cast<double>(xit) != LoA.getTXOffset() )
            {
                LoA.setTXOffset(xit);

                qCDebug(runtime) << "XIT changed - emitting: " << QSTRING_FREQ(Hz2MHz(LoA.getTXOffset()));
                qCDebug(runtime) << "FREQ changed - emitting: " << QSTRING_FREQ(Hz2MHz(LoA.getFreq()))
                                                                << QSTRING_FREQ(Hz2MHz(LoA.getRITFreq()))
                                                                << QSTRING_FREQ(Hz2MHz(LoA.getXITFreq()));

                emit xitChanged(LoA.getID(), Hz2MHz(LoA.getTXOffset()));
                emit frequencyChanged(LoA.getID(),
                                      Hz2MHz(LoA.getFreq()),
                                      Hz2MHz(LoA.getRITFreq()),
                                      Hz2MHz(LoA.getXITFreq()));
            }
        }
        else
        {
            qWarning() << "Cannot get RIG function RIG_FUNC_XIT";
        }
    }
    else
    {
        qCDebug(runtime) << "Get XIT is disabled";
    }

    timer->start(connectedRigProfile.pollInterval);
    rigLock.unlock();
}

void Rig::open()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &Rig::openImpl, Qt::QueuedConnection);
}

void Rig::openImpl()
{
    FCT_IDENTIFICATION;

    rigLock.lock();
    __openRig();
    rigLock.unlock();
}

void Rig::close()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &Rig::closeImpl, Qt::QueuedConnection);
}

void Rig::closeImpl()
{
    FCT_IDENTIFICATION;

    rigLock.lock();
    __closeRig();
    rigLock.unlock();
}

void Rig::__closeRig()
{
    FCT_IDENTIFICATION;

    connectedRigProfile = RigProfile();
    LoA.clear();

    if ( rig )
    {
        rig_close(rig);
        rig_cleanup(rig);
        rig = nullptr;
    }

    emit rigDisconnected();
}

void Rig::__openRig()
{
    FCT_IDENTIFICATION;

    // if rig is active then close it
    __closeRig();

    RigProfile newRigProfile = RigProfilesManager::instance()->getCurProfile1();

    qCDebug(runtime) << "Opening profile name: " << newRigProfile.profileName;

    rig = rig_init(newRigProfile.model);

    if (!rig)
    {
        // initialization failed
        emit rigErrorPresent(tr("Initialization Error"),
                             QString());
        return;
    }

    rig_set_debug(RIG_DEBUG_BUG);

    if ( isNetworkRig(rig->caps) )
    {
        //handling Network Radio
        strncpy(rig->state.rigport.pathname, newRigProfile.hostname.toLocal8Bit().constData(), HAMLIB_FILPATHLEN - 1);
        //port is hardcoded in hamlib - not necessary to set it.
    }
    else
    {
        //handling Serial Port Radio
        strncpy(rig->state.rigport.pathname, newRigProfile.portPath.toLocal8Bit().constData(), HAMLIB_FILPATHLEN - 1);
        rig->state.rigport.parm.serial.rate = newRigProfile.baudrate;
        rig->state.rigport.parm.serial.data_bits = newRigProfile.databits;
        rig->state.rigport.parm.serial.stop_bits = newRigProfile.stopbits;
        rig->state.rigport.parm.serial.handshake = stringToFlowControl(newRigProfile.flowcontrol);
        rig->state.rigport.parm.serial.parity = stringToParity(newRigProfile.parity);
    }

    int status = rig_open(rig);

    if (status != RIG_OK)
    {
        __closeRig();
        emit rigErrorPresent(tr("Open Connection Error"),
                             hamlibErrorString(status));
        return;
    }

    connectedRigProfile = newRigProfile;

    LoA.setRXOffset(MHz(connectedRigProfile.ritOffset));
    LoA.setTXOffset(MHz(connectedRigProfile.xitOffset));

    emit rigConnected();
}

rmode_t Rig::modeSubmodeToModeT(const QString &mode, const QString &submode)
{
    FCT_IDENTIFICATION;

    if ( mode == "SSB" )
    {
        if ( submode == "LSB" ) return RIG_MODE_LSB;
        else return RIG_MODE_USB;
    }
    else if ( mode == "CW" )
    {
        return RIG_MODE_CW;
    }
    else if ( mode == "AM" )
    {
        return RIG_MODE_AM;
    }
    else if ( mode == "FM" )
    {
        return RIG_MODE_FM;
    }
    else if ( mode == "RTTY")
    {
        return RIG_MODE_RTTY;
    }

    return RIG_MODE_NONE;

}

QString Rig::hamlibErrorString(int errorCode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << errorCode;

    QStringList errorList = QString(rigerror(errorCode)).split(QRegExp("[\r\n]"));
    QString ret;

    if ( errorList.size() >= 1 )
    {
        ret = errorList.at(0);
    }

    qCDebug(runtime) << ret;

    return ret;
}

void Rig::setFrequency(double newFreq)
{
    FCT_IDENTIFICATION;

    if ( newFreq > 0.0 )
    {
        QMetaObject::invokeMethod(this, "setFrequencyImpl", Qt::QueuedConnection,
                                  Q_ARG(double,newFreq));
    }
}

void Rig::setFrequencyImpl(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newFreq;

    if (!rig || !connectedRigProfile.getFreqInfo) return;

    rigLock.lock();

    if ( newFreq != LoA.getFreq() )
    {
        int status = rig_set_freq(rig, RIG_VFO_CURR, newFreq);

        if ( status != RIG_OK )
        {
            __closeRig();
            emit rigErrorPresent(tr("Set Frequency Error"),
                                 hamlibErrorString(status));
        }

        /* It is not needed to call VFO set freq function here because Rig's Update function do it */

        // wait a moment because Rigs are slow and they are not possible to set and get
        // mode so quickly (get mode is called in the main thread's update() function
#ifdef Q_OS_WIN
        Sleep(100);
#else
        usleep(100000);
#endif

    }
    rigLock.unlock();
}

void Rig::setMode(const QString &newMode, const QString &newSubMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newMode << " " << newSubMode;

    setMode(modeSubmodeToModeT(newMode, newSubMode));
}

void Rig::setMode(const QString & newMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newMode;

    setMode(rig_parse_mode(newMode.toLatin1()));
}

void Rig::setMode(rmode_t newModeIDe)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setModeImpl", Qt::QueuedConnection,
                              Q_ARG(rmode_t,newModeIDe));
}

void Rig::setPTT(bool active)
{
    FCT_IDENTIFICATION;
    QMetaObject::invokeMethod(this, "setPTTImpl", Qt::QueuedConnection,
                              Q_ARG(bool, active));

}

void Rig::setModeImpl(rmode_t newModeID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<newModeID;

    if (!rig || !connectedRigProfile.getModeInfo) return;

    rigLock.lock();

    if ( newModeID != RIG_MODE_NONE
         && newModeID != LoA.getMode() )
    {

        int status = rig_set_mode(rig, RIG_VFO_CURR, newModeID, RIG_PASSBAND_NOCHANGE);

        if (status != RIG_OK)
        {
            /* Ignore Error */
            /* not all rigs have correctly implemented Set Mode */
        /*
        __closeRig();
        emit rigErrorPresent(QString(tr("Set Mode Error - ")) + QString(rigerror(status)));
        */
        }

        /* It is not needed to call setMode here because Update function do it */

        // wait a moment because Rigs are slow and they are not possible to set and get
        // mode so quickly (get mode is called in the main thread's update() function
#ifdef Q_OS_WIN
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    rigLock.unlock();
}

void Rig::setPTTImpl(bool active)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << active;

    if (!rig || !connectedRigProfile.getPTTInfo) return;

    rigLock.lock();

    int status = rig_set_ptt(rig, RIG_VFO_CURR, (active ? RIG_PTT_ON : RIG_PTT_OFF));

    if ( status != RIG_OK )
    {
        __closeRig();
        emit rigErrorPresent(tr("Set PTT Error"),
                             hamlibErrorString(status));
    }

    /* It is not needed to call VFO set freq function here because Rig's Update function do it */

    // wait a moment because Rigs are slow and they are not possible to set and get
    // mode so quickly (get mode is called in the main thread's update() function
#ifdef Q_OS_WIN
    Sleep(100);
#else
    usleep(100000);
#endif
    rigLock.unlock();
}

void Rig::stopTimerImplt()
{
    FCT_IDENTIFICATION;

    if ( timer )
    {
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }
}

QStringList Rig::getAvailableModes()
{
    FCT_IDENTIFICATION;

    QStringList modeList;

    RigProfile newRigProfile = RigProfilesManager::instance()->getCurProfile1();

    RIG *localRig = rig_init(newRigProfile.model);

    if ( localRig )
    {
        rmode_t localRigModes = RIG_MODE_NONE;

        if ( localRig->state.mode_list != RIG_MODE_NONE )
        {
            localRigModes = static_cast<rmode_t>(localRig->state.mode_list);
        }
        else
        {
            if ( isNetworkRig(localRig->caps) )
            {
                /* Network rig has no mode */
                /* Add some modes */
                localRigModes = static_cast<rmode_t>(RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_AM);
            }
        }

        /* hamlib 3.x and 4.x are very different - workaround */
        for (unsigned char i = 0; i < (sizeof(rmode_t)*8)-1; i++ )
        {
            /* hamlib 3.x and 4.x are very different - workaround */
            const char *ms = rig_strrmode(static_cast<rmode_t>(localRigModes & rig_idx2setting(i)));

            if (!ms || !ms[0])
            {
                continue;
            }
            qCDebug(runtime) << "Supported Mode :" << ms;

            modeList.append(QString(ms));
        }

        rig_cleanup(localRig);
    }

    return modeList;
}

QString Rig::getModeFromRigMode(const QString &rigMode)
{
    FCT_IDENTIFICATION;

    rmode_t rmodet;

    rmodet = rig_parse_mode(rigMode.toLocal8Bit().data());

    if ( rmodet == RIG_MODE_NONE )
    {
        return QString();
    }
    QString submode;

    return getModeNormalizedText(rmodet, submode);
}

Rig::Rig(QObject *parent) :
    SerialPort(parent),
    LoA(VFO1, this),
    timer(nullptr)
{
    FCT_IDENTIFICATION;

    rig = nullptr;
    rig_set_debug(RIG_DEBUG_BUG);
    rig_load_all_backends();
}

Rig::~Rig()
{
    FCT_IDENTIFICATION;
}

LocalOscilator::LocalOscilator(VFOID id, QObject *parent) :
    QObject(parent),
    freq(RIG_FREQ_NONE),
    mode(RIG_MODE_NONE),
    vfo(RIG_VFO_NONE),
    ptt(false),
    power(0),
    RXOffset(0.0),
    TXOffset(0.0),
    ID(id)
{}

freq_t LocalOscilator::getFreq() const
{
    return freq;
}

void LocalOscilator::setFreq(const freq_t &value)
{
    freq = value;
}

rmode_t LocalOscilator::getMode() const
{
    return mode;
}

QString LocalOscilator::getModeText() const
{
    const char *rawMode = rig_strrmode(getMode());

    return QString(rawMode);
}

QString LocalOscilator::getModeNormalizedText(QString &submode) const
{
    return Rig::getModeNormalizedText(getMode(), submode);
}

void LocalOscilator::setMode(const rmode_t &value)
{
    mode = value;
}

vfo_t LocalOscilator::getVFO() const
{
    return vfo;
}

void LocalOscilator::setVFO(const vfo_t &value)
{
    vfo = value;
}

QString LocalOscilator::getVFOText() const
{
    const char *rawVFO = rig_strvfo(getVFO());
    return QString(rawVFO);
}

unsigned int LocalOscilator::getPower() const
{
    return power;
}

void LocalOscilator::setPower(unsigned int value)
{
    power = value;
}

VFOID LocalOscilator::getID() const
{
    return ID;
}

void LocalOscilator::setID(VFOID value)
{
    ID = value;
}

double LocalOscilator::getRXOffset() const
{
    return RXOffset;
}

void LocalOscilator::setRXOffset(double value)
{
    RXOffset = value;
}

double LocalOscilator::getTXOffset() const
{
    return TXOffset;
}

void LocalOscilator::setTXOffset(double value)
{
    TXOffset = value;
}

double LocalOscilator::getRITFreq() const
{
    return getFreq() + getRXOffset();
}

double LocalOscilator::getXITFreq() const
{
    return getFreq() + getTXOffset();
}

bool LocalOscilator::getPTT() const
{
    return ptt;
}

void LocalOscilator::setPTT(bool newPTT)
{
    ptt = newPTT;
}

void LocalOscilator::clear()
{
    setFreq(RIG_FREQ_NONE),
    setMode(RIG_MODE_NONE),
    setVFO(RIG_VFO_NONE),
    setVFO(0.0);
    setRXOffset(0.0);
    setTXOffset(0.0);
    setPower(0.0);
    setPTT(false);
}

