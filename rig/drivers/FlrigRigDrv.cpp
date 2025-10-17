#include "FlrigRigDrv.h"
#include "core/debug.h"
#include "rig/macros.h"

MODULE_IDENTIFICATION("qlog.rig.driver.flrigrigdrv");

RigCaps FlrigRigDrv::getCaps(int)
{
    FCT_IDENTIFICATION;

    RigCaps ret;

    ret.isNetworkOnly = true;

    ret.canGetFreq = true;
    ret.canGetMode = true;
    ret.canGetVFO = true;
    ret.canGetPTT = true;
    ret.canGetPWR = true;
    ret.canGetKeySpeed = true;
    ret.canSendMorse = true;
    ret.needPolling = true;

    return ret;
}

QList<QPair<int, QString> > FlrigRigDrv::getModelList()
{
    FCT_IDENTIFICATION;

    return {{1, "FLRig"}};
}

FlrigRigDrv::FlrigRigDrv(const RigProfile &profile,
                         QObject *parent)
    : GenericRigDrv(profile, parent),
      networkManager(new QNetworkAccessManager(this)),
      rigReady(false),
      hostUrl(QString("http://%1:%2/").arg(profile.hostname).arg(profile.netport))
{
    FCT_IDENTIFICATION;

    buildModeMappingHash();
    resetCurrStates();
}

FlrigRigDrv::~FlrigRigDrv()
{
    FCT_IDENTIFICATION;
    networkManager->deleteLater();
}

bool FlrigRigDrv::open()
{
    FCT_IDENTIFICATION;

    reqGET_MODES();

    // Cannot retrieve mode from rig while Send/Pause is active. turn it off.
    reqCWIO_SEND(false);
    return true;
}

bool FlrigRigDrv::isMorseOverCatSupported()
{
    FCT_IDENTIFICATION;

    return true;
}

QStringList FlrigRigDrv::getAvailableModes()
{
    FCT_IDENTIFICATION;

    return rigAvailableModes.keys();
}

void FlrigRigDrv::setFrequency(double newFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << QSTRING_FREQ(newFreq);

    if ( !rigProfile.getFreqInfo || !rigReady ) return;

    if ( newFreq == currFreq )
    {
        qCDebug(runtime) << "The same Freq - skip change" << currFreq << currFreq;
        return;
    }

    sendXmlRpcCommand("rig.set_vfo", { newFreq });
}

void FlrigRigDrv::setRawMode(const QString &rawMode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    if ( !rigProfile.getModeInfo || rawMode.isEmpty() || !rigReady ) return;

    if ( rawMode == currRawMode )
    {
        qCDebug(runtime) << "The same Mode - skip change" << rawMode << currRawMode;
        return;
    }

    sendXmlRpcCommand("rig.set_mode", { rawMode });
}

void FlrigRigDrv::setMode(const QString &mode, const QString &subMode, bool digiVariant)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << subMode << digiVariant;

    setRawMode(mode2RawMode(mode, subMode, digiVariant));
}

void FlrigRigDrv::setPTT(bool ptt)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ptt;

    if ( !rigProfile.getPTTInfo || !rigReady ) return;

    sendXmlRpcCommand("rig.set_ptt", { ptt });
}

void FlrigRigDrv::setKeySpeed(qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    if ( wpm <= 0 || !rigReady ) return;

    if ( !rigProfile.getKeySpeed ) return;

    sendXmlRpcCommand("rig.cwio_set_wpm", { wpm });
}

void FlrigRigDrv::syncKeySpeed(qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    if ( !rigProfile.keySpeedSync || !rigReady ) return;

    setKeySpeed(wpm);
}

void FlrigRigDrv::sendMorse(const QString &text)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << text;

    if ( text.isEmpty() || !rigReady ) return;

    QString innerText = text;

    if ( !sendTextFlag && !text.contains('['))
        innerText.prepend("[");

    innerText.replace("\n", "]");

    sendTextFlag = !innerText.contains("]");

    sendXmlRpcCommand("rig.cwio_text", {innerText});
}

