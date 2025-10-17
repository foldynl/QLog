
// This module is compiled only under Windows - therefore no ifdef related to Windows is needed

#include <Combaseapi.h>

#include <QTimer>
#include "OmnirigRigDrv.h"
#include "core/debug.h"
#include "rig/macros.h"

#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Drv mutex"; \
                        QMutexLocker locker(&drvLock); \
                        qCDebug(runtime) << "Using Drv"

MODULE_IDENTIFICATION("qlog.rig.driver.omnirigdrv");

QList<QPair<int, QString> > OmnirigRigDrv::getModelList()
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    ret << QPair<int, QString>(1, tr("Rig 1"))
        << QPair<int, QString>(2, tr("Rig 2"));

    return ret;
}

RigCaps OmnirigRigDrv::getCaps(int)
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

    return ret;
}

OmnirigRigDrv::OmnirigRigDrv(const RigProfile &profile,
                       QObject *parent)
    : GenericRigDrv(profile, parent),
      currFreq(0),
      currRIT(0),
      currXIT(0),
      currPTT(false),
      omniRigInterface(nullptr),
      rig(nullptr),
      readableParams(0),
      writableParams(0)
{
    FCT_IDENTIFICATION;

    CoInitializeEx(nullptr, 0);

    omniRigInterface = new OmniRig::OmniRigX(this);

    if ( !omniRigInterface )
    {
        //initialization failed
        qCDebug(runtime) << "Cannot allocate Omnirig structure";
        lastErrorText = tr("Initialization Error");
    }
}

OmnirigRigDrv::~OmnirigRigDrv()
{
    FCT_IDENTIFICATION;

    if ( !drvLock.tryLock(200) )
    {
        qCDebug(runtime) << "Waited too long";
        // better to make a memory leak
        CoUninitialize();
        return;
    }

    if ( rig )
    {
        delete rig;
        rig = nullptr;
    }

    if ( omniRigInterface )
    {
        delete omniRigInterface;
        omniRigInterface = nullptr;
    }

    CoUninitialize();

    drvLock.unlock();
}

bool OmnirigRigDrv::open()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( !omniRigInterface )
    {
        // initialization failed
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Rig is not initialized";
        return false;
    }

    connect(omniRigInterface, &OmniRig::OmniRigX::exception,
            this, &OmnirigRigDrv::COMException);

    connect(omniRigInterface, SIGNAL(RigTypeChange(int)),
            this, SLOT(rigTypeChange(int)));
    connect(omniRigInterface, SIGNAL(StatusChange(int)),
            this, SLOT(rigStatusChange(int)));
    connect(omniRigInterface, SIGNAL(ParamsChange(int, int)),
            this, SLOT(rigParamsChange(int, int)));

    qCDebug(runtime) << "Omnirig Version" << static_cast<quint16> (omniRigInterface->SoftwareVersion () >> 16)
                     << "." << static_cast<quint16> (omniRigInterface->SoftwareVersion () & 0xffff)
                     << "Interface Version" << static_cast<int> (omniRigInterface->InterfaceVersion () >> 8 & 0xff)
                     << "." << static_cast<int> (omniRigInterface->InterfaceVersion () >> 8 & 0xff);

    OmniRig::IRigX* rigInterface = getRigPtr();

    if ( !rigInterface )
    {
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Cannot get Rig Instance";
        return false;
    }

    rig = new OmniRig::RigX(rigInterface);

    if ( !rig )
    {
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Cannot allocate Rig Interface";
        return false;
    }

    __rigTypeChange(rigProfile.model);

    QTimer::singleShot(300, this, [this]()
    {
        OmnirigRigDrv::rigStatusChange(rigProfile.model);
    });

    emit rigIsReady();

    // TODO - solve timeout from library. Is it possible????
    return true;
}

bool OmnirigRigDrv::isMorseOverCatSupported()
{
    FCT_IDENTIFICATION;

    return false;
}

