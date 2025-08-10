#include "dxlabserver.h"
#include <QRegularExpression>
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.dxlabcommanderserver");

static inline bool eq(const QString &a, const char *b)
{ return a.compare(QLatin1String(b), Qt::CaseInsensitive) == 0; }

DxlabServer::DxlabServer(Rig *rig, QObject *parent)
    : QObject(parent), _rig(rig)
{
    FCT_IDENTIFICATION;
    buildModeMappingHash();
    connect(&_srv, &QTcpServer::newConnection, this, &DxlabServer::onNewConn);
}

DxlabServer::~DxlabServer() { stop(); }

bool DxlabServer::start(quint16 port) {
    FCT_IDENTIFICATION;

    if (_srv.isListening()) stop();
    if (!_srv.listen(QHostAddress::Any, port)) {
        emit warn(QStringLiteral("Commander TCP listen failed on %1: %2")
                      .arg(port).arg(_srv.errorString()));
        return false;
    }
    emit info(QStringLiteral("Commander TCP listening on %1").arg(port));
    return true;
}

void DxlabServer::stop() {
    FCT_IDENTIFICATION;

    for (QTcpSocket *s : _srv.findChildren<QTcpSocket*>()) {
        s->disconnect(this); s->close(); s->deleteLater();
    }
    _srv.close();
}

void DxlabServer::onNewConn() {
    FCT_IDENTIFICATION;

    while (_srv.hasPendingConnections()) {
        auto *s = _srv.nextPendingConnection();
        auto *st = new ClientState();
        st->flushTimer.setSingleShot(true);
        st->flushTimer.setInterval(50); // ms; tune if needed
        // When timer fires, finalize any pending command
        connect(&st->flushTimer, &QTimer::timeout, this, [this, s, st](){
            if (!st->inflight.name.isEmpty()) {
                processInflight(s, st->inflight);
                st->inflight = Inflight{};
            }
        });
        s->setProperty("state", QVariant::fromValue(reinterpret_cast<quintptr>(st)));

        connect(s, &QTcpSocket::readyRead, this, &DxlabServer::onReady);
        connect(s, &QTcpSocket::disconnected, this, &DxlabServer::onGone);
        emit info(QStringLiteral("Commander client %1 connected").arg(s->peerAddress().toString()));
    }
}

void DxlabServer::onGone() {
    FCT_IDENTIFICATION;

    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    if (auto p = s->property("state"); p.isValid())
        delete reinterpret_cast<ClientState*>(p.value<quintptr>());
    s->deleteLater();
}

void DxlabServer::onReady() {
    FCT_IDENTIFICATION;

    auto *s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    auto p = s->property("state"); if (!p.isValid()) return;
    auto *st = reinterpret_cast<ClientState*>(p.value<quintptr>());

    st->rx += s->readAll();

    Token t;
    while (extractOneToken(st->rx, t)) handleToken(s, *st, t);
}

bool DxlabServer::extractOneToken(QByteArray &buf, Token &tok) {
    FCT_IDENTIFICATION;

    tok = Token{};
    int lt = buf.indexOf('<'); if (lt < 0) return false;
    if (lt > 0) buf.remove(0, lt);
    int colon = buf.indexOf(':', lt+1);
    int gt    = buf.indexOf('>', colon+1);
    if (colon < 0 || gt < 0) return false;
    QByteArray nameBA = buf.mid(lt+1, colon-(lt+1));
    QByteArray lenBA  = buf.mid(colon+1, gt-(colon+1));
    bool ok=false; int n = lenBA.toInt(&ok);
    if (!ok || n < 0) { buf.remove(0, gt+1); return true; }
    int need = gt + 1 + n;
    if (buf.size() < need) return false;
    tok.name = QString::fromLatin1(nameBA);
    tok.len  = n;
    tok.val  = buf.mid(gt+1, n);
    buf.remove(0, need);
    return true;
}