void FlrigRigDrv::stopMorse()
{
    FCT_IDENTIFICATION;

    if ( !rigReady ) return;

    reqCWIO_SEND(false);
}

void FlrigRigDrv::sendState()
{
    FCT_IDENTIFICATION;

    resetCurrStates();
}

void FlrigRigDrv::stopTimers()
{
    FCT_IDENTIFICATION;

    for ( QTimer* timer : static_cast<const QList<QTimer*>>(runningTimers) )
        timer->stop();

    runningTimers.clear();
    // deallocation is performed during the class destruction - QTimer is a child
}

void FlrigRigDrv::sendDXSpot(const DxSpot &)
{
    FCT_IDENTIFICATION;
    // not implemented
}

void FlrigRigDrv::startRigStatePoll()
{
    FCT_IDENTIFICATION;

    reqGET_VFO();
    reqGET_MODE();
    reqGET_BW();
    reqGET_POWER();
    reqGET_AB();
    reqGET_PTT();
    reqCWIO_GET_WPM();
}

void FlrigRigDrv::reqGET_MODES()
{
    FCT_IDENTIFICATION;

    sendXmlRpcCommand("rig.get_modes");
}

void FlrigRigDrv::reqCWIO_SEND(bool state)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << state;

    sendTextFlag = state;
    sendXmlRpcCommand("rig.cwio_send", {(state) ? 1 : 0});
}

void FlrigRigDrv::rspGET_MODES(const QVariant &value)
{
    FCT_IDENTIFICATION;

    /* <?xml version=\"1.0\"?>
     * <methodResponse><params><param>
     * <value><array><data><value>LSB</value><value>USB</value></data></array></value>
     * </param></params></methodResponse>
     */

    qCDebug(function_parameters) << value;

    const QStringList &receivedModes = value.toStringList();

    rigAvailableModes.clear();

    for ( const QString &mode : receivedModes)
        if ( raw2ADIFModeMapping.contains(mode) )
            rigAvailableModes[mode] = raw2ADIFModeMapping.value(mode);

    qCDebug(runtime) << "Rig Modes" << rigAvailableModes.keys();

    rigReady = true;
    emit rigIsReady();
    startRigStatePoll();
}

void FlrigRigDrv::reqGET_VFO()
{
    FCT_IDENTIFICATION;

    if ( rigProfile.getFreqInfo ) sendXmlRpcCommand("rig.get_vfo");
}

void FlrigRigDrv::rspGET_VFO(const QVariant &value)
{
    FCT_IDENTIFICATION;

    /* <?xml version=\"1.0\"?>
     * <methodResponse><params><param>
     * <value>14070000</value>
     * </param></params></methodResponse>
     */

    qCDebug(function_parameters) << value;

    double vfoFreq = Hz2MHz(value.toLongLong());
    qCDebug(runtime) << "Rig Freq: "<< QSTRING_FREQ(vfoFreq);
    qCDebug(runtime) << "Object Freq: "<< QSTRING_FREQ(currFreq);

    if ( vfoFreq != currFreq )
    {
        currFreq = vfoFreq;
        qCDebug(runtime) << "emitting FREQ changed";
        emit frequencyChanged(currFreq,
                              currFreq,
                              currFreq);
    }

    QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqGET_VFO);
}

void FlrigRigDrv::reqGET_MODE()
{
    FCT_IDENTIFICATION;

     if ( rigProfile.getModeInfo ) sendXmlRpcCommand("rig.get_mode");
}

void FlrigRigDrv::rspGET_MODE(const QVariant &value)
{
    FCT_IDENTIFICATION;

    /* <?xml version=\"1.0\"?>
     * <methodResponse><params><param>
     * <value>USB</value>
     * </param></params></methodResponse>
     */

    qCDebug(function_parameters) << value;

    const QString &rawMode = value.toString();

    qCDebug(runtime) << "Rig Mode: " << rawMode;
    qCDebug(runtime) << "Object Mode: "<< currRawMode;

    if ( rawMode != currRawMode )
    {
        // mode change
        currRawMode = rawMode;

        QString submode;
        const QString &mode = getModeNormalizedText(rawMode, submode);

        qCDebug(runtime) << "emitting MODE changed" << rawMode << mode << submode;
        emit modeChanged(rawMode, mode, submode, 0);
    }

    QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqGET_MODE);
}

