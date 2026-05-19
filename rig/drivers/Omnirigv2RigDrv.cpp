// This module is compiled only under Windows - therefore no ifdef related to Windows is needed

#define NOMINMAX
#include <windows.h>
#include <ocidl.h>
#include <oleauto.h>
#include <comdef.h>

#include <QTimer>
#include <QThread>
#include <QMutexLocker>

#include "Omnirigv2RigDrv.h"
#include "core/debug.h"
#include "rig/macros.h"

#if 0
#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Drv mutex"; \
                        QMutexLocker locker(&drvLock); \
                        qCDebug(runtime) << "Using Drv"
#else
#define MUTEXLOCKER     qCDebug(runtime) << "Mutex-free";
#endif

MODULE_IDENTIFICATION("qlog.rig.driver.omnirigv2drv");

// import omnirigv2
#import "C:\\Program Files (x86)\\Omni-Rig V2\\omnirig2.exe" raw_interfaces_only named_guids rename_namespace("OmnirigV2")
#include "OmniRigEventSink.h"

static QString bstrToQString(BSTR b)
{
    if ( !b ) return QString();
    QString s = QString::fromWCharArray(b, SysStringLen(b));
    SysFreeString(b);
    return s;
}

using OmniRigEventSinkV2 = OmniRigEventSink<OmnirigV2RigDrv, OmnirigV2::IOmniRigXEvents>;

QList<QPair<int, QString> > OmnirigV2RigDrv::getModelList()
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    ret << QPair<int, QString>(1, tr("Rig 1"))
        << QPair<int, QString>(2, tr("Rig 2"))
        << QPair<int, QString>(3, tr("Rig 3"))
        << QPair<int, QString>(4, tr("Rig 4"));

    return ret;
}

RigCaps OmnirigV2RigDrv::getCaps(int)
{
    FCT_IDENTIFICATION;

    RigCaps ret;

    ret.isNetworkOnly = true;

    ret.canGetFreq = true;
    ret.canGetMode = true;
    ret.canGetVFO = true;
    //ret.canGetRIT = true; // temporary disabled because there is not rig with the implemented RitOffset
    //XIT is not supported by Omnirig lib now
    ret.canGetPTT = true;
    ret.canGetSplit = true;

    return ret;
}