void DxlabServer::handleToken(QTcpSocket *s, ClientState &st, const Token &t) {
    FCT_IDENTIFICATION;

    // New command starts a new in-flight; if something was pending, finalize it first

    if (t.name.compare("command", Qt::CaseInsensitive) == 0) {
        if (!st.inflight.name.isEmpty()) {
            processInflight(s, st.inflight);
            st.inflight = Inflight{};
        }
        st.inflight.name = QString::fromLatin1(t.val);
        st.inflight.params.clear();
        st.inflight.sawParameters = false;
        st.flushTimer.start(); // safety for commands with no parameters
        return;
    }

    if (t.name.compare("parameters", Qt::CaseInsensitive) == 0) {
        st.inflight.sawParameters = true;
        QByteArray sub = t.val;
        Token p;
        while (extractOneToken(sub, p)) {
            st.inflight.params.insert(p.name, p.val);
        }
        st.flushTimer.stop();
        // We now have all parameters: execute exactly once
        if (!st.inflight.name.isEmpty()) {
            processInflight(s, st.inflight);
            st.inflight = Inflight{};
        }
        return;
    }

    // Loose parameter outside <parameters:...> — accept and accumulate
    if (!st.inflight.name.isEmpty()) {
        st.inflight.params.insert(t.name, t.val);
        // We'll execute on next <command> or when the timer fires
        return;
    }
    qDebug() << "Unprocessed" << t.name << t.val;
}



void DxlabServer::sendToken(QTcpSocket *s, const QString &name, const QByteArray &val) {
    FCT_IDENTIFICATION;

    QByteArray header = "<" + name.toLatin1() + ":" + QByteArray::number(val.size()) + ">";
    s->write(header); s->write(val); s->flush();
}

// ===== Replies =====
void DxlabServer::replyRxFreqStd(QTcpSocket *s) {
    FCT_IDENTIFICATION;

    double rxHz = currFrequency * 1000;
    QByteArray tok = QString::number(rxHz,'f',3).toUtf8();
    sendToken(s, "CmdFreq", tok);

}

void DxlabServer::replyTxFreqStd(QTcpSocket *s) {
    FCT_IDENTIFICATION;

    double rxHz = currFrequency * 1000;
    QByteArray tok = QString::number(rxHz,'f',3).toUtf8();
    sendToken(s, "CmdFreq", tok);
}

void DxlabServer::replyMode(QTcpSocket *s) {
    FCT_IDENTIFICATION;

    QString mode;
    RigMode rigmode = raw2ADIFModeMapping.value(rawModeText);

    if(rigmode.mode == "SSB" && rigmode.submode == "USB" && rigmode.digiMode) { mode = "Data-U";}
    if(rigmode.mode == "SSB" && rigmode.submode == "LSB" && rigmode.digiMode) { mode = "Data-L";}
    if(rigmode.mode == "SSB" && rigmode.submode == "USB" && !rigmode.digiMode) { mode = "USB";}
    if(rigmode.mode == "SSB" && rigmode.submode == "LSB" && !rigmode.digiMode) { mode = "LSB";}
    if(rigmode.mode == "CW" ) { mode = "CW";}
    if(rigmode.mode == "AM" ) { mode = "AM";}
    if(rigmode.mode == "FM" ) { mode = "FM";}

    sendToken(s, "CmdMode", mode.toUtf8()); // must be spec names
}

void DxlabServer::replySplit(QTcpSocket *s) {
    FCT_IDENTIFICATION;

    sendToken(s, "CmdSplit", "OFF");
}

void DxlabServer::replyPTT(QTcpSocket *s) {
    FCT_IDENTIFICATION;

    sendToken(s, "CmdTX", QByteArray(currPTT ? "ON" : "OFF"));
}

bool DxlabServer::parseKhzToHz(const QByteArray &s, qint64 &outHz) {
    FCT_IDENTIFICATION;

    QByteArray t = s; t.replace(",", "");
    bool ok=false; double khz = t.toDouble(&ok);
    if (!ok) return false;
    outHz = static_cast<qint64>(khz) * 1000;
    return true;
}