void FlrigRigDrv::reqGET_BW()
{
    FCT_IDENTIFICATION;

    if ( rigProfile.getModeInfo ) sendXmlRpcCommand("rig.get_bw");
}

void FlrigRigDrv::rspGET_BW(const QVariant &value)
{
    FCT_IDENTIFICATION;

    /* <?xml version=\"1.0\"?>
     * <methodResponse><params><param>
     * <value><array><data><value>NONE</value><value></value></data></array></value>
     * </param></params></methodResponse>
     */

    qCDebug(function_parameters) << value;

    const QVariantList &values = value.toList();

    if ( values.isEmpty() )
        qCDebug(runtime) << "BW List is empty";
    else
    {
        bool ok = false;
        int rigBW = values.at(0).toInt(&ok);
        if ( !ok ) qCDebug(runtime) << "Received BW is not a number";

        qCDebug(runtime) << "Rig BW: "<< rigBW;
        qCDebug(runtime) << "Object BW: "<< currBW;

        if ( rigBW != currBW )
        {
            currBW = rigBW;

            qCDebug(runtime) << "emitting BW changed" << currBW;
            QString submode;
            const QString &mode = getModeNormalizedText(currRawMode, submode);
            emit modeChanged(currRawMode, mode, submode, currBW);
        }
    }
    QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqGET_BW);
}

void FlrigRigDrv::reqGET_POWER()
{
    FCT_IDENTIFICATION;

    if ( rigProfile.getPWRInfo )  sendXmlRpcCommand("rig.get_power");
}

void FlrigRigDrv::rspGET_POWER(const QVariant &value)
{
    FCT_IDENTIFICATION;

    /* <?xml version=\"1.0\"?>
     * <methodResponse><params><param>
     * <value>0</value>
     * </param></params></methodResponse>
     */

    qCDebug(function_parameters) << value;

    qlonglong pwr = value.toLongLong();

    qCDebug(runtime) << "Rig PWR: "<< pwr;
    qCDebug(runtime) << "Object PWR: "<< currPWR;

    if ( pwr != currPWR )
    {
        currPWR = pwr;

        qCDebug(runtime) << "emitting PWR changed " << pwr;
        emit powerChanged(pwr);
    }

    QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqGET_POWER);
}

void FlrigRigDrv::reqGET_AB()
{
    FCT_IDENTIFICATION;

    if ( rigProfile.getVFOInfo )  sendXmlRpcCommand("rig.get_AB");
}

void FlrigRigDrv::rspGET_AB(const QVariant &value)
{
    FCT_IDENTIFICATION;

    /* <?xml version=\"1.0\"?>
     * <methodResponse><params><param>
     * <value>A</value>
     * </param></params></methodResponse>
     */

    qCDebug(function_parameters) << value;

    const QString &vfo = "VFO " + value.toString();

    qCDebug(runtime) << "Rig AB: "<< vfo;
    qCDebug(runtime) << "Object AB: "<< currVFO;

    if ( vfo != currVFO )
    {
        currVFO = vfo;

        qCDebug(runtime) << "emitting VFO changed" << currVFO;
        emit vfoChanged(currVFO);
    }

   QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqGET_AB);
}

void FlrigRigDrv::reqGET_PTT()
{
    FCT_IDENTIFICATION;

    if ( rigProfile.getPTTInfo )  sendXmlRpcCommand("rig.get_ptt");
}

void FlrigRigDrv::rspGET_PTT(const QVariant &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << value;

    bool ptt = value.toBool();

    qCDebug(runtime) << "Rig PTT: "<< ptt;
    qCDebug(runtime) << "Object PTT: "<< QString::number(currPTT);

    if ( ptt != currPTT )
    {
        currPTT = (ptt) ? 1 : 0;

        qCDebug(runtime) << "emitting PTT changed" << QString::number(currPTT);
        emit pttChanged(currPTT);
    }

    QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqGET_PTT);
}

