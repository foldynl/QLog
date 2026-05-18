#include <QStandardPaths>
#include <QThread>
#include <QTcpSocket>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>

#include "RigctldManager.h"
#include "core/debug.h"
#include "data/SerialPort.h"

MODULE_IDENTIFICATION("qlog.rig.rigctldmanager");

RigctldManager::RigctldManager(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
}

RigctldManager::~RigctldManager()
{
    FCT_IDENTIFICATION;

    if ( thread() == QThread::currentThread() )
    {
        stop();
    }
    else if ( rigctldProcess )
    {
        qCWarning(runtime) << "Skipping rigctld shutdown from non-owner thread";
    }
}

bool RigctldManager::start(const RigProfile &profile)
{
    FCT_IDENTIFICATION;

    if ( rigctldProcess && rigctldProcess->state() != QProcess::NotRunning)
    {
        qCWarning(runtime) << "rigctld is already running";
        return false;
    }

    // Find rigctld executable
    QString rigctldPath = profile.rigctldPath;
    if ( rigctldPath.isEmpty() )
    {
        rigctldPath = findRigctldPath();

        if ( rigctldPath.isEmpty() )
        {
            qCWarning(runtime) << "rigctld executable not found";
#ifdef QLOG_FLATPAK
            emit errorOccurred(tr("rigctld executable not found in /app/bin/. This should not happen in Flatpak build."));
#else
            emit errorOccurred(tr("rigctld executable not found. Please install Hamlib or specify the path in Advanced settings."));
#endif
            return false;
        }
    }

    qCDebug(runtime) << "Using rigctld at:" << rigctldPath;

    // Check major version compatibility
    // Hamlib 3 vs 4 changed RIG model IDs and they are not compatible
    const RigctldVersion rigctldVersion = getVersion(rigctldPath);

    if ( rigctldVersion.isValid()
          && ((rigctldVersion.major >= 4 && HAMLIBVERSION_MAJOR <= 3)
               || (rigctldVersion.major <= 3 && HAMLIBVERSION_MAJOR >= 4)))
    {
        qCDebug(runtime) << "Hamlib major version mismatch: QLog compiled with"
                           << HAMLIBVERSION_MAJOR << "but rigctld is" << rigctldVersion.major;
        emit errorOccurred(tr("Hamlib major version mismatch: QLog was compiled with Hamlib %1 "
                              "but rigctld reports version %2.%3.%4. "
                              "Rig model IDs are incompatible between major versions.")
                           .arg(HAMLIBVERSION_MAJOR)
                           .arg(rigctldVersion.major)
                           .arg(rigctldVersion.minor)
                           .arg(rigctldVersion.patch));
        return false;
    }

    currentPort = profile.rigctldPort;

    // Check if port is already in use
    {
        QTcpSocket testSocket;
        testSocket.connectToHost("127.0.0.1", currentPort);
        if ( testSocket.waitForConnected(500) )
        {
            testSocket.disconnectFromHost();
            qCWarning(runtime) << "Port" << currentPort << "is already in use";
            emit errorOccurred(tr("Port %1 is already in use. "
                                  "Another rigctld or application may be running on this port.")
                               .arg(currentPort));
            return false;
        }
        testSocket.abort();
    }

    // Create process
    rigctldProcess = new QProcess(this);
    rigctldProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(rigctldProcess, &QProcess::started, this, &RigctldManager::onProcessStarted);
    connect(rigctldProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &RigctldManager::onProcessFinished);
    connect(rigctldProcess, &QProcess::errorOccurred, this, &RigctldManager::onProcessError);
    connect(rigctldProcess, &QProcess::readyReadStandardOutput, this, &RigctldManager::onReadyReadStdout);
    connect(rigctldProcess, &QProcess::readyReadStandardError, this, &RigctldManager::onReadyReadStderr);

    // exec arguments
    const QStringList args = buildArguments(profile);
    qCDebug(runtime) << "Starting rigctld with args:" << args;

    rigctldProcess->start(rigctldPath, args);

    if ( !rigctldProcess->waitForStarted(5000) )
    {
        qCDebug(runtime) << "Failed to start rigctld";
        // onProcessError already emits errorOccurred via QProcess::errorOccurred signal
        delete rigctldProcess;
        rigctldProcess = nullptr;
        return false;
    }

    // Wait for rigctld to be ready (accepting connections)
    if ( !waitForRigctldReady() )
    {
        qCDebug(runtime) << "rigctld not responding on port" << currentPort;
        stop();
        emit errorOccurred(tr("rigctld started but not responding on port %1.").arg(currentPort));
        return false;
    }

    qCDebug(runtime) << "rigctld started successfully on port" << currentPort;
    emit started();
    return true;
}