OmnirigV2RigDrv::OmnirigV2RigDrv(const RigProfile &profile,
                           QObject *parent)
    : GenericRigDrv(profile, parent),
      currFreq(0),
      currTxFreq(0),
      currRIT(0),
      currXIT(0),
      currPTT(false),
      currSplitEnabled(false),
      currModeID(),
      currVFO(),
      omniInterface(nullptr),
      rig(nullptr),
      eventSink(nullptr),
      connPoint(nullptr),
      connCookie(0),
      readableParams(0),
      writableParams(0),
      FREQMASK(OmnirigV2::PM_FREQA | OmnirigV2::PM_FREQB | OmnirigV2::PM_FREQ),
      VFO_A_MASK(OmnirigV2::PM_VFOA | OmnirigV2::PM_VFOAA | OmnirigV2::PM_VFOAB),
      VFO_B_MASK(OmnirigV2::PM_VFOB | OmnirigV2::PM_VFOBA | OmnirigV2::PM_VFOBB),
      VFO_SPEC_MASK(OmnirigV2::PM_VFOEQUAL | OmnirigV2::PM_VFOSWAP),
      ALLVFOsMASK(VFO_A_MASK | VFO_B_MASK | VFO_SPEC_MASK),
      SPLIT_MASK(OmnirigV2::PM_SPLITON | OmnirigV2::PM_SPLITOFF)
{
    FCT_IDENTIFICATION;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if ( FAILED(hr) )
        qCWarning(runtime) << "CoInitializeEx failed, hr =" << QString::number(hr, 16);

    modeMap.insert(OmnirigV2::PM_CW_U,  "CWR");
    modeMap.insert(OmnirigV2::PM_CW_L,  "CW");
    modeMap.insert(OmnirigV2::PM_SSB_U, "USB");
    modeMap.insert(OmnirigV2::PM_SSB_L, "LSB");
    modeMap.insert(OmnirigV2::PM_DIG_U, "DIG_U");
    modeMap.insert(OmnirigV2::PM_DIG_L, "DIG_L");
    modeMap.insert(OmnirigV2::PM_AM,    "AM");
    modeMap.insert(OmnirigV2::PM_FM,    "FM");

    // Timer
    offlineTimer.setSingleShot(true);
    QObject::connect(&offlineTimer, &QTimer::timeout, this, [this]()
    {
        qCDebug(runtime) << "Offline timer exceeded";
        emitDisconnect();
    });

    hr = CoCreateInstance(__uuidof(OmnirigV2::OmniRigX),
                          nullptr,
                          CLSCTX_LOCAL_SERVER,
                          __uuidof(OmnirigV2::IOmniRigX),
                          reinterpret_cast<void **>(&omniInterface));

    if ( FAILED(hr) || !omniInterface )
    {
        qCWarning(runtime) << "CoCreateInstance(OmniRig.OmniRigX) failed, hr =" << QString::number(hr, 16);
        lastErrorText = tr("Initialization Error");
        omniInterface = nullptr;
        return;
    }

    // Event sink
    eventSink = new OmniRigEventSinkV2(this);

    IConnectionPointContainer *cpc = nullptr;
    hr = omniInterface->QueryInterface(IID_IConnectionPointContainer, reinterpret_cast<void **>(&cpc));

    qCDebug(runtime) << "QI(IConnectionPointContainer) hr =" << QString::number(hr, 16);

    if ( SUCCEEDED(hr) && cpc )
    {
        hr = cpc->FindConnectionPoint(__uuidof(OmnirigV2::IOmniRigXEvents), &connPoint);
        qCDebug(runtime) << "FindConnectionPoint(IOmniRigXEvents) hr =" << QString::number(hr, 16);

        if (SUCCEEDED(hr) && connPoint)
        {
            hr = connPoint->Advise(eventSink, &connCookie);

            qCDebug(runtime) << "Advise(IOmniRigXEvents) hr =" << QString::number(hr, 16)
                             << "cookie =" << connCookie;

            if ( FAILED(hr) )
            {
                qCWarning(runtime) << "IConnectionPoint::Advise failed";
                connPoint->Release();
                connPoint = nullptr;
                connCookie = 0;
            }
        }
        cpc->Release();
    }
}

OmnirigV2RigDrv::~OmnirigV2RigDrv()
{
    FCT_IDENTIFICATION;

    if ( !drvLock.tryLock(200) )
    {
        qCDebug(runtime) << "Waited too long";
        // better to make a memory leak
        CoUninitialize();
        return;
    }

    if ( connPoint && connCookie )
    {
        connPoint->Unadvise(connCookie);
        connCookie = 0;
    }

    if ( connPoint )
    {
        connPoint->Release();
        connPoint = nullptr;
    }

    if ( eventSink )
    {
        eventSink->Release();
        eventSink = nullptr;
    }

    if ( rig )
    {
        rig->Release();
        rig = nullptr;
    }

    if ( omniInterface )
    {
        omniInterface->Release();
        omniInterface = nullptr;
    }

    CoUninitialize();

    drvLock.unlock();
}

bool OmnirigV2RigDrv::open()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( !omniInterface )
    {
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Rig is not initialized";
        return false;
    }

    long ifaceVer = 0;
    long swVer    = 0;

    omniInterface->get_InterfaceVersion(&ifaceVer);
    omniInterface->get_SoftwareVersion(&swVer);

    quint16 swMajor = static_cast<quint16>(swVer >> 16);
    quint16 swMinor = static_cast<quint16>(swVer & 0xFFFF);
    quint16 ifMajor = static_cast<quint16>(ifaceVer >> 8);
    quint16 ifMinor = static_cast<quint16>(ifaceVer & 0xFF);

    qCDebug(runtime) << "Omnirig Version"
                     << swMajor << "." << swMinor
                     << "Interface Version"
                     << ifMajor << "." << ifMinor;

    if ( rig )
    {
        rig->Release();
        rig = nullptr;
    }

    HRESULT hr = E_FAIL;

    switch (rigProfile.model)
    {
    case 1: hr = omniInterface->get_Rig1(&rig); break;
    case 2: hr = omniInterface->get_Rig2(&rig); break;
    case 3: hr = omniInterface->get_Rig3(&rig); break;
    case 4: hr = omniInterface->get_Rig4(&rig); break;
    default:
        hr = E_INVALIDARG;
        break;
    }

    if ( FAILED(hr) || !rig )
    {
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Cannot get Rig Instance, hr =" << QString::number(hr, 16);
        return false;
    }

    __rigTypeChange(rigProfile.model);

    QTimer::singleShot(500, this, [this]()
    {
        this->rigStatusChange(rigProfile.model);
    });

    return true;
}