static const char* kModes[] = {"AM","CW","CW-R","DATA-L","DATA-U","FM","LSB","USB","RTTY","RTTY-R","WBFM"};
bool DxlabServer::isValidMode(const QString &m) {
    FCT_IDENTIFICATION;

    for (auto *x: kModes) if (m.compare(QLatin1String(x), Qt::CaseInsensitive)==0) return true;
    return false;
}

QString DxlabServer::normMode(const QString &m) {
    FCT_IDENTIFICATION;

    const QString u = m.trimmed().toUpper();
    for (auto *x: kModes) if (u == QLatin1String(x)) return QLatin1String(x);
    return u;
}

void DxlabServer::updateFrequency(VFOID vfoid, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid)

    currFrequency = vfoFreq;
    currRIT = ritFreq;
    currXIT = xitFreq;
}

void DxlabServer::updateMode(VFOID vfoid, const QString &rawMode, const QString &mode,
                           const QString &submode, qint32 pb)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(submode)
    Q_UNUSED(vfoid)

    currMode = mode;
    currSubMode = submode;
    rawModeText = rawMode;
    currPBWidth = pb;
}

void DxlabServer::updatePWR(VFOID vfoid, double pwr)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid)

    currPower = pwr;
}

void DxlabServer::updateVFO(VFOID vfoid, const QString &vfo)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid)

    currVFO = vfo;
}

void DxlabServer::updateXIT(VFOID vfoid, double xit)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid);

    currXIT = xit;
}

void DxlabServer::updateRIT(VFOID vfoid, double rit)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid);

    currXIT = rit;
}

void DxlabServer::updatePTT(VFOID vfoid, bool ptt)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(vfoid);

    currPTT = ptt;
}

void DxlabServer::rigConnected()
{
    FCT_IDENTIFICATION;

    rigOnline = true;
}

void DxlabServer::rigDisconnected()
{
    FCT_IDENTIFICATION;

    rigOnline = false;
}