void FlrigRigDrv::reqCWIO_GET_WPM()
{
    FCT_IDENTIFICATION;

    if ( rigProfile.getKeySpeed )  sendXmlRpcCommand("rig.cwio_get_wpm");
}

void FlrigRigDrv::rspCWIO_GET_WPM(const QVariant &value)
{
    FCT_IDENTIFICATION;

    int keySpeed = value.toInt();

    qCDebug(runtime) << "Rig Key Speed: "<< keySpeed;
    qCDebug(runtime) << "Object Key Speed: "<< QString::number(currKeySpeed);


    if ( keySpeed != currKeySpeed )
    {
        currKeySpeed = keySpeed;
        qCDebug(runtime) << "emitting Key Speed changed" << QString::number(currKeySpeed);
        emit keySpeedChanged(currKeySpeed);
    }
    QTimer::singleShot(rigProfile.pollInterval, this, &FlrigRigDrv::reqCWIO_GET_WPM);
}

void FlrigRigDrv::resetCurrStates()
{
    // clear current values means emiting all params
    currFreq = 0.0;
    currVFO.clear();
    currRawMode.clear();
    currPWR = -1;
    currBW = -1;
    currPTT = -1;
    currKeySpeed = -1;
}

void FlrigRigDrv::handleError(const QString &category, const QString &errorMsg)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << errorMsg;

    if ( !lastErrorText.isEmpty() )
        return; // only one error message will be reported

    lastErrorText = errorMsg;
    emit errorOccurred(category, lastErrorText);
}

void FlrigRigDrv::sendXmlRpcCommand(const QString &method, const QList<QVariant> &params, bool emitError)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << method << params << emitError;

    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.writeStartDocument();

    // custom clientid header
    writer.writeDTD("<?clientid=\"qlog-client\"?>");

    writer.writeStartElement("methodCall");
    writer.writeTextElement("methodName", method);

    if (!params.isEmpty())
    {
        writer.writeStartElement("params");
        for (const QVariant &param : params)
        {
            writer.writeStartElement("param");
            writer.writeStartElement("value");
            switch ( param.type() )
            {
                case QVariant::Double:
                  writer.writeTextElement("double", QString::number(param.toDouble())); break;
                case QVariant::Int:
                  writer.writeTextElement("i4", QString::number(param.toInt())); break;
                default:
                  writer.writeTextElement("string", param.toString());
            }
            writer.writeEndElement(); // value
            writer.writeEndElement(); // param
        }
        writer.writeEndElement(); // params
    }

    writer.writeEndElement(); // methodCall
    writer.writeEndDocument();

    QNetworkRequest request(hostUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
    qCDebug(runtime) << data;
    QNetworkReply *reply = networkManager->post(request, data);

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->start(RESPONSE_TIMEOUT);
    runningTimers << timeoutTimer;

    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, timeoutTimer, emitError, method]()
    {
        qCDebug(runtime) << "Response timeout detected for" << method;
        if ( reply->isRunning() )
        {
            reply->abort();
            reply->deleteLater();
            if (emitError)
                handleError(tr("Timeout"), tr("FLRig response timeout"));
        }
        timeoutTimer->deleteLater();
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, method, timeoutTimer]()
    {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
        handleXmlRpcResponse(reply, method);
        reply->deleteLater();
    });
}

QVariantList FlrigRigDrv::parseArray(QXmlStreamReader &reader)
{
    FCT_IDENTIFICATION;

    QVariantList list;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement() && reader.name().toString() == "value")
        {
            const QVariant &val = parseSingleValue(reader);
            list << val;
        }

        if (reader.isEndElement() && reader.name().toString() == "array") break;
    }

    return list;
}