bool OmnirigV2RigDrv::isMorseOverCatSupported()
{
    FCT_IDENTIFICATION;
    return false;
}

QStringList OmnirigV2RigDrv::getAvailableModes()
{
    FCT_IDENTIFICATION;

    QStringList ret;

    for ( auto it = modeMap.constBegin(); it != modeMap.constEnd(); ++it )
        if ( it.key() & writableParams )
            ret.append(it.value());

    return ret;
}

void OmnirigV2RigDrv::setFrequency(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << QSTRING_FREQ(newFreq);

    if ( !rigProfile.getFreqInfo || !rig ) return;

    long internalFreq = static_cast<long>(newFreq);

    qCDebug(runtime) << "Received freq" << internalFreq << "current" << currFreq;

    if ( internalFreq == currFreq ) return;

    MUTEXLOCKER;

    OmnirigV2::RigParamX vfo = OmnirigV2::PM_UNKNOWN;

    rig->get_Vfo(&vfo);

    if ( vfo & VFO_B_MASK )
    {
        qCDebug(runtime) << "Setting VFO B Freq";
        rig->put_FreqB(internalFreq);
    }
    else if ( writableParams & OmnirigV2::PM_FREQA )
    {
        qCDebug(runtime) << "Setting VFO A Freq";
        rig->put_FreqA(internalFreq);
    }
    else
    {
        qCDebug(runtime) << "Setting Generic VFO Freq";
        rig->put_Freq(internalFreq);
    }

    commandSleep();
}

void OmnirigV2RigDrv::setFrequency(VFOID vfoid, double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << vfoid << QSTRING_FREQ(newFreq);

    if ( !rigProfile.getFreqInfo || !rig ) return;

    if ( vfoid == VFO1 )
    {
        setFrequency(newFreq);
        return;
    }

    // VFO2 — TX frequency (set the non-active VFO)
    long internalFreq = static_cast<long>(newFreq);

    MUTEXLOCKER;

    OmnirigV2::RigParamX vfo = OmnirigV2::PM_UNKNOWN;
    rig->get_Vfo(&vfo);

    if ( vfo & VFO_B_MASK )
    {
        qCDebug(runtime) << "Active VFO is B, setting TX freq to VFO A";
        rig->put_FreqA(internalFreq);
    }
    else
    {
        qCDebug(runtime) << "Active VFO is A, setting TX freq to VFO B";
        rig->put_FreqB(internalFreq);
    }

    commandSleep();
}

void OmnirigV2RigDrv::setSplit(bool enabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << enabled;

    if ( !rigProfile.getSplitInfo || !rig ) return;

    MUTEXLOCKER;

    rig->put_Split(enabled ? OmnirigV2::PM_SPLITON : OmnirigV2::PM_SPLITOFF);

    commandSleep();
}

void OmnirigV2RigDrv::setRawMode(const QString &rawMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    if ( !rigProfile.getModeInfo || !rig ) return;

    MUTEXLOCKER;

    const QList<int> mappedMode = modeMap.keys(rawMode);

    if (!mappedMode.isEmpty())
    {
        OmnirigV2::RigParamX m = static_cast<OmnirigV2::RigParamX>(mappedMode.at(0));
        qCDebug(runtime) << "Mode Found" << m;
        if ( m & writableParams )
        {
            qCDebug(runtime) << "Setting Mode";
            rig->put_Mode(m);
            commandSleep();
        }
    }
}

void OmnirigV2RigDrv::setMode(const QString &mode, const QString &submode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << submode << digiVariant;

    QString innerSubmode(submode);

    if ( digiVariant && !innerSubmode.isEmpty() )
    {
        const QString digMode = QLatin1String("DIG_") + innerSubmode.at(0);
        if (modeMap.key(digMode, 0) & writableParams)
            innerSubmode = digMode;
    }

    setRawMode((submode.isEmpty()) ? mode.toUpper() : innerSubmode.toUpper());
}

