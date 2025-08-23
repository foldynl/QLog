// RigctldManager.cpp
#include "rigctldmanager.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QThread> // for msleep in the polling loop
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.rig.rigctldmanager");

static QString rigLogPath() {
    FCT_IDENTIFICATION;

    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base + "/logs");
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return base + "/logs/rigctld_" + stamp + ".log";
}

void RigctldManager::appendToRigctldLog(const QString& s) {
    FCT_IDENTIFICATION;

    if (!debugLogs_) return;
    if (logPath_.isEmpty()) {
        logPath_ = rigLogPath();
        proc_.setStandardOutputFile(logPath_, QIODevice::Append);
        proc_.setStandardErrorFile(logPath_, QIODevice::Append);
    }
    QFile f(logPath_);
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << s;
    }
}

bool RigctldManager::isTcpPortOpen(const QHostAddress& host, quint16 port, int timeoutMs) {
    FCT_IDENTIFICATION;

    QTcpSocket s;
    s.connectToHost(host, port);
    return s.waitForConnected(timeoutMs);
}

quint16 RigctldManager::findFreePort(quint16 preferred) {
    FCT_IDENTIFICATION;

    QTcpServer s;
    if (preferred && s.listen(QHostAddress::LocalHost, preferred)) return preferred;
    if (s.listen(QHostAddress::LocalHost, 0)) return s.serverPort();
    return 0;
}

RigctldManager::RigctldManager(QObject* parent) : QObject(parent) {
    FCT_IDENTIFICATION;

    proc_.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&proc_, &QProcess::readyReadStandardOutput, this, [this]{
        const auto chunk = QString::fromLocal8Bit(proc_.readAllStandardOutput());
        emit rigctldOutput(chunk);
        appendToRigctldLog(chunk);
    });
    connect(&proc_, &QProcess::readyReadStandardError, this, [this]{
        const auto chunk = QString::fromLocal8Bit(proc_.readAllStandardError());
        emit rigctldError(chunk);
        appendToRigctldLog(chunk);
    });

    connect(&proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e){
        appendToRigctldLog(QStringLiteral("QProcess error: %1\n").arg(int(e)));
    });
    connect(&proc_, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus st){
                appendToRigctldLog(QStringLiteral("rigctld finished: exitCode=%1, exitStatus=%2\n")
                                       .arg(code).arg(st==QProcess::NormalExit ? "Normal" : "Crashed"));
                emit rigctldExited(code, st);
            });
}

bool RigctldManager::startFromProfile(const RigProfile& prof,
                                      quint16 preferPort,
                                      QString rigctldPath,
                                      QString* err) {
    FCT_IDENTIFICATION;

    if (isRunning()) return true;

    port_ = findFreePort(preferPort ? preferPort : 4532);
    if (!port_) { if (err) *err = "No free TCP port for rigctld."; return false; }

    if (isTcpPortOpen(QHostAddress::LocalHost, port_)) {
        appendToRigctldLog(QStringLiteral("Reusing existing server on %1\n").arg(port_));
        return true;
    }

    QStringList args = buildRigctldArgs(prof, port_);
    if (debugLogs_) { args << "-vvvv"; }

    appendToRigctldLog(QStringLiteral("Launching: %1 %2\n")
                           .arg(rigctldPath, args.join(' ')));

    proc_.setProgram(rigctldPath);
    proc_.setArguments(args);

    proc_.start();
    if (!proc_.waitForStarted(5000)) {
        if (err) *err = "Failed to start rigctld (path/signature issue?).";
        appendToRigctldLog(*err + "\n");
        return false;
    }

    const int maxWaitMs = 5000, stepMs = 100;
    int waited = 0;
    while (waited < maxWaitMs) {
        if (isTcpPortOpen(QHostAddress::LocalHost, port_)) {
            appendToRigctldLog(QStringLiteral("rigctld is listening on 127.0.0.1:%1\n").arg(port_));
            return true;
        }
        QThread::msleep(stepMs);
        waited += stepMs;

        if (proc_.state() != QProcess::Running) {
            if (err) *err = "rigctld exited before it became ready.";
            appendToRigctldLog(*err + "\n");
            return false;
        }
    }

    if (err) *err = "rigctld did not open its TCP port in time.";
    appendToRigctldLog(*err + "\n");
    return false;
}

void RigctldManager::stop() {
    FCT_IDENTIFICATION;

    if (!isRunning()) return;
    proc_.terminate();
    if (!proc_.waitForFinished(2000)) {
        proc_.kill();
        proc_.waitForFinished(2000);
    }
}

QString RigctldManager::normalizeParity(const QString& p) {
    FCT_IDENTIFICATION;

    const QString v = p.trimmed().toLower();
    if (v == "even") return "Even";
    if (v == "odd")  return "Odd";
    return "None";
}

QString RigctldManager::normalizeHandshake(const QString& s) {
    FCT_IDENTIFICATION;

    const QString v = s.trimmed().toLower();
    if (v == "hardware" || v == "rtscts") return "Hardware";
    if (v == "software" || v == "xonxoff") return "Software";
    return "None";
}

QStringList RigctldManager::buildRigctldArgs(const RigProfile& rigProfile, quint16 tcpPortToServeOn) {
    FCT_IDENTIFICATION;

    QStringList args;

    RigProfile::rigPortType portType = rigProfile.getPortType();

    args << "-m" << QString::number(rigProfile.model);
    args << "-p" << QString::number(tcpPortToServeOn);


    if (portType == RigProfile::NETWORK_ATTACHED)
    {
        args << "-r" << QString("%1:%2").arg(rigProfile.hostname).arg(rigProfile.netport);
    }
    else if (portType == RigProfile::SERIAL_ATTACHED)
    {
        args << "-r" << rigProfile.portPath.toLocal8Bit().constData();
        args << "-s" << QString::number(rigProfile.baudrate);
        args << "-C" << QString("stop_bits=%1").arg(rigProfile.stopbits);
        args << "-C" << QString("data_bits=%1").arg(rigProfile.databits);
        args << "-C" << QString("serial_handshake=%1").arg(normalizeHandshake(rigProfile.flowcontrol));
        args << "-C" << QString("parity=%1").arg(rigProfile.parity);

        if ( !rigProfile.pttPortPath.isEmpty() )
        {
            args << "-p" << rigProfile.pttPortPath.toLocal8Bit().constData();
        }
    }

    return args;
}

QString RigctldManager::findRigctldExecutable() {
    FCT_IDENTIFICATION;

#ifdef Q_OS_MAC
    // bundled next to the main binary
    const QString bundled = QCoreApplication::applicationDirPath() + "/rigctld";
#elif defined(Q_OS_WIN)
    const QString bundled = QCoreApplication::applicationDirPath() + "/rigctld.exe";
#else
    const QString bundled = QCoreApplication::applicationDirPath() + "/rigctld";
#endif
    if (QFileInfo::exists(bundled) && QFileInfo(bundled).isExecutable())
        return bundled;

    // fallback: search PATH
    const QString rigctld = QStandardPaths::findExecutable(
#ifdef Q_OS_WIN
        "rigctld.exe"
#else
        "rigctld"
#endif
        );
    return rigctld; // may be empty if not found
}

