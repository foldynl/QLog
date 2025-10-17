#include <QColor>
#include <QPalette>

#include "TCIRigDrv.h"
#include "rig/macros.h"
#include "data/Data.h"
#include "data/BandPlan.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.rig.driver.tcidrv");

// https://github.com/ExpertSDR3/TCI/blob/main/TCI%20Protocol.pdf

QList<QPair<int, QString> > TCIRigDrv::getModelList()
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    ret << QPair<int, QString>(1, tr("Rig 0")) // 0 cause a crash in setting dialog when RigModel combo is expacted, I don't know why
        << QPair<int, QString>(2, tr("Rig 1"))
        << QPair<int, QString>(3, tr("Rig 2"))
        << QPair<int, QString>(4, tr("Rig 3"));

    return ret;
}

RigCaps TCIRigDrv::getCaps(int)
{
    FCT_IDENTIFICATION;

    RigCaps ret;

    ret.isNetworkOnly = true;

    ret.canGetFreq = true;
    ret.canGetMode = true;
    ret.canGetVFO = true;
    ret.canGetRIT = true;
    ret.canGetXIT = true;
    ret.canGetPTT = true;
    ret.canSendMorse = true;
    ret.canGetKeySpeed = true;
    ret.canGetPWR = true;
    ret.canProcessDXSpot = true;

    return ret;
}

TCIRigDrv::TCIRigDrv(const RigProfile &profile, QObject *parent)
    : GenericRigDrv(profile, parent),
      ready(false),
      receivedOnly(false),
      currFreq(0.0),
      currRIT(0.0),
      currXIT(0.0),
      RITEnabled(false),
      XITEnabled(false)
{
    FCT_IDENTIFICATION;

    connect(&ws, &QWebSocket::connected, this, &TCIRigDrv::onConnected);
#if (QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
    connect(&ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &TCIRigDrv::onSocketError);
#else
    connect(&ws, &QWebSocket::errorOccurred, this, &TCIRigDrv::onSocketError);
#endif
}

TCIRigDrv::~TCIRigDrv()
{
    FCT_IDENTIFICATION;

    TCIRigDrv::stopTimers();
}

bool TCIRigDrv::open()
{
    FCT_IDENTIFICATION;

    QUrl url;
    url.setScheme("ws");
    url.setHost(rigProfile.hostname);
    url.setPort(rigProfile.netport);

    ws.open(url);
    return true;
}

bool TCIRigDrv::isMorseOverCatSupported()
{
    FCT_IDENTIFICATION;

    return true;
}

QStringList TCIRigDrv::getAvailableModes()
{
    return modeList;
}

void TCIRigDrv::setFrequency(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newFreq;

    if ( !rigProfile.getFreqInfo )
        return;

    unsigned long long internalFreq = static_cast<unsigned long long>(newFreq);

    if ( internalFreq == currFreq )
    {
        qCDebug(runtime) << "The same Freq - skip change" << internalFreq << currFreq;
        return;
    }

    QStringList args = {"0", //VFOID
                        QString::number(internalFreq)
                       };
    sendCmd("vfo", true, args);
}

void TCIRigDrv::setRawMode(const QString &rawMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    if ( !rigProfile.getModeInfo || rawMode.isEmpty() )
        return;

    if ( rawMode == currMode )
    {
        qCDebug(runtime) << "The same Mode - skip change" << rawMode << currMode;
        return;
    }

    QStringList args = {rawMode};
    sendCmd("modulation", true, args);
}

void TCIRigDrv::setMode(const QString &mode, const QString &subMode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << subMode << digiVariant;

    setRawMode(mode2RawMode(mode, subMode, digiVariant));
}

void TCIRigDrv::setPTT(bool newPTTState)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newPTTState;

    if ( !rigProfile.getPTTInfo )
        return;

    if ( receivedOnly )
    {
        qCDebug(runtime) << "Only receiver - no action";
        return;
    }

    QStringList args = {(newPTTState) ? "true" : "false"};
    sendCmd("trx", true, args);
}