void OmnirigV2RigDrv::setPTT(bool newPTTState)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newPTTState;

    if ( !rigProfile.getPTTInfo || !rig ) return;

    MUTEXLOCKER;

    rig->put_Tx(newPTTState ? OmnirigV2::PM_TX : OmnirigV2::PM_RX);

    commandSleep();
}

void OmnirigV2RigDrv::setKeySpeed(qint16)
{
    FCT_IDENTIFICATION;
    //not implemented
}

void OmnirigV2RigDrv::syncKeySpeed(qint16)
{
    FCT_IDENTIFICATION;
    //not implemented
}

void OmnirigV2RigDrv::sendMorse(const QString &)
{
    FCT_IDENTIFICATION;
    //not implemented
}

void OmnirigV2RigDrv::stopMorse()
{
    FCT_IDENTIFICATION;
    //not implemented
}

void OmnirigV2RigDrv::sendState()
{
    FCT_IDENTIFICATION;
    MUTEXLOCKER;

    checkChanges(0, true);
}

void OmnirigV2RigDrv::stopTimers()
{
    FCT_IDENTIFICATION;

    offlineTimer.stop();
}

void OmnirigV2RigDrv::sendDXSpot(const DxSpot &)
{
    FCT_IDENTIFICATION;

    // no action
}

void OmnirigV2RigDrv::rigTypeChange(int rigID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig ID" << rigID;

    if ( rigID != rigProfile.model )
        return;

    MUTEXLOCKER;

    __rigTypeChange(rigID);
}

void OmnirigV2RigDrv::__rigTypeChange(int rigID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig ID" << rigID;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigID != rigProfile.model ) return;

    long r = 0;
    long w = 0;
    rig->get_ReadableParams(&r);
    rig->get_WriteableParams(&w);

    readableParams = static_cast<int>(r);
    writableParams = static_cast<int>(w);

    qCDebug(runtime) << "R-params" << QString::number(readableParams, 16)
                     << "W-params" << QString::number(writableParams, 16);
}

void OmnirigV2RigDrv::rigStatusChange(int rigID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig ID" << rigID;

    if ( rigID != rigProfile.model ) return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    OmnirigV2::RigStatusX st = OmnirigV2::ST_NOTCONFIGURED;
    rig->get_Status(&st);

    BSTR statusStr = nullptr;
    rig->get_StatusStr(&statusStr);
    QString qStatusStr = bstrToQString(statusStr);

    qCDebug(runtime) << "Rig ID " << rigID;
    qCDebug(runtime) << "New Status" << st << qStatusStr;

    if ( OmnirigV2::ST_ONLINE != st )
    {
        qCDebug(runtime) << "New status" << qStatusStr;
        if (!offlineTimer.isActive())
            offlineTimer.start(OFFLINETIMER_TIME_MS);
    }
    else
    {
        offlineTimer.stop();
        emit rigIsReady();
    }
}

void OmnirigV2RigDrv::rigParamsChange(int rigID, int params)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rigID << params;

    if (rigID != rigProfile.model)
        return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    checkChanges(params);
}

void OmnirigV2RigDrv::checkChanges(int params, bool force)
{
    FCT_IDENTIFICATION;

    checkSplitChange(params, force);
    checkFreqChange(params, force);
    checkModeChange(params, force);
    checkPTTChange(params, force);
    checkVFOChange(params, force);
    checkRITChange(params, force);
}