QStringList OmnirigRigDrv::getAvailableModes()
{
    FCT_IDENTIFICATION;

    QStringList ret;

    for ( auto it = modeMap.constBegin(); it != modeMap.constEnd(); ++it )
        if ( it.key() & writableParams )
            ret.append(it.value());

    return ret;
}

void OmnirigRigDrv::setFrequency(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << QSTRING_FREQ(newFreq);

    if ( !rigProfile.getFreqInfo )
        return;

    unsigned int internalFreq = static_cast<unsigned int>(newFreq);

    qCDebug(runtime) << "Received freq" << internalFreq << "current" << currFreq;

    if ( internalFreq == currFreq )
        return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rig->Vfo() == OmniRig::PM_VFOB
         || rig->Vfo() == OmniRig::PM_VFOBB
         || rig->Vfo() == OmniRig::PM_VFOBA )
    {
        qCDebug(runtime) << "Setting VFO B Freq";
        rig->SetFreqB(internalFreq);
    }
    else if ( (writableParams & OmniRig::PM_FREQA) )
    {
        qCDebug(runtime) << "Setting VFO A Freq";
        rig->SetFreqA(internalFreq);
    }
    else
    {
        qCDebug(runtime) << "Setting Generic VFO Freq";
        rig->SetFreq(internalFreq);
    }

    commandSleep();
}

void OmnirigRigDrv::setRawMode(const QString &rawMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    if ( !rigProfile.getModeInfo )
        return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    const QList<OmniRig::RigParamX> mappedMode = modeMap.keys(rawMode);

    if ( mappedMode.size() > 0 )
    {
        OmniRig::RigParamX rawMode = mappedMode.at(0);
        qCDebug(runtime) << "Mode Found" << rawMode;
        if ( rawMode & writableParams )
        {
            qCDebug(runtime) << "Setting Mode";
            rig->SetMode(rawMode);
            commandSleep();
        }
    }
}

void OmnirigRigDrv::setMode(const QString &mode, const QString &submode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << submode << digiVariant;

    QString innerSubmode(submode);

    if ( digiVariant )
    {
        const QString digMode = QLatin1String("DIG_") + innerSubmode.at(0);
        if ( modeMap.key(digMode) & writableParams )
            innerSubmode = digMode;
    }

    setRawMode((submode.isEmpty()) ? mode.toUpper() : innerSubmode.toUpper());
}

void OmnirigRigDrv::setPTT(bool newPTTSTate)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newPTTSTate;

    if ( !rigProfile.getPTTInfo )
        return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    rig->SetTx((newPTTSTate) ? OmniRig::PM_TX : OmniRig::PM_RX);

    commandSleep();
}

void OmnirigRigDrv::setKeySpeed(qint16)
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigRigDrv::syncKeySpeed(qint16)
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigRigDrv::sendMorse(const QString &)
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigRigDrv::stopMorse()
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigRigDrv::sendState()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    checkChanges(0, true);
}

void OmnirigRigDrv::stopTimers()
{
    FCT_IDENTIFICATION;

    // not timer
    return;
}

void OmnirigRigDrv::sendDXSpot(const DxSpot &)
{
    FCT_IDENTIFICATION;

    //no action
}

void OmnirigRigDrv::__rigTypeChange(int rigID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig ID" << rigID;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    qCDebug(runtime) << "Rig ID" << rigID << "Changed";

    if ( rigID != rigProfile.model )
        return;

    readableParams = rig->ReadableParams();
    writableParams = rig->WriteableParams();

    qCDebug(runtime) << "R-params" << QString::number(readableParams, 16)
                     << "W-params" << QString::number(writableParams, 16);
}

void OmnirigRigDrv::commandSleep()
{
    QThread::msleep(200);
}