QVariant FlrigRigDrv::parseSingleValue(QXmlStreamReader &reader)
{
    FCT_IDENTIFICATION;

    while (!reader.atEnd())
    {
        reader.readNext();

        if ( reader.isStartElement())
        {
            const QString &type = reader.name().toString();

            if (type == "int" || type == "i4")
                return reader.readElementText().toInt();
            else if (type == "double")
                return reader.readElementText().toDouble();
            else if (type == "string")
                return reader.readElementText();
            else if (type == "array")
                return parseArray(reader);
            else
                return reader.readElementText();
        }

        if (reader.isCharacters())  return reader.text().toString(); // untyped

        if (reader.isEndElement() && reader.name().toString() == "value") break;
    }

    return {};
}

QVariantMap FlrigRigDrv::parseStruct(QXmlStreamReader &reader)
{
    FCT_IDENTIFICATION;

    QVariantMap map;
    QString currentName;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement())
        {
            if (reader.name().toString() == "name")
                currentName = reader.readElementText();
            else if (reader.name().toString() == "value" && !currentName.isEmpty())
            {
                map[currentName] = parseSingleValue(reader);
                currentName.clear();
            }
        }

        if ( reader.isEndElement() && reader.name().toString() == "struct") break;
    }
    return map;
}

QVariant FlrigRigDrv::parseValueFromResponse(const QByteArray &xml, bool *ok, QString &errorMessage)
{
    FCT_IDENTIFICATION;

    if (ok) *ok = false;
    errorMessage.clear();

    QXmlStreamReader reader(xml);

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement() && reader.name().toString() == "value")
        {
            if (ok) *ok = true;
            return parseSingleValue(reader);
        }

        if (reader.isStartElement() && reader.name().toString() == "fault")
        {
            while (!reader.atEnd())
            {
                reader.readNext();

                if (reader.isStartElement() && reader.name().toString() == "value")
                {
                    QVariantMap faultStruct = parseStruct(reader);
                    if ( faultStruct.contains("faultString"))
                        errorMessage = faultStruct.value("faultString").toString();
                    return faultStruct;
                }
            }
        }
    }

    if (reader.hasError()) errorMessage = reader.errorString();

    return {};
}

