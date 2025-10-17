// This module is compiled only under Windows - therefore no ifdef related to Windows is needed

#include <Combaseapi.h>

#include <QTimer>
#include "Omnirigv2RigDrv.h"
#include "core/debug.h"
#include "rig/macros.h"

#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Drv mutex"; \
                        QMutexLocker locker(&drvLock); \
                        qCDebug(runtime) << "Using Drv"

MODULE_IDENTIFICATION("qlog.rig.driver.omnirigv2drv");

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

    return ret;
}

OmnirigV2RigDrv::OmnirigV2RigDrv(const RigProfile &profile,
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

    omniRigInterface = new OmniRigV2::OmniRigX(this);

    if ( !omniRigInterface )
    {
        //initialization failed
        qCDebug(runtime) << "Cannot allocate Omnirig structure";
        lastErrorText = tr("Initialization Error");
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

bool OmnirigV2RigDrv::open()
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

    connect(omniRigInterface, &OmniRigV2::OmniRigX::exception,
            this, &OmnirigV2RigDrv::COMException);

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

    OmniRigV2::IRigX* rigInterface = getRigPtr();

    if ( !rigInterface )
    {
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Cannot get Rig Instance";
        return false;
    }

    rig = new OmniRigV2::RigX(rigInterface);

    if ( !rig )
    {
        lastErrorText = tr("Initialization Error");
        qCDebug(runtime) << "Cannot allocate Rig Interface";
        return false;
    }

    __rigTypeChange(rigProfile.model);

    QTimer::singleShot(300, this, [this]()
    {
        OmnirigV2RigDrv::rigStatusChange(rigProfile.model);
    });

    emit rigIsReady();

    // TODO - solve timeout from library. Is it possible????
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

    if ( rig->Vfo() == OmniRigV2::PM_VFOB
         || rig->Vfo() == OmniRigV2::PM_VFOBB
         || rig->Vfo() == OmniRigV2::PM_VFOBA )
    {
        qCDebug(runtime) << "Setting VFO B Freq";
        rig->SetFreqB(internalFreq);
    }
    else if ( (writableParams & OmniRigV2::PM_FREQA) )
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

void OmnirigV2RigDrv::setRawMode(const QString &rawMode)
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

    const QList<OmniRigV2::RigParamX> mappedMode = modeMap.keys(rawMode);

    if ( mappedMode.size() > 0 )
    {
        OmniRigV2::RigParamX rawMode = mappedMode.at(0);
        qCDebug(runtime) << "Mode Found" << rawMode;
        if ( rawMode & writableParams )
        {
            qCDebug(runtime) << "Setting Mode";
            rig->SetMode(rawMode);
            commandSleep();
        }
    }
}

void OmnirigV2RigDrv::setMode(const QString &mode, const QString &submode, bool digiVariant)
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

void OmnirigV2RigDrv::setPTT(bool newPTTSTate)
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

    rig->SetTx((newPTTSTate) ? OmniRigV2::PM_TX : OmniRigV2::PM_RX);

    commandSleep();

}

void OmnirigV2RigDrv::setKeySpeed(qint16)
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigV2RigDrv::syncKeySpeed(qint16)
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigV2RigDrv::sendMorse(const QString &)
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
}

void OmnirigV2RigDrv::stopMorse()
{
    FCT_IDENTIFICATION;
    //not implemented
    return;
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

    // not timer
    return;
}

void OmnirigV2RigDrv::sendDXSpot(const DxSpot &spot)
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

    qCDebug(runtime) << "Rig ID" << rigID << "Changed";

    if ( rigID != rigProfile.model )
        return;

    readableParams = rig->ReadableParams();
    writableParams = rig->WriteableParams();

    qCDebug(runtime) << "R-params" << QString::number(readableParams, 16)
                     << "W-params" << QString::number(writableParams, 16);
}

void OmnirigV2RigDrv::rigStatusChange(int rigID)
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

    if ( OmniRigV2::ST_ONLINE != rig->Status () )
    {
        qCDebug(runtime) << "New status" << rig->StatusStr();
        emit errorOccurred(tr("Rig status changed"),
                          tr("Rig is not connected"));
    }
    else
        emit rigIsReady();
}