void TCIRigDrv::setKeySpeed(qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    if ( !rigProfile.getKeySpeed )
        return;

    if ( receivedOnly )
    {
        qCDebug(runtime) << "Only receiver - no action";
        return;
    }

    QStringList args = {QString::number(wpm)};
    sendCmd("cw_keyer_speed", false, args);
    sendCmd("cw_macros_speed", false, args);
}

void TCIRigDrv::syncKeySpeed(qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    if ( !rigProfile.keySpeedSync )
        return;

    setKeySpeed(wpm);
}

void TCIRigDrv::sendMorse(const QString &text)
{
    FCT_IDENTIFICATION;

    if ( text.isEmpty() )
        return;

    if ( receivedOnly )
    {
        qCDebug(runtime) << "Only receiver - no action";
        return;
    }

    QStringList args {text};
    sendCmd("cw_macros", true, args);
}

void TCIRigDrv::stopMorse()
{
    FCT_IDENTIFICATION;

    if ( receivedOnly )
    {
        qCDebug(runtime) << "Only receiver - no action";
        return;
    }

    sendCmd("cw_macros_stop", false);
}

void TCIRigDrv::sendState()
{
    FCT_IDENTIFICATION;

    // it seems that Rig sends its state at the begining of the session
    // but it has to be implmented due to "Exit From Manual Mode"

    sendCmd("vfo", true, QStringList("0")); //Freq
    // sendCmd("trx", true); // PTT
                             // Disabled because if this command is received by Thetis
                             // then Thetis unexpectedly closes the connection.
                             // Expert SDR software does not do this.

    sendCmd("modulation", true); //Mode
    //sendCmd(); // Current VFO ????
    sendCmd("drive", true); //PWR
    sendCmd("rit_offset", true);
    sendCmd("rit_enable", true);
    sendCmd("xit_offset", true);
    sendCmd("xit_enable", true);
    sendCmd("cw_macros_speed", false);
}

void TCIRigDrv::stopTimers()
{
    FCT_IDENTIFICATION;

    // no timer

    // send STOP command????

    // close the WebSocket here becuase this method is called
    // via queue signals. What caused that there is not warning
    // that Websocket is destroyed from another thread
    ws.close();
    return;
}

void TCIRigDrv::sendDXSpot(const DxSpot &spot)
{
    FCT_IDENTIFICATION;

    if ( !rigProfile.dxSpot2Rig )
        return;

    const QColor &spotColor = Data::statusToColor(spot.status, spot.dupeCount, QColor(187,194,195));

    unsigned long long internalFreq = static_cast<unsigned long long>(MHz(spot.freq));

    QString submode;
    const QString &mode = BandPlan::bandPlanMode2ExpectedMode(spot.bandPlanMode, submode);

    QStringList args = {
        spot.callsign,
        mode2RawMode(mode, submode, (spot.bandPlanMode == BandPlan::BAND_MODE_FT8 || spot.bandPlanMode == BandPlan::BAND_MODE_DIGITAL)),
        QString::number(internalFreq),
        QString::number(spotColor.rgba()),
        spot.callsign
    };

    sendCmd("spot", 0, args);
}

void TCIRigDrv::onConnected()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Rig is connected";

    connect(&ws, &QWebSocket::textMessageReceived,
            this, &TCIRigDrv::onTextMessageReceived);

    // QLog has to wait for READY message to complete connection = to emit RigReady
}

void TCIRigDrv::onSocketError(QAbstractSocket::SocketError socker_error)
{
    FCT_IDENTIFICATION;

    QString error_msg;

    qCDebug(runtime) << socker_error;

    switch (socker_error)
    {
    case QAbstractSocket::ConnectionRefusedError:
        error_msg.append(QObject::tr("Connection Refused"));
        break;

    case QAbstractSocket::RemoteHostClosedError:
        error_msg.append(QObject::tr("Host closed the connection"));
        //reconectRequested = true;
        break;

    case QAbstractSocket::HostNotFoundError:
        error_msg.append(QObject::tr("Host not found"));
        break;
    case QAbstractSocket::SocketTimeoutError:
        error_msg.append(QObject::tr("Timeout"));
        //reconectRequested = true;
        break;
    case QAbstractSocket::NetworkError:
        error_msg.append(QObject::tr("Network Error"));
        break;
    default:
        error_msg.append(QObject::tr("Internal Error"));
    }

   emit errorOccurred(tr("Error Occurred"),
                     error_msg);
}

