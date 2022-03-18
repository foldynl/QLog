#include <QDebug>
#include <QSettings>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Rig.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.rig");

#ifndef HAMLIB_FILPATHLEN
#define HAMLIB_FILPATHLEN FILPATHLEN
#endif

#define POLL_INTERVAL 2000

static QString modeToString(rmode_t mode, QString &submode) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<mode << " " << submode;

    switch (mode) {
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
    default : return "";
    }
}

static rmode_t stringToMode(const QString &mode, const QString &submode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<mode << " " << submode;

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

static enum serial_handshake_e stringToFlowControl(const QString &in_flowcontrol)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<in_flowcontrol;

    QString flowcontrol = in_flowcontrol.toLower();

    if ( flowcontrol == "software" ) return RIG_HANDSHAKE_XONXOFF;
    if ( flowcontrol == "hardware" ) return RIG_HANDSHAKE_HARDWARE;
    return RIG_HANDSHAKE_NONE;
}

static enum serial_parity_e stringToParity(const QString &in_parity)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<in_parity;

    QString parity = in_parity.toLower();

    if ( parity == "even" ) return RIG_PARITY_EVEN;
    if ( parity == "odd" ) return RIG_PARITY_ODD;
    if ( parity == "mark" ) return RIG_PARITY_MARK;
    if ( parity == "space" ) return RIG_PARITY_SPACE;

    return RIG_PARITY_NONE;
}

Rig* Rig::instance() {
    FCT_IDENTIFICATION;

    static Rig instance;
    return &instance;
}

void Rig::start() {
    FCT_IDENTIFICATION;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(POLL_INTERVAL);
}