void FlrigRigDrv::buildModeMappingHash()
{
    FCT_IDENTIFICATION;

    /* Following Modes are used in Flrig source code
     * git grep "modes_ =" | awk '{print $1" "$4 }' | while read file var; do  fixed_var="v${var::-1}"; sed -n "/static const char \*${fixed_var}/,/};/p" "${file::-1}"; done | grep -oE '"[^"]+"' | tr -d '"' | sort  -u | paste -sd, -
     *
     * A1A,AM,AM-2,AM2.4,AM6.0,AM-D,AM-D1,AM-D2,AM-D3,AM-N,AMN,AM-syn,AM(Sync),AMW,C4FM,CW,CW2.4,CW500,CW-L,CWL,
     * CW-LSB,CW-N,CWN,CW-NR,CW-R,CW-U,CWU,CW-USB,DATA,DATA2-LSB,DATA-FM,DATA-FMN,DATA-L,DATA-LSB,DATA-R,DATA-U,
     * DATA-USB,D-FM,DFM,DIG,DIGI,DIGL,DIGU,D-LSB,DRM,DSB,D-USB,DV,DV-R,F1B,FM,FM(1),FM1,FM(2),FM2,FM-D,FM-D1,
     * FM-D2,FM-D3,FM-M,FM-N,FMN,FM-W,FMW,FSK,FSK-R,H3E,LCW,LSB,LSB-D,LSB-D1,LSB-D2,LSB-D3,NFM,PKT,PKT(FM),
     * PKT-FM,PKT(L),PKT-L,PKT-U,PSK,PSK-R,RTTY,RTTY(L),RTTY-L,RTTY-R,RTTY(U),RTTY-U,SAH,SAL,SAM,SPEC,UCW,USB,
     * USB-D,USB-D1,USB-D2,USB-D3,USER-L,USER-U,W-FM,WFM
     */

    // * Mapping from FLrig Mode to ADIF modes
    raw2ADIFModeMapping = {
        { "A1A",       { "CW",      "",     false } },
        { "AM",        { "AM",      "",     false } },
        { "AM-2",      { "AM",      "",     false } },
        { "AM2.4",     { "AM",      "",     false } },
        { "AM6.0",     { "AM",      "",     false } },
        { "AM-D",      { "AM",      "",     true  } },
        { "AM-D1",     { "AM",      "",     true  } },
        { "AM-D2",     { "AM",      "",     true  } },
        { "AM-D3",     { "AM",      "",     true  } },
        { "AM-N",      { "AM",      "",     false } },
        { "AMN",       { "AM",      "",     false } },
        { "AM-syn",    { "AM",      "",     false } },
        { "AM(Sync)",  { "AM",      "",     false } },
        { "AMW",       { "AM",      "",     false } },
        { "C4FM",      { "DIGITALVOICE",    "C4FM", true  } },
        { "CW",        { "CW",      "",     false } },
        { "CW2.4",     { "CW",      "",     false } },
        { "CW500",     { "CW",      "",     false } },
        { "CW-L",      { "CW",      "",     false } },
        { "CWL",       { "CW",      "",     false } },
        { "CW-LSB",    { "CW",      "",     false } },
        { "CW-N",      { "CW",      "",     false } },
        { "CWN",       { "CW",      "",     false } },
        { "CW-NR",     { "CW",      "",     false } },
        { "CW-R",      { "CW",      "",     false } },
        { "CW-U",      { "CW",      "",     false } },
        { "CWU",       { "CW",      "",     false } },
        { "CW-USB",    { "CW",      "",     false } },
        { "DATA",      { "SSB",     "USB",  true  } },
        { "DATA2-LSB", { "SSB",     "LSB",  true  } },
        { "DATA-FM",   { "FM",      "",     true  } },
        { "DATA-FMN",  { "FM",      "",     true  } },
        { "DATA-L",    { "SSB",     "LSB",  true  } },
        { "DATA-LSB",  { "SSB",     "LSB",  true  } },
        { "DATA-R",    { "SSB",     "LSB",  true  } },
        { "DATA-U",    { "SSB",     "USB",  true  } },
        { "DATA-USB",  { "SSB",     "USB",  true  } },
        { "D-FM",      { "FM",      "",     true  } },
        { "DFM",       { "FM",      "",     true  } },
        { "DIG",       { "SSB",     "USB",  true  } },
        { "DIGI",      { "SSB",     "USB",  true  } },
        { "DIGL",      { "SSB",     "LSB",  true  } },
        { "DIGU",      { "SSB",     "USB",  true  } },
        { "D-LSB",     { "SSB",     "LSB",  true  } },
      //{ "DRM",       { "DATA",    "DRM",  true  } },
        { "DSB",       { "SSB",   "",       false } },
        { "D-USB",     { "SSB",    "USB",   true } },
        { "DV",        { "DIGITALVOICE",    "DSTAR", true } },
        { "DV-R",      { "DIGITALVOICE",    "DSTAR", true } },
        { "F1B",       { "RTTY",    "",     true  } },
        { "FM",        { "FM",      "",     false } },
        { "FM(1)",     { "FM",      "",     false } },
        { "FM1",       { "FM",      "",     false } },
        { "FM(2)",     { "FM",      "",     false } },
        { "FM2",       { "FM",      "",     false } },
        { "FM-D",      { "FM",      "",     true  } },
        { "FM-D1",     { "FM",      "",     true  } },
        { "FM-D2",     { "FM",      "",     true  } },
        { "FM-D3",     { "FM",      "",     true  } },
        { "FM-M",      { "FM",      "",     false } },
        { "FM-N",      { "FM",      "",     false } },
        { "FMN",       { "FM",      "",     false } },
        { "FM-W",      { "FM",      "",     false } },
        { "FMW",       { "FM",      "",     false } },
        { "FSK",       { "RTTY",    "",     true  } },
        { "FSK-R",     { "RTTY",    "",     true  } },
        { "H3E",       { "AM",      "",     false } },
        { "LCW",       { "CW",      "",     false } },
        { "LSB",       { "SSB",     "LSB",  false } },
        { "LSB-D",     { "SSB",     "LSB",  true  } },
        { "LSB-D1",    { "SSB",     "LSB",  true  } },
        { "LSB-D2",    { "SSB",     "LSB",  true  } },
        { "LSB-D3",    { "SSB",     "LSB",  true  } },
        { "NFM",       { "FM",      "",     false } },
        { "PKT",       { "SSB",     "USB",  true  } },
        { "PKT(FM)",   { "FM",      "",     true  } },
        { "PKT-FM",    { "FM",      "",     true  } },
        { "PKT(L)",    { "SSB",     "LSB",  true  } },
        { "PKT-L",     { "SSB",     "LSB",  true  } },
        { "PKT-U",     { "SSB",     "USB",  true  } },
        { "PSK",       { "RTTY",    "",     true  } },
        { "PSK-R",     { "RTTY",    "",     true  } },
        { "RTTY",      { "RTTY",    "",     true  } },
        { "RTTY(L)",   { "RTTY",    "",     true  } },
        { "RTTY-L",    { "RTTY",    "",     true  } },
        { "RTTY-R",    { "RTTY",    "",     true  } },
        { "RTTY(U)",   { "RTTY",    "",     true  } },
        { "RTTY-U",    { "RTTY",    "",     true  } },
     // { "SAH",       { "DATA",    "SAH",  true  } },
     // { "SAL",       { "DATA",    "SAL",  true  } },
     // { "SAM",       { "DATA",    "SAM",  true  } },
     // { "SPEC",      { "DATA",    "SPEC", true  } },
        { "UCW",       { "CW",      "",     false } },
        { "USB",       { "SSB",     "USB",  false } },
        { "USB-D",     { "SSB",     "USB",  true  } },
        { "USB-D1",    { "SSB",     "USB",  true  } },
        { "USB-D2",    { "SSB",     "USB",  true  } },
        { "USB-D3",    { "SSB",     "USB",  true  } },
        { "USER-L",    { "SSB",     "LSB",  true  } },
        { "USER-U",    { "SSB",     "USB",  true  } },
        { "W-FM",      { "FM",      "",     false } },
        { "WFM",       { "FM",      "",     false } }
    };

}