void TCIRigDrv::onTextMessageReceived(const QString &message)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << message;

    if ( message.isEmpty() )
        return;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    const QStringList &commands = message.split(";", Qt::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
    const QStringList &commands = message.split(";", QString::SkipEmptyParts);
#endif

    for ( const QString &command : commands )
    {
        // message structure CMD1;CMD2;CMD3....
        // CMD structure cmdName:arg0,arg1,arg2
        qCDebug(runtime) << command;

        const QString &trimmedCommand = command.trimmed();

        if ( trimmedCommand.isEmpty() )
            continue;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        const QStringList &cmdElemets = trimmedCommand.split(":", Qt::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
        const QStringList &cmdElemets = trimmedCommand.split(":", QString::SkipEmptyParts);
#endif

        const QString &cmdName = cmdElemets.at(0).toLower().trimmed();
        QStringList cmdArgs;
        TCIRigDrv::parseFce parser = responseParsers.value(cmdName);

        if ( !parser )
        {
            qCDebug(runtime) << "Parser function is not defined for" << cmdName;
            continue;
        }

        if ( cmdElemets.size() > 1 )
        {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
            cmdArgs = cmdElemets.at(1).split(",", Qt::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
            cmdArgs = cmdElemets.at(1).split(",", QString::SkipEmptyParts);
#endif
        }

        // Trim argument - is a space possible?
        for( QString &arg : cmdArgs )
            arg = arg.trimmed();

        (this->*(parser))(cmdArgs);
    }
}

void TCIRigDrv::sendCmd(const QString &cmd,
                     bool addRigID,
                     const QStringList& args)
{
    FCT_IDENTIFICATION;

    if ( !ready )
    {
        qCDebug(runtime) << "Rig is not ready";
        return;
    }

    QStringList modifiedArgs(args);

    if ( addRigID )
        modifiedArgs.prepend(QString::number(rigProfile.model - 1));

    //Mandatory part of the command
    QString cmdText = cmd.toUpper();

    if ( modifiedArgs.size() > 0 )
        cmdText.append(":" + modifiedArgs.join(","));

    cmdText.append(";");

    qCDebug(runtime) << "Sending command:" << cmdText;
    ws.sendTextMessage(cmdText);
}

const QString TCIRigDrv::getModeNormalizedText(const QString &rawMode, QString &submode)
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

    if ( rawMode == "NFM" )
        return "FM";

    if ( rawMode == "WFM" )
        return "FM";

    if ( rawMode == "DIGL" )
    {
        submode = "LSB";
        return "SSB";
    }

    if ( rawMode == "DIGU" )
    {
        submode = "USB";
        return "SSB";
    }

    return QString();
}

const QString TCIRigDrv::mode2RawMode(const QString &mode, const QString &submode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    if (mode == "SSB")
    {
        QString innerSubmode = submode;
        if ( digiVariant )
        {
            const QString digMode = QLatin1String("DIG") + submode.at(0);
            if ( modeList.contains(digMode) )
                innerSubmode = digMode;
        }
        return innerSubmode;
    }

    if ( mode == "CW" )
        return mode;

    if ( mode == "FM" )
        return "NFM";

    if ( mode == "AM" )
        return mode;

    if ( mode == "FT8" )
    {
        if ( modeList.contains("FT8") )
            return "FT8";

        if ( modeList.contains("DIGU") )
            return "DIGU";

        return "USB";
    }

    return QString();
}

#define CHECK_PARAMS_COUNT( size, min ) \
    if ( size < min ) \
    { qCWarning(runtime) << "Incorrect number of arguments" << size << min; \
      return; }

void TCIRigDrv::rspPROTOCOL(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 — program name.
    // аrg1 — version of the protocol.
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    qCDebug(runtime) << "Program name:" << cmdArgs.at(0)
                     << "Procotol version:" << cmdArgs.at(1);
}

void TCIRigDrv::rspREADY(const QStringList &)
{
    FCT_IDENTIFICATION;

    ready = true;
    emit rigIsReady();
}

void TCIRigDrv::rspSTART(const QStringList &)
{
    FCT_IDENTIFICATION;

    ready = true;
}

void TCIRigDrv::rspSTOP(const QStringList &)
{
    FCT_IDENTIFICATION;

    ready = false;

    emit errorOccurred(tr("Rig status changed"),
                      tr("Rig is not connected"));
}

void TCIRigDrv::rspRECEIVE_ONLY(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - received only (true), transceiver (false)
    CHECK_PARAMS_COUNT(cmdArgs.size(), 1);

    receivedOnly = ( cmdArgs.at(0).toLower() == "true" );
}

void TCIRigDrv::rspMODULATIONS_LIST(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 ... N
    for ( const QString &arg : cmdArgs )
        modeList << arg.toUpper();

    qCDebug(runtime) << modeList;
}

void TCIRigDrv::rspVFO(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - VFO
    // arg2 - freq
    CHECK_PARAMS_COUNT(cmdArgs.size(), 3);

    if ( !rigProfile.getFreqInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    if ( cmdArgs.at(1) != "0" )
    {
        qCDebug(runtime) << "Skipping info from VFO" << cmdArgs.at(1);
        return;
    }

    bool ok;
    currFreq = cmdArgs.at(2).toDouble(&ok);
    if ( ok )
    {
        qCDebug(runtime) << "Rig Freq" << QSTRING_FREQ(Hz2MHz(currFreq));
        qCDebug(runtime) << "emitting FREQ changed";
        emit frequencyChanged(Hz2MHz(currFreq),
                              Hz2MHz(getRITFreq()),
                              Hz2MHz(getXITFreq()));
    }
    else
    {
        qCDebug(runtime) << "Received Freq is not double" << cmdArgs.at(2);
    }
}

void TCIRigDrv::rspTRX(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - status
    // arg2 - signal source (optional)
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getPTTInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    bool ptt = ( cmdArgs.at(1).compare("true", Qt::CaseInsensitive) == 0 );

    qCDebug(runtime) << "Rig PTT:"<< ptt;
    qCDebug(runtime) << "emitting PTT changed" << ptt;
    emit pttChanged(ptt);
}

void TCIRigDrv::rspMODULATION(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - mode
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getModeInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    currMode = cmdArgs.at(1);
    QString submode;
    const QString mode = getModeNormalizedText(currMode, submode);


    qCDebug(runtime) << "emitting MODE changed" << currMode << mode << submode;
    emit modeChanged(currMode,
                     mode, submode,
                     0);  // TODO: bandwidth should also be emited
                          // when RX_FILTER_BAND is processed
}

void TCIRigDrv::rspTUNE_DRIVE(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - power output (in watts????)
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);


    // NO ACTION NOW
}

void TCIRigDrv::rspDRIVE(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - power output (in watts)
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getPWRInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    bool ok;
    unsigned int rigPower = cmdArgs.at(1).toUInt(&ok);

    if ( ok )
    {
        qCDebug(runtime) << "Rig PWR" << rigPower;
        qCDebug(runtime) << "emitting PWR changed";
        emit powerChanged(rigPower);
    }
    else
    {
        qCDebug(runtime) << "Received PWR is not a number" << cmdArgs.at(1);
    }
}

void TCIRigDrv::rspRIT_OFFSET(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - rit (+- in HZ)
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getRITInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    bool ok;
    currRIT = cmdArgs.at(1).toDouble(&ok);

    if ( ok )
    {
        qCDebug(runtime) << "Rig RIT" << RITEnabled << currRIT;
        qCDebug(runtime) << "emitting RIT changed" << QSTRING_FREQ(Hz2MHz(getRawRIT()));
        qCDebug(runtime) << "emitting FREQ changed " << QSTRING_FREQ(Hz2MHz(currFreq))
                                                     << QSTRING_FREQ(Hz2MHz(getRITFreq()))
                                                     << QSTRING_FREQ(Hz2MHz(getXITFreq()));

        emit ritChanged(Hz2MHz(getRawRIT()));
        emit frequencyChanged(Hz2MHz(currFreq),
                              Hz2MHz(getRITFreq()),
                              Hz2MHz(getXITFreq()));
    }
    else
    {
        qCDebug(runtime) << "Received RIT is not a double" << cmdArgs.at(1);
    }
}

void TCIRigDrv::rspXIT_OFFSET(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - rit (+- in HZ)
    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getXITInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    bool ok;
    currXIT = cmdArgs.at(1).toDouble(&ok);

    if ( ok )
    {
        qCDebug(runtime) << "Rig XIT" << XITEnabled << currXIT;
        qCDebug(runtime) << "emitting XIT changed" << QSTRING_FREQ(Hz2MHz(getRawXIT()));
        qCDebug(runtime) << "emitting FREQ changed " << QSTRING_FREQ(Hz2MHz(currFreq))
                                                     << QSTRING_FREQ(Hz2MHz(getRITFreq()))
                                                     << QSTRING_FREQ(Hz2MHz(getXITFreq()));

        emit xitChanged(Hz2MHz(getRawXIT()));
        emit frequencyChanged(Hz2MHz(currFreq),
                              Hz2MHz(getRITFreq()),
                              Hz2MHz(getXITFreq()));
    }
    else
    {
        qCDebug(runtime) << "Received XIT is not a double" << cmdArgs.at(1);
    }
}

void TCIRigDrv::rspCW_MACROS_SPEED(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - wpm
    CHECK_PARAMS_COUNT(cmdArgs.size(), 1);

    bool ok;
    unsigned int wpm = cmdArgs.at(0).toInt(&ok);

    if ( ok )
    {
        qCDebug(runtime) << "Rig Key Speed:" << wpm;
        emit keySpeedChanged(wpm);
    }
    else
    {
        qCDebug(runtime) << "Received RIT is not a double" << cmdArgs.at(1);
    }
}

void TCIRigDrv::rspRIT_ENABLE(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - status indicator.

    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getRITInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    RITEnabled = ( cmdArgs.at(1).toLower() == "true" );

    qCDebug(runtime) << "Rig RIT status changed" << RITEnabled << currRIT;
    qCDebug(runtime) << "emitting RIT changed" << QSTRING_FREQ(Hz2MHz(getRawRIT()));
    emit ritChanged(Hz2MHz(getRawRIT()));
    emit frequencyChanged(Hz2MHz(currFreq),
                          Hz2MHz(getRITFreq()),
                          Hz2MHz(getXITFreq()));
}

void TCIRigDrv::rspXIT_ENABLE(const QStringList &cmdArgs)
{
    FCT_IDENTIFICATION;

    // arg0 - rigid
    // arg1 - status indicator.

    CHECK_PARAMS_COUNT(cmdArgs.size(), 2);

    if ( !rigProfile.getXITInfo )
        return;

    if ( cmdArgs.at(0) != QString::number(rigProfile.model - 1))
    {
        qCDebug(runtime) << "Command is not for QLog";
        return;
    }

    XITEnabled = ( cmdArgs.at(1).toLower() == "true" );

    qCDebug(runtime) << "Rig XIT status changed" << XITEnabled << currXIT;
    qCDebug(runtime) << "emitting XIT changed" << QSTRING_FREQ(Hz2MHz(getRawXIT()));
    emit xitChanged(Hz2MHz(getRawXIT()));
    emit frequencyChanged(Hz2MHz(currFreq),
                          Hz2MHz(getRITFreq()),
                          Hz2MHz(getXITFreq()));
}

#undef CHECK_PARAMS_COUNT

double TCIRigDrv::getRITFreq()
{
    FCT_IDENTIFICATION;

    return currFreq + getRawRIT();
}

void TCIRigDrv::setRITFreq(double rit)
{
    currRIT = rit;
}

double TCIRigDrv::getXITFreq()
{
    return currFreq + currXIT;
}

void TCIRigDrv::setXITFreq(double xit)
{
    currXIT = xit;
}

double TCIRigDrv::getRawRIT()
{
    return ( ( RITEnabled ) ? currRIT : 0.0 );
}

double TCIRigDrv::getRawXIT()
{
    return ( ( XITEnabled ) ? currXIT : 0.0 );
}