const QString OmnirigRigDrv::getModeNormalizedText(const QString &rawMode, QString &submode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    submode = QString();

    if ( rawMode.contains("CW") )
        return "CW";

    if ( rawMode == "USB" )
    {
        submode = "USB";
        return "SSB";
    }

    if ( rawMode == "LSB" )
    {
        submode = "LSB";
        return "SSB";
    }

    if ( rawMode == "AM" )
        return "AM";

    if ( rawMode == "FM" )
        return "FM";

    // maybe bad maybe good
    if ( rawMode == "DIG_U" )
    {
        submode = "USB";
        return "SSB";
    }

    // maybe bad maybe good
    if ( rawMode == "DIG_L" )
    {
        submode = "LSB";
        return "SSB";
    }

    return QString();
}

void OmnirigRigDrv::rigTypeChange(int rigID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig ID" << rigID;

    if ( rigID != rigProfile.model )
        return;

    MUTEXLOCKER;

    __rigTypeChange(rigID);
}

void OmnirigRigDrv::rigStatusChange(int rigID)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "Rig ID" << rigID;

    if ( rigID != rigProfile.model )
        return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    qCDebug(runtime) << "Rig ID " << rigID;
    qCDebug(runtime) << "New Status" << rig->Status() << rig->StatusStr();

    if ( OmniRig::ST_ONLINE != rig->Status () )
    {
        qCDebug(runtime) << "New status" << rig->StatusStr();
        emit errorOccurred(tr("Rig status changed"),
                          tr("Rig is not connected"));
    }
    else
        emit rigIsReady();
}

void OmnirigRigDrv::COMException(int code,
                              QString source,
                              QString destination,
                              QString help)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << code
                                 << source
                                 << destination
                                 << help;

    emit errorOccurred(tr("Omnirig Error"),
                      QString("%1 at %2: %3 (%4)").arg(QString::number(code),
                                                       source,
                                                       destination,
                                                       help));
}

void OmnirigRigDrv::checkChanges(int params, bool force)
{
    FCT_IDENTIFICATION;

    checkFreqChange(params, force);
    checkModeChange(params, force);
    checkPTTChange(params, force);
    checkVFOChange(params, force);
    checkRITChange(params, force);
}


void OmnirigRigDrv::rigParamsChange(int rigID, int params)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rigID << params;

    if ( rigID != rigProfile.model )
        return;

    MUTEXLOCKER;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    checkChanges(params);
}

OmniRig::IRigX *OmnirigRigDrv::getRigPtr()
{
    FCT_IDENTIFICATION;

    return ( rigProfile.model == 1 ) ? omniRigInterface->Rig1()
                                     : omniRigInterface->Rig2();
}


bool OmnirigRigDrv::checkFreqChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return false;
    }

    unsigned int vfo_freq = 0;
    if ( rigProfile.getFreqInfo
         && ( params & OmniRig::PM_FREQA
              || params & OmniRig::PM_FREQB
              || params & OmniRig::PM_FREQ
              || force) )
    {
        if ( rig->Vfo() == OmniRig::PM_VFOB
            || rig->Vfo() == OmniRig::PM_VFOBB
            || rig->Vfo() == OmniRig::PM_VFOBA )
        {
            qCDebug(runtime) << "Getting VFO B Freq";
            vfo_freq = rig->FreqB();
        }
        else if ( (writableParams & OmniRig::PM_FREQA) )
        {
            qCDebug(runtime) << "Getting VFO A Freq";
            vfo_freq = rig->FreqA();
        }
        else
        {
            qCDebug(runtime) << "Getting Generic VFO Freq";
            vfo_freq = rig->Freq();
        };

        qCDebug(runtime) << "Rig Freq: "<< vfo_freq;
        qCDebug(runtime) << "Object Freq: "<< currFreq;

        if ( vfo_freq != currFreq
             || force )
        {
            currFreq = vfo_freq;
            qCDebug(runtime) << "emitting FREQ changed" << currFreq << Hz2MHz(currFreq);
            emit frequencyChanged(Hz2MHz(currFreq),
                                  Hz2MHz(getRITFreq()),
                                  Hz2MHz(getXITFreq()));
        }
    }
    return true;
}