void Rig::update() {
    FCT_IDENTIFICATION;

    int status = RIG_OK;

    if (!rig) return;

    if (!rigLock.tryLock(POLL_INTERVAL)) return;

    freq_t vfo_freq;

    status = rig_get_freq(rig, RIG_VFO_CURR, &vfo_freq);

    if ( status == RIG_OK )
    {
        int new_freq = static_cast<int>(vfo_freq);

        if (new_freq != freq_rx) {
            freq_rx = new_freq;
            emit frequencyChanged(freq_rx/1e6);
        }
    }
    else
    {
        __closeRig();
         emit rigErrorPresent(QString(tr("Get Frequency Error - ")) + QString(rigerror(status)));
        timer->start(POLL_INTERVAL);
        rigLock.unlock();
        return;
    }

    pbwidth_t pbwidth;
    rmode_t curr_modeId;

    status = rig_get_mode(rig, RIG_VFO_CURR, &curr_modeId, &pbwidth);

    if ( status == RIG_OK )
    {
        if ( curr_modeId != modeId )
        {
           // mode change
           QString mode;
           QString submode;
           modeId = curr_modeId;
           mode = modeToString(curr_modeId, submode);
           emit modeChanged(mode, submode);
        }
    }
    else
    {
        __closeRig();
        emit rigErrorPresent(QString(tr("Get Mode Error - ")) + QString(rigerror(status)));
        timer->start(POLL_INTERVAL);
        rigLock.unlock();
        return;
    }

    value_t rigPowerLevel;
    unsigned int rigPower;

    status = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &rigPowerLevel);

    if ( status == RIG_OK )
    {

        status = rig_power2mW(rig, &rigPower, rigPowerLevel.f, freq_rx, modeId);

        if (  status == RIG_OK )
        {
            if (rigPower != power) {
                power = rigPower;
                emit powerChanged(power/1000.0);
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
    timer->start(POLL_INTERVAL);
    rigLock.unlock();
}

void Rig::open() {
    FCT_IDENTIFICATION;

    QSettings settings;
    int model = settings.value("hamlib/rig/model").toInt();
    int baudrate = settings.value("hamlib/rig/baudrate").toInt();
    int databits = settings.value("hamlib/rig/databits").toInt();
    float stopbits = settings.value("hamlib/rig/stopbits").toFloat();
    QString flowControl = settings.value("hamlib/rig/stopbits").toString();
    QString parity = settings.value("hamlib/rig/parity").toString();
    QByteArray portStr = settings.value("hamlib/rig/port").toByteArray();
    QString hostname = settings.value("hamlib/rig/hostname").toString();
    int netport = settings.value("hamlib/rig/netport").toInt();

    const char* port = portStr.constData();

    qCDebug(runtime) << portStr;
    rigLock.lock();

    // if rig is active then close it
    __closeRig();

    rig = rig_init(model);
    if (!rig)
    {
        // initialization failed
        emit rigErrorPresent(QString(tr("Initialization Error")));
        rigLock.unlock();
        return;
    }
    rig_set_debug(RIG_DEBUG_ERR);

    if (rig->caps->port_type == RIG_PORT_NETWORK
        || rig->caps->port_type == RIG_PORT_UDP_NETWORK )
    {
        //handling Network Radio
        strncpy(rig->state.rigport.pathname, hostname.toLocal8Bit().constData(), HAMLIB_FILPATHLEN - 1);
        //port is hardcoded in hamlib - not necessary to set it.
        (void)netport;

    }
    else
    {
        //handling Serial Port Radio
        strncpy(rig->state.rigport.pathname, port, HAMLIB_FILPATHLEN - 1);
        rig->state.rigport.parm.serial.rate = baudrate;
        rig->state.rigport.parm.serial.data_bits = databits;
        rig->state.rigport.parm.serial.stop_bits = stopbits;
        rig->state.rigport.parm.serial.handshake = stringToFlowControl(flowControl);
        rig->state.rigport.parm.serial.parity = stringToParity(parity);
    }

    int status = rig_open(rig);

    if (status != RIG_OK)
    {
        __closeRig();
        emit rigErrorPresent(QString(tr("Open Connection Error - ")) + QString(rigerror(status)));
    }

    rigLock.unlock();
}

void Rig::__closeRig()
{
    FCT_IDENTIFICATION;

    if ( rig )
    {
        rig_close(rig);
        rig_cleanup(rig);
        rig = nullptr;
    }
}
void Rig::close()
{
    FCT_IDENTIFICATION;

    rigLock.lock();
    __closeRig();
    rigLock.unlock();
}

void Rig::setFrequency(double newFreq) {
    FCT_IDENTIFICATION;
    qCDebug(function_parameters)<<newFreq;

    if (!rig) return;

    rigLock.lock();
    freq_rx = static_cast<int>(newFreq*1e6);
    int status = rig_set_freq(rig, RIG_VFO_CURR, freq_rx);

    if ( status != RIG_OK )
    {
        __closeRig();
        emit rigErrorPresent(QString(tr("Set Frequency Error - ")) + QString(rigerror(status)));
    }

    rigLock.unlock();
}

void Rig::setMode(const QString &newMode, const QString &newSubMode)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters)<<newMode << " " << newSubMode;

    if (!rig) return;

    rigLock.lock();

    rmode_t new_modeId = stringToMode(newMode, newSubMode);

    if ( new_modeId != RIG_MODE_NONE )
    {
        int status = rig_set_mode(rig, RIG_VFO_CURR, new_modeId, RIG_PASSBAND_NOCHANGE);

        if (status != RIG_OK)
        {
            /* Ignore Error */
            /*
        __closeRig();
        emit rigErrorPresent(QString(tr("Set Mode Error - ")) + QString(rigerror(status)));
        */
        }

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

void Rig::setPower(double newPower) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<newPower;

    if (!rig) return;
    power = (int)(newPower*1000);
}

Rig::Rig(QObject *parent) :
    QObject(parent),
    timer(nullptr)
{
    FCT_IDENTIFICATION;

    rig = nullptr;
    freq_rx = 0;
    power = 0;
}

Rig::~Rig()
{
    if ( timer )
    {
        timer->stop();
        timer->deleteLater();
    }
}