void RigctldManager::stop()
{
    FCT_IDENTIFICATION;

    if ( !rigctldProcess || stoppingInProgress ) return;

    stoppingInProgress = true;

    QProcess *process = rigctldProcess;
    rigctldProcess = nullptr;

    // During a normal stop, stop receiving this process's signals here. Leave
    // any other QProcess connections alone.
    disconnect(process, nullptr, this, nullptr);

    if ( process->state() != QProcess::NotRunning )
    {
        qCDebug(runtime) << "Stopping rigctld";
        process->terminate();
        if ( !process->waitForFinished(3000) )
        {
            qCWarning(runtime) << "rigctld did not terminate gracefully, killing";
            process->kill();
            process->waitForFinished(1000);
        }
    }

    delete process;
    stoppingInProgress = false;

    emit stopped();
}

bool RigctldManager::isRunning() const
{
    return rigctldProcess && rigctldProcess->state() == QProcess::Running;
}

QString RigctldManager::findRigctldPath()
{
    FCT_IDENTIFICATION;

    // First priority: Check application directory (bundled rigctld)
#ifdef Q_OS_WIN
    const QString appDirPath = QCoreApplication::applicationDirPath() + "/rigctld.exe";
#else
    const QString appDirPath = QCoreApplication::applicationDirPath() + "/rigctld";
#endif
    if ( QFile::exists(appDirPath) )
    {
        qCDebug(runtime) << "Found bundled rigctld at:" << appDirPath;
        return appDirPath;
    }

    // Second priority: Platform-specific paths
    const QStringList platformPaths =
    {
#ifdef Q_OS_WIN
        "C:/Program Files/Hamlib/bin/rigctld.exe",
        "C:/Program Files (x86)/Hamlib/bin/rigctld.exe",
        QDir::homePath() + "/AppData/Local/Hamlib/bin/rigctld.exe"
#endif
#ifdef Q_OS_MACOS
        "/opt/homebrew/bin/rigctld",
        "/usr/local/bin/rigctld",
        "/opt/local/bin/rigctld"
#endif
#ifdef QLOG_FLATPAK
        "/app/bin/rigctld",  // Flatpak path
#else
        "/usr/bin/rigctld",
        "/usr/local/bin/rigctld",
        "/opt/hamlib/bin/rigctld"
#endif
    };

    for ( const QString &p : platformPaths )
    {
        if ( QFile::exists(p) )
        {
            qCDebug(runtime) << "Found rigctld at:" << p;
            return p;
        }
    }

    // Last resort: Try $PATH
    const QString path = QStandardPaths::findExecutable("rigctld");
    if ( !path.isEmpty() )
    {
        qCDebug(runtime) << "Found rigctld in PATH:" << path;
        return path;
    }

    qCWarning(runtime) << "rigctld not found";
    return QString();
}