void DxlabServer::processInflight(QTcpSocket *s, Inflight &in) {
    FCT_IDENTIFICATION;

    const QString cmd = in.name;

    if (!rigOnline) return;

    if (eq(cmd, "CmdGetFreq") || eq(cmd, "CmdSendFreq"))         { replyRxFreqStd(s);   QThread::msleep(25); return; }
    if (eq(cmd, "CmdGetTXFreq") || eq(cmd, "CmdSendTXFreq"))     { replyTxFreqStd(s); QThread::msleep(25);  return; }
    if (eq(cmd, "CmdSendMode"))                                  { replyMode(s); QThread::msleep(25);       return; }
    if (eq(cmd, "CmdSendSplit"))                                 { replySplit(s); QThread::msleep(25);      return; }
    if (eq(cmd, "CmdSendTX"))                                    { replyPTT(s);  QThread::msleep(25);       return; }

    if (eq(cmd, "CmdTX")) { _rig->setPTT(true); QThread::msleep(25);return; }
    if (eq(cmd, "CmdRX")) { _rig->setPTT(false); QThread::msleep(25); return; }

    if (eq(cmd, "CmdSetFreqMode")) {
        qint64 hz{}; const bool haveHz = parseKhzToHz(in.params.value("xcvrfreq"), hz);
        const QString mode = normMode(QString::fromLatin1(in.params.value("xcvrmode")));
        const bool haveMode = isValidMode(mode);
        const bool preserve = QString::fromLatin1(in.params.value("preservesplitanddual")).trimmed().compare("Y", Qt::CaseInsensitive)==0;
        if (haveHz)   { _rig->setFrequency(hz);  currFrequency = hz / 1e6; }
        if (haveMode)
        {
            /*_rig->setModeByName(mode);*/
            _qsxMode = mode;
            rawModeText = mode;
            if(rawModeText == "DATA-U") { _rig->setMode("SSB","USB",true);}
            if(rawModeText == "DATA-L") { _rig->setMode("SSB","LSB",true);}
            if(rawModeText == "USB") { _rig->setMode("SSB","USB",false);}
            if(rawModeText == "LSB") { _rig->setMode("SSB","LSB",false);}
            if(rawModeText == "CW") { _rig->setMode("CW","",false);}
            if(rawModeText == "AM") { _rig->setMode("AM","",false);}
            if(rawModeText == "FM") { _rig->setMode("FM","",false);}
            if(rawModeText == "RTTY") { _rig->setMode("SSB","USB",true);}
            if(rawModeText == "RTTY-R") { _rig->setMode("SSB","LSB",true);}

            qWarning() << rawModeText;
        }
        if (!preserve) { /* _rig->setSplit(false); */ }
        QThread::msleep(25);

        return;
    }

    if (eq(cmd, "CmdSetFreq")) {
        qint64 hz{}; if (parseKhzToHz(in.params.value("xcvrfreq"), hz)) {
            _rig->setFrequency(hz);
        }
        QThread::msleep(25);
        return;
    }

    if (eq(cmd, "CmdSetTXFreq")) {
        qint64 hz{}; if (parseKhzToHz(in.params.value("xcvrfreq"), hz)) {
            /* if you support a TX VFO setter, call it here */
        }
        QThread::msleep(25);
        return;
    }

    if (eq(cmd, "CmdSetMode")) {
        const QString m = normMode(QString::fromLatin1(in.params.value("1"))); // spec uses <1:n>MODE
        if (isValidMode(m)) {

            /* _rig->setModeByName(m); */ rawModeText = m;
            if(rawModeText == "DATA-U") { _rig->setMode("SSB","USB",true);}
            if(rawModeText == "DATA-L") { _rig->setMode("SSB","LSB",true);}
            if(rawModeText == "USB") { _rig->setMode("SSB","USB",false);}
            if(rawModeText == "LSB") { _rig->setMode("SSB","LSB",false);}
            if(rawModeText == "CW") { _rig->setMode("CW","",false);}
            if(rawModeText == "AM") { _rig->setMode("AM","",false);}
            if(rawModeText == "FM") { _rig->setMode("FM","",false);}
            if(rawModeText == "RTTY") { _rig->setMode("SSB","USB",true);}
            if(rawModeText == "RTTY-R") { _rig->setMode("SSB","LSB",true);}
        }
        QThread::msleep(25);
        return;
    }

    if (eq(cmd, "CmdSplit")) {
        const QString v = QString::fromLatin1(in.params.value("1")).trimmed();
       //const bool on = (v.compare("on", Qt::CaseInsensitive)==0 || v=="1");
        /* _rig->setSplit(on); */  // you can track a local bool if needed
        QThread::msleep(25);
        return;
    }

    if (eq(cmd, "CmdQSXSplit")) {
        qint64 hz{}; if (parseKhzToHz(in.params.value("xcvrfreq"), hz)) {
            /* enable split and set TX freq if your rig supports it */
        }
        const bool suppressMode = QString::fromLatin1(in.params.value("SuppressModeChange")).trimmed().compare("Y", Qt::CaseInsensitive)==0;
        if (!suppressMode && isValidMode(_qsxMode)) {
            /* _rig->setModeByName(_qsxMode); */
        }
        QThread::msleep(25);
        return;
    }
    qDebug() << "unknown" << in.name << in.params << in.sawParameters;
}

void DxlabServer::buildModeMappingHash()
{
    FCT_IDENTIFICATION;

    raw2ADIFModeMapping = {
        { "CW",        { "CW",      "",     false } },
        { "AM",        { "AM",      "",     false } },
        { "CWR",       { "CW",      "",     false } },
        { "PKTUSB",    { "SSB",     "USB",  true  } },
        { "PKTLSB",    { "SSB",     "LSB",  true  } },
        { "FM",        { "FM",      "",     false } },
        { "FMN",       { "FM",      "",     false } },
        { "LSB",       { "SSB",     "LSB",  false } },
        { "RTTY",      { "RTTY",    "",     true  } },
        { "RTTYR",     { "RTTY",    "",     true  } },
        { "USB",       { "SSB",     "USB",  false } },
        { "WFM",       { "FM",      "",     false } }
    };

}