void OmnirigV2RigDrv::COMException(int code,
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

void OmnirigV2RigDrv::rigParamsChange(int rigID, int params)
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

OmniRigV2::IRigX *OmnirigV2RigDrv::getRigPtr()
{
    FCT_IDENTIFICATION;

    switch ( rigProfile.model )
    {
    case 1: return omniRigInterface->Rig1();
    case 2: return omniRigInterface->Rig2();
    case 3: return omniRigInterface->Rig3();
    case 4: return omniRigInterface->Rig4();
    }

    return nullptr;
}

void OmnirigV2RigDrv::commandSleep()
{
    QThread::msleep(100);
}

const QString OmnirigV2RigDrv::getModeNormalizedText(const QString &rawMode, QString &submode)
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

void OmnirigV2RigDrv::checkChanges(int params, bool force)
{
    FCT_IDENTIFICATION;

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

    unsigned int vfo_freq = 0;
    if ( rigProfile.getFreqInfo
         && ( params & OmniRigV2::PM_FREQA
              || params & OmniRigV2::PM_FREQB
              || params & OmniRigV2::PM_FREQ
              || force) )
    {
        if ( rig->Vfo() == OmniRigV2::PM_VFOB
            || rig->Vfo() == OmniRigV2::PM_VFOBB
            || rig->Vfo() == OmniRigV2::PM_VFOBA )
        {
            qCDebug(runtime) << "Getting VFO B Freq";
            vfo_freq = rig->FreqB();
        }
        else if ( (writableParams & OmniRigV2::PM_FREQA) )
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
        int inParams = ( force ) ? rig->Mode() : params;

        QMap<OmniRigV2::RigParamX, QString>::const_iterator it;

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

void OmnirigV2RigDrv::checkPTTChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigProfile.getPTTInfo
         && ( params & OmniRigV2::PM_RX
              || params & OmniRigV2::PM_TX
              || force ) )
    {
        int inParams = ( force ) ? rig->Tx() : params;
        bool ptt = false;

        if ( inParams & OmniRigV2::PM_RX )
            ptt = false;

        if ( inParams & OmniRigV2::PM_TX )
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

    if ( rigProfile.getVFOInfo
         && ( params & OmniRigV2::PM_VFOA
              || params & OmniRigV2::PM_VFOAA
              || params & OmniRigV2::PM_VFOAB
              || params & OmniRigV2::PM_VFOB
              || params & OmniRigV2::PM_VFOBB
              || params & OmniRigV2::PM_VFOBA
              || params & OmniRigV2::PM_VFOEQUAL
              || params & OmniRigV2::PM_VFOSWAP
              || force) )
    {
        int inParams = ( force
                         || params & OmniRigV2::PM_VFOEQUAL
                         ||  params & OmniRigV2::PM_VFOSWAP ) ? rig->Vfo()
                                                            : params;
        QString vfo;

        if ( inParams & OmniRigV2::PM_VFOA
             || inParams & OmniRigV2::PM_VFOAA
             || inParams & OmniRigV2::PM_VFOAB )
            vfo = "VFOA";

        if ( inParams & OmniRigV2::PM_VFOB
             || inParams & OmniRigV2::PM_VFOBB
             || inParams & OmniRigV2::PM_VFOBA )
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

void OmnirigV2RigDrv::checkRITChange(int params, bool force)
{
    FCT_IDENTIFICATION;

    if ( !rig )
    {
        qCWarning(runtime) << "Rig is not active";
        return;
    }

    if ( rigProfile.getRITInfo
         && ( params & OmniRigV2::PM_RITON
              || params & OmniRigV2::PM_RITOFF
              || force) )
    {
        int inParams = ( force ) ? rig->Rit() : params;
        unsigned int rit = (inParams & OmniRigV2::PM_RITON ) ? static_cast<unsigned int>(rig->RitOffset()) : 0;

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

double OmnirigV2RigDrv::getRITFreq()
{
    FCT_IDENTIFICATION;

    return currFreq + currRIT;

}

void OmnirigV2RigDrv::setRITFreq(double rit)
{
    currRIT = rit;
}

double OmnirigV2RigDrv::getXITFreq()
{
    return currFreq + currXIT;
}

void OmnirigV2RigDrv::setXITFreq(double xit)
{
     currXIT = xit;
}

#undef MUTEXLOCKER