RigctldVersion RigctldManager::getVersion(const QString &rigctldPath)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << rigctldPath;

    RigctldVersion version;

    QString path = rigctldPath;
    if ( path.isEmpty() )
        path = findRigctldPath();

    if ( path.isEmpty() )
    {
        qCDebug(runtime) << "rigctld not found";
        return version;
    }

    QProcess process;
    process.start(path, QStringList("--version"));

    if ( !process.waitForFinished(2000) )
    {
        qCDebug(runtime) << "rigctld --version timed out";
        return version;
    }

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    qCDebug(runtime) << "rigctld version output:" << output;

    // "rigctld Hamlib 4.5.5"
    QRegularExpression re("rigct\\S*\\s+\\S+\\s+(\\d+)\\.(\\d+)\\.(\\d+)");
    QRegularExpressionMatch match = re.match(output); // clazy:exclude=use-static-qregularexpression

    if ( match.hasMatch() )
    {
        version.major = match.captured(1).toInt();
        version.minor = match.captured(2).toInt();
        version.patch = match.captured(3).toInt();
        qCDebug(runtime) << "Parsed version:" << version.major << version.minor << version.patch;
    }
    else
    {
        qCDebug(runtime) << "Failed to parse version from output:" << output;
    }

    return version;
}

bool RigctldManager::waitForRigctldReady(int timeoutMs)
{
    FCT_IDENTIFICATION;

    QTcpSocket testSocket;
    int elapsed = 0;
    const int checkInterval = 100;

    // test connection
    while ( elapsed < timeoutMs )
    {
        testSocket.connectToHost("127.0.0.1", currentPort);
        if ( testSocket.waitForConnected(500) )
        {
            testSocket.disconnectFromHost();
            qCDebug(runtime) << "rigctld is ready after" << elapsed << "ms";
            return true;
        }
        testSocket.abort();

        // Check if process is still running
        if ( rigctldProcess && rigctldProcess->state() != QProcess::Running )
        {
            qCWarning(runtime) << "rigctld process terminated unexpectedly";
            return false;
        }

        QThread::msleep(checkInterval);
        elapsed += checkInterval;
    }

    return false;
}

QStringList RigctldManager::buildArguments(const RigProfile &profile) const
{
    FCT_IDENTIFICATION;

    QStringList args;

    // Model
    args << "-m" << QString::number(profile.model);

    // Listen Port
    args << "-t" << QString::number(profile.rigctldPort);

    // Serial port settings
    if ( profile.getPortType() == RigProfile::SERIAL_ATTACHED )
    {
        args << "-r" << profile.portPath;

        // BandRate
        if ( profile.baudrate > 0 ) args << "-s" << QString::number(profile.baudrate);

        // Data bits
        if ( profile.databits > 0 ) args << "-C" << QString("data_bits=%1").arg(profile.databits);

        // Stop bits
        if ( profile.stopbits > 0 ) args << "-C" << QString("stop_bits=%1").arg(static_cast<int>(profile.stopbits));

        // Parity
        if ( !profile.parity.isEmpty() && profile.parity.compare(SerialPort::SERIAL_PARITY_NO, Qt::CaseInsensitive) )
        {
            QString parity = profile.parity.toLower();
            if      ( parity == SerialPort::SERIAL_PARITY_EVEN ) parity = "E";
            else if ( parity == SerialPort::SERIAL_PARITY_ODD )  parity = "O";
            else parity = "N";
            args << "-C" << QString("serial_parity=%1").arg(parity);
        }

        // Flow control
        if ( !profile.flowcontrol.isEmpty() && profile.flowcontrol.compare(SerialPort::SERIAL_FLOWCONTROL_NONE, Qt::CaseInsensitive) )
        {
            QString flow = profile.flowcontrol.toLower();
            if ( flow == SerialPort::SERIAL_FLOWCONTROL_HARDWARE )      flow = "Hardware";
            else if ( flow == SerialPort::SERIAL_FLOWCONTROL_SOFTWARE ) flow = "XONXOFF";
            args << "-C" << QString("serial_handshake=%1").arg(flow);
        }

        // CIV address for Icom
        if (profile.civAddr >= 0) args << "-C" << QString("civaddr=%1").arg(profile.civAddr, 2, 16, QChar('0'));

        // DTR signal
        if ( !profile.dtr.isEmpty() && profile.dtr.compare(SerialPort::SERIAL_SIGNAL_NONE, Qt::CaseInsensitive) )
        {
            QString dtr = profile.dtr.toLower();
            if ( dtr == SerialPort::SERIAL_SIGNAL_HIGH )     dtr = "ON";
            else if ( dtr == SerialPort::SERIAL_SIGNAL_LOW ) dtr = "OFF";
            args << "-C" << QString("dtr_state=%1").arg(dtr);
        }

        // RTS signal
        if ( !profile.rts.isEmpty() && profile.rts.compare(SerialPort::SERIAL_SIGNAL_NONE, Qt::CaseInsensitive) )
        {
            QString rts = profile.rts.toLower();
            if ( rts == SerialPort::SERIAL_SIGNAL_HIGH )     rts = "ON";
            else if ( rts == SerialPort::SERIAL_SIGNAL_LOW ) rts = "OFF";
            args << "-C" << QString("rts_state=%1").arg(rts);
        }
    }

    // Additional user-specified arguments
    if ( !profile.rigctldArgs.isEmpty() )
    {
        // Split by whitespace, respecting quotes
        QStringList extraArgs = profile.rigctldArgs.split(QRegularExpression("\\s+"), // clazy:exclude=use-static-qregularexpression
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                                                          Qt::SkipEmptyParts);
#else
                                                          QString::SkipEmptyParts);
#endif

        args << extraArgs;
    }

    qCDebug(runtime) << args;

    return args;
}