bool OmnirigRigDrv::checkModeChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return false;
    }

    if ( rigProfile.getModeInfo )
    {
        int inParams = ( force ) ? rig->Mode() : params;

        QMap<OmniRig::RigParamX, QString>::const_iterator it;

        for ( it = modeMap.begin(); it != modeMap.end(); ++it )
        {
            if ( inParams & it.key() )
            {
                qCDebug(runtime) << "Rig Mode: "<< it.value();
                qCDebug(runtime) << "Object Mode: "<< currModeID;

                if ( currModeID != it.value()
                     || force )
                {
                    currModeID = it.value();

                    QString submode;
                    const QString mode = getModeNormalizedText(currModeID, submode);
                    qCDebug(runtime) << "emitting MODE changed" << currModeID << mode << submode << 0;
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

void OmnirigRigDrv::checkPTTChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigProfile.getPTTInfo
         && ( params & OmniRig::PM_RX
              || params & OmniRig::PM_TX
              || force ) )
    {
        int inParams = ( force ) ? rig->Tx() : params;
        bool ptt = false;

        if ( inParams & OmniRig::PM_RX )
            ptt = false;

        if ( inParams & OmniRig::PM_TX )
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

void OmnirigRigDrv::checkVFOChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigProfile.getVFOInfo
         && ( params & OmniRig::PM_VFOA
              || params & OmniRig::PM_VFOAA
              || params & OmniRig::PM_VFOAB
              || params & OmniRig::PM_VFOB
              || params & OmniRig::PM_VFOBB
              || params & OmniRig::PM_VFOBA
              || params & OmniRig::PM_VFOEQUAL
              || params & OmniRig::PM_VFOSWAP
              || force) )
    {
        int inParams = ( force
                         || params & OmniRig::PM_VFOEQUAL
                         ||  params & OmniRig::PM_VFOSWAP ) ? rig->Vfo()
                                                            : params;
        QString vfo;

        if ( inParams & OmniRig::PM_VFOA
             || inParams & OmniRig::PM_VFOAA
             || inParams & OmniRig::PM_VFOAB )
            vfo = "VFOA";

        if ( inParams & OmniRig::PM_VFOB
             || inParams & OmniRig::PM_VFOBB
             || inParams & OmniRig::PM_VFOBA )
            vfo = "VFOB";

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

void OmnirigRigDrv::checkRITChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigProfile.getRITInfo
         && ( params & OmniRig::PM_RITON
              || params & OmniRig::PM_RITOFF
              || force) )
    {
        int inParams = ( force ) ? rig->Rit() : params;
        unsigned int rit = (inParams & OmniRig::PM_RITON ) ? static_cast<unsigned int>(rig->RitOffset()) : 0;

        qCDebug(runtime) << "Rig RIT: "<< rit;
        qCDebug(runtime) << "Object RIT: "<< currRIT;

        if ( rit != currRIT || force )
        {
            currRIT = rit;
            qCDebug(runtime) << "emitting RIT changed" << QSTRING_FREQ(Hz2MHz(currRIT));
            qCDebug(runtime) << "emitting FREQ changed " << QSTRING_FREQ(Hz2MHz(currFreq))
                                                         << QSTRING_FREQ(Hz2MHz(getRITFreq()))
                                                         << QSTRING_FREQ(Hz2MHz(getXITFreq()));
            emit ritChanged(Hz2MHz(currRIT));
            emit frequencyChanged(Hz2MHz(currFreq),
                                  Hz2MHz(getRITFreq()),
                                  Hz2MHz(getXITFreq()));
        }
    }
}

double OmnirigRigDrv::getRITFreq()
{
    FCT_IDENTIFICATION;

    return currFreq + currRIT;
}

void OmnirigRigDrv::setRITFreq(double rit)
{
    currRIT = rit;
}

double OmnirigRigDrv::getXITFreq()
{
    return currFreq + currXIT;
}

void OmnirigRigDrv::setXITFreq(double xit)
{
    currXIT = xit;
}

#undef MUTEXLOCKER