bool OmnirigV2RigDrv::checkFreqChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return false;
    }

    if ( !rigProfile.getFreqInfo ) return true;
    if ( !force && !( params & (FREQMASK | ALLVFOsMASK) ) ) return true;

    unsigned int vfo_freq = 0;
    OmnirigV2::RigParamX vfo = OmnirigV2::PM_UNKNOWN;
    rig->get_Vfo(&vfo);
    const bool vfoIsB = (vfo & VFO_B_MASK);

    long tmp = 0L;
    if ( vfoIsB )
    {
        qCDebug(runtime) << "Getting VFO B Freq";
        rig->get_FreqB(&tmp);
        if ( !tmp )
        {
            qCDebug(runtime) << "FreqB returned 0, falling back to Freq()";
            rig->get_Freq(&tmp);
        }
    }
    else
    {
        qCDebug(runtime) << "Getting VFO A Freq";
        rig->get_FreqA(&tmp);
        if ( !tmp )
        {
            qCDebug(runtime) << "FreqA returned 0, falling back to Freq()";
            rig->get_Freq(&tmp);
        }
    }

    vfo_freq = static_cast<unsigned int>(tmp);

    qCDebug(runtime) << "Rig Freq: "<< vfo_freq;
    qCDebug(runtime) << "Object Freq: "<< currFreq;

    if ( vfo_freq != currFreq || force )
    {
        currFreq = vfo_freq;
        qCDebug(runtime) << "emitting FREQ changed" << currFreq << Hz2MHz(currFreq);
        emit frequencyChanged(Hz2MHz(currFreq),
                              Hz2MHz(getRITFreq()),
                              Hz2MHz(getXITFreq()));
    }

    // TX frequency (the non-active VFO) when split is active
    if ( rigProfile.getSplitInfo && currSplitEnabled )
    {
        long txTmp = 0L;
        if ( vfoIsB )
            rig->get_FreqA(&txTmp);
        else
            rig->get_FreqB(&txTmp);

        unsigned int txFreq = static_cast<unsigned int>(txTmp);

        if ( txFreq != currTxFreq || force )
        {
            currTxFreq = txFreq;
            qCDebug(runtime) << "emitting TX FREQ changed" << currTxFreq << Hz2MHz(currTxFreq);
            emit txFrequencyChanged(Hz2MHz(currTxFreq));
        }
    }

    return true;
}

bool OmnirigV2RigDrv::checkModeChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return false;
    }

    if ( rigProfile.getModeInfo )
    {
        int inParams = params;
        if ( force )
        {
            OmnirigV2::RigParamX m = OmnirigV2::PM_UNKNOWN;
            rig->get_Mode(&m);
            inParams = m;
        }

        QMap<int, QString>::const_iterator it;
        for ( it = modeMap.begin(); it != modeMap.end(); ++it )
        {
            if ( inParams & it.key() )
            {
                qCDebug(runtime) << "Rig Mode: "<< it.value();
                qCDebug(runtime) << "Object Mode: "<< currModeID;

                if ( currModeID != it.value() || force)
                {
                    currModeID = it.value();

                    QString submode;
                    const QString mode = getModeNormalizedText(currModeID, submode);
                    qCDebug(runtime) << "emitting MODE changed"
                                     << currModeID << mode << submode << 0;
                    emit modeChanged(currModeID,
                                     mode, submode,
                                     0);
                }
                break;
            }
        }
    }
    return true;
}

void OmnirigV2RigDrv::checkPTTChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigProfile.getPTTInfo
         && (params & OmnirigV2::PM_RX
            || params & OmnirigV2::PM_TX
            || force) )
    {
        int inParams = params;
        if ( force )
        {
            OmnirigV2::RigParamX tx = OmnirigV2::PM_RX;
            rig->get_Tx(&tx);
            inParams = tx;
        }

        bool ptt = false;

        if ( inParams & OmnirigV2::PM_RX )
            ptt = false;

        if ( inParams & OmnirigV2::PM_TX )
            ptt = true;

        qCDebug(runtime) << "Rig PTT: "<< ptt;
        qCDebug(runtime) << "Object Mode: "<< currPTT;

        if ( ptt != currPTT || force )
        {
            currPTT = ptt;
            qCDebug(runtime) << "emitting PTT changed" << currPTT;
            emit pttChanged(currPTT);
        }
    }
}

void OmnirigV2RigDrv::checkVFOChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( !rigProfile.getVFOInfo ) return;

    if ( (params & ALLVFOsMASK) || force )
    {
        int inParams;
        if (force || ( params & VFO_SPEC_MASK ) )
        {
            OmnirigV2::RigParamX v;
            rig->get_Vfo(&v);
            inParams = v;
        }
        else
        {
            inParams = params;
        }

        QString vfo;

        if ( inParams & VFO_A_MASK ) vfo = "VFOA";
        if ( inParams & VFO_B_MASK ) vfo = "VFOB";

        qCDebug(runtime) << "Rig VFO: "<< vfo;
        qCDebug(runtime) << "Object VFO: "<< currVFO;

        if ( vfo != currVFO || force )
        {
            currVFO = vfo;
            qCDebug(runtime) << "emitting VFO changed" << currVFO;
            emit vfoChanged(currVFO);
        }
    }
}