void RigctldManager::onProcessStarted()
{
    FCT_IDENTIFICATION;
    qCDebug(runtime) << "rigctld process started";
}

void RigctldManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    FCT_IDENTIFICATION;
    qCDebug(runtime) << "rigctld process finished with exit code" << exitCode
                     << "status" << exitStatus;

    if ( stoppingInProgress )
        return;

    emit stopped();
}

void RigctldManager::onProcessError(QProcess::ProcessError error)
{
    FCT_IDENTIFICATION;

    QString errorStr;
    switch ( error )
    {
    case QProcess::FailedToStart:
    {
        const QString program = rigctldProcess ? rigctldProcess->program() : QString();
        const QStringList args = rigctldProcess ? rigctldProcess->arguments() : QStringList();
        errorStr = tr("Failed to start rigctld: %1 %2").arg(program, args.join(" "));
        break;
    }
    case QProcess::Crashed:
        errorStr = tr("rigctld crashed.");
        break;
    case QProcess::Timedout:
        errorStr = tr("rigctld timed out.");
        break;
    case QProcess::WriteError:
        errorStr = tr("Write error with rigctld.");
        break;
    case QProcess::ReadError:
        errorStr = tr("Read error with rigctld.");
        break;
    default:
        errorStr = tr("Unknown rigctld error.");
        break;
    }

    qCDebug(runtime) << "rigctld process error:" << errorStr;

    if ( !stoppingInProgress )
        emit errorOccurred(errorStr);
}

void RigctldManager::onReadyReadStdout()
{
    if (!rigctldProcess)
        return;

    const QByteArray data = rigctldProcess->readAllStandardOutput();
    if ( !data.isEmpty() )
    {
        // Split by lines and log each
        const QList<QByteArray> lines = data.split('\n');
        for ( const QByteArray &line : lines )
        {
            if ( !line.trimmed().isEmpty() )
                qCDebug(runtime) << "rigctld:" << line.trimmed();
        }
    }
}

void RigctldManager::onReadyReadStderr()
{
    if (!rigctldProcess)
        return;

    const QByteArray data = rigctldProcess->readAllStandardError();
    if ( !data.isEmpty() )
    {
        const QList<QByteArray> lines = data.split('\n');
        for ( const QByteArray &line : lines )
        {
            if ( !line.trimmed().isEmpty() )
                qCDebug(runtime) << "rigctld stderr:" << line.trimmed();
        }
    }
}