void FlrigRigDrv::handleXmlRpcResponse(QNetworkReply *reply, const QString &method)
{
    FCT_IDENTIFICATION;

    if (reply->error() != QNetworkReply::NoError)
    {
        handleError(tr("Network Error"), reply->errorString());
        return;
    }

    bool ok = false;
    QString errorMessage;
    const QByteArray &rawResp = reply->readAll();
    QVariant respValue = parseValueFromResponse(rawResp, &ok, errorMessage);
    qCDebug(runtime) << "Method:" << method
                     << "Response:" << respValue
                     << "Raw:" << rawResp;
    if ( !ok )
    {
        handleError(tr("Network Error"), errorMessage);
        return;
    }

    FlrigRigDrv::responseHandler handler = responseHandlers.value(method);

    if ( !handler )
    {
        qCDebug(runtime) << "Handler not defined for" << method;
        return;
    }

    (this->*(handler))(respValue);
}

const QString FlrigRigDrv::getModeNormalizedText(const QString &rawMode, QString &submode) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rawMode;

    const RigMode &modeInfo = raw2ADIFModeMapping.value(rawMode);
    qCDebug(runtime) << "Result" << modeInfo.mode << modeInfo.submode << modeInfo.digiMode;
    submode = modeInfo.submode;
    return modeInfo.mode;
}

const QString FlrigRigDrv::mode2RawMode(const QString &mode, const QString &submode, bool digiVariant) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << submode << digiVariant;

    // find a suitable mode from the connected Rig modes
    for ( auto it = rigAvailableModes.cbegin(); it != rigAvailableModes.cend(); it++ )
    {
        qCDebug(runtime) << "Key:" << it.key()
                         << "Values:" << it->mode << it->submode << it->digiMode;
        if ( it->mode == mode && it->submode == submode && it->digiMode == digiVariant )
            return it.key();
    }

    return QString();
}