void OmnirigV2RigDrv::checkRITChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if (rigProfile.getRITInfo
        && (params & OmnirigV2::PM_RITON
            || params & OmnirigV2::PM_RITOFF
            || force))
    {
        int inParams = params;
        if ( force )
        {
            OmnirigV2::RigParamX r;
            rig->get_Rit(&r);
            inParams = r;
        }

        long off = 0;
        rig->get_RitOffset(&off);
        unsigned int rit = (inParams & OmnirigV2::PM_RITON)
                               ? static_cast<unsigned int>(off)
                               : 0;

        qCDebug(runtime) << "Rig RIT: "<< rit;
        qCDebug(runtime) << "Object RIT: "<< currRIT;

        if ( rit != currRIT || force )
        {
            currRIT = rit;
            qCDebug(runtime) << "emitting RIT changed" << QSTRING_FREQ(Hz2MHz(currRIT));
            qCDebug(runtime) << "emitting FREQ changed "
                             << QSTRING_FREQ(Hz2MHz(currFreq))
                             << QSTRING_FREQ(Hz2MHz(getRITFreq()))
                             << QSTRING_FREQ(Hz2MHz(getXITFreq()));
            emit ritChanged(Hz2MHz(currRIT));
            emit frequencyChanged(Hz2MHz(currFreq),
                                  Hz2MHz(getRITFreq()),
                                  Hz2MHz(getXITFreq()));
        }
    }
}

void OmnirigV2RigDrv::checkSplitChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( !rigProfile.getSplitInfo )
    {
        qCDebug(runtime) << "Get SPLIT is disabled";
        return;
    }

    if ( (params & SPLIT_MASK) || force )
    {
        int inParams = params;
        if ( force )
        {
            OmnirigV2::RigParamX s;
            rig->get_Split(&s);
            inParams = s;
        }

        bool splitEnabled = ( inParams & OmnirigV2::PM_SPLITON ) != 0;

        qCDebug(runtime) << "Rig Split:" << splitEnabled;
        qCDebug(runtime) << "Object Split:" << currSplitEnabled;

        if ( splitEnabled != currSplitEnabled || force )
        {
            currSplitEnabled = splitEnabled;
            qCDebug(runtime) << "emitting SPLIT changed" << currSplitEnabled;
            emit splitChanged(currSplitEnabled);

            if ( !currSplitEnabled )
                currTxFreq = 0;
        }
    }
}

double OmnirigV2RigDrv::getRITFreq()
{
    FCT_IDENTIFICATION;
    return currFreq + currRIT;
}

void OmnirigV2RigDrv::setRITFreq(double rit)
{
    currRIT = static_cast<unsigned int>(rit);
}

double OmnirigV2RigDrv::getXITFreq()
{
    return currFreq + currXIT;
}

void OmnirigV2RigDrv::setXITFreq(double xit)
{
    currXIT = static_cast<unsigned int>(xit);
}

void OmnirigV2RigDrv::emitDisconnect()
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    emit errorOccurred(tr("Rig status changed"),
                       tr("Rig is not connected"));
}

void OmnirigV2RigDrv::commandSleep()
{
    QThread::msleep(200);
}

const QString OmnirigV2RigDrv::getModeNormalizedText(const QString &rawMode,
                                                     QString &submode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    submode = QString();

    if (rawMode.contains(QStringLiteral("CW")))
        return QStringLiteral("CW");

    if (rawMode == QStringLiteral("USB")) {
        submode = QStringLiteral("USB");
        return QStringLiteral("SSB");
    }

    if (rawMode == QStringLiteral("LSB")) {
        submode = QStringLiteral("LSB");
        return QStringLiteral("SSB");
    }

    if (rawMode == QStringLiteral("AM"))
        return QStringLiteral("AM");

    if (rawMode == QStringLiteral("FM"))
        return QStringLiteral("FM");

    // maybe bad maybe good
    if (rawMode == QStringLiteral("DIG_U")) {
        submode = QStringLiteral("USB");
        return QStringLiteral("SSB");
    }

    // maybe bad maybe good
    if (rawMode == QStringLiteral("DIG_L")) {
        submode = QStringLiteral("LSB");
        return QStringLiteral("SSB");
    }

    return QString();
}
