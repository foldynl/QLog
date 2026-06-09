#include "CWKeyer.h"
#include "cwkey/drivers/CWKey.h"
#include "cwkey/drivers/CWDummyKey.h"
#include "cwkey/drivers/CWWinKey.h"
#include "cwkey/drivers/CWCatKey.h"
#include "cwkey/drivers/CWDaemonKey.h"
#include "cwkey/drivers/CWFldigiKey.h"
#include "core/debug.h"
#include "data/CWKeyProfile.h"
#include <QSemaphore>
#include <QSharedPointer>
#include <QThread>

MODULE_IDENTIFICATION("qlog.cwkey.cwkeyer");

#define TIME_PERIOD 1000

void CWKeyer::start()
{
    FCT_IDENTIFICATION;

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CWKeyer::update);
    timer->start(TIME_PERIOD);
}

void CWKeyer::stopTimer()
{
    FCT_IDENTIFICATION;
    bool check = QMetaObject::invokeMethod(CWKeyer::instance(), &CWKeyer::stopTimerImplt, Qt::QueuedConnection);
    Q_ASSERT( check );
}

void CWKeyer::shutdown()
{
    FCT_IDENTIFICATION;

    if ( QThread::currentThread() == thread() )
    {
        shutdownImpl();
        return;
    }

    if ( !thread() || !thread()->isRunning() )
    {
        qCWarning(runtime) << "Cannot shut down CWKeyer because owner thread is not running";
        return;
    }

    QSharedPointer<QSemaphore> shutdownDone = QSharedPointer<QSemaphore>::create();
    bool check = QMetaObject::invokeMethod(this, [this, shutdownDone]()
    {
        shutdownImpl();
        shutdownDone->release();
    }, Qt::QueuedConnection);

    if ( !check )
    {
        qCWarning(runtime) << "Cannot queue CWKeyer shutdown";
        return;
    }

    if ( !shutdownDone->tryAcquire(1, SHUTDOWN_TIMEOUT_MS) )
    {
        qCWarning(runtime) << "CWKeyer shutdown did not finish within"
                           << SHUTDOWN_TIMEOUT_MS << "ms";
    }
}

void CWKeyer::update()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";
    if ( !cwKeyLock.tryLock(200) )
    {
        qCDebug(runtime) << "Waited too long";
        return;
    }
    qCDebug(runtime) << "Updating key state";

    if ( !cwKey )
    {
        cwKeyLock.unlock();
        return;
    }

    CWKeyProfile currCWProfile = CWKeyProfilesManager::instance()->getCurProfile1();
    /***********************************************************/
    /* Is Opened Profile still the globbaly used CW Profile ? */
    /* if NO then reconnect it                                 */
    /***********************************************************/
    if ( currCWProfile != connectedCWKeyProfile)
    {
        /* CW Key Profile Changed
         * Need to reconnect CW Key
         */
        const bool sameProfileName = currCWProfile.profileName == connectedCWKeyProfile.profileName;
        const CWKey::CWKeyOutputID reconnectCWKeyOutput = sameProfileName ? connectedCWKeyOutput
                                                                          : CWKey::CWKEY_OUTPUT_DEFAULT;
        if ( sameProfileName )
        {
            qCDebug(runtime) << "CW keyer profile settings changed; reconnecting instance"
                             << cwKey << "of profile" << currCWProfile.profileName
                             << "while preserving output"
                             << CWKey::outputName(reconnectCWKeyOutput);
        }
        else
        {
            qCDebug(runtime) << "CW keyer profile changed; reconnecting instance"
                             << cwKey << "from profile"
                             << connectedCWKeyProfile.profileName << "to"
                             << currCWProfile.profileName << "with output"
                             << CWKey::outputName(reconnectCWKeyOutput);
        }
        __openCWKey(reconnectCWKeyOutput);
    }
    timer->start(TIME_PERIOD);
    cwKeyLock.unlock();
}

void CWKeyer::open()
{
    FCT_IDENTIFICATION;

    open(CWKey::CWKEY_OUTPUT_DEFAULT);
}

void CWKeyer::open(qint32 cwKeyOutput)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << cwKeyOutput;

    qCDebug(runtime) << "Queueing CW keyer open with output"
                     << CWKey::outputName(CWKey::intToOutputID(cwKeyOutput));
    QMetaObject::invokeMethod(this,
                              [this, cwKeyOutput]() { openImpl(cwKeyOutput); },
                              Qt::QueuedConnection);
}

void CWKeyer::openImpl(qint32 cwKeyOutput)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << cwKeyOutput;

    QMutexLocker locker(&cwKeyLock);

    CWKeyProfile newProfile = CWKeyProfilesManager::instance()->getCurProfile1();
    const CWKey::CWKeyOutputID output = CWKey::intToOutputID(cwKeyOutput);
    qCDebug(runtime) << "Processing CW keyer open for profile"
                     << newProfile.profileName << "model" << newProfile.model
                     << "output" << CWKey::outputName(output)
                     << "connected instance" << cwKey;

    if ( cwKey && newProfile == connectedCWKeyProfile )
    {
        qCDebug(runtime) << "Reusing CW keyer instance" << cwKey
                         << "because profile" << newProfile.profileName
                         << "is already open; applying output without disconnect";
        if ( !setCWKeyOutput(output) )
            __closeCWKey();
        return;
    }

    __openCWKey(output);
}

bool CWKeyer::setCWKeyOutput(CWKey::CWKeyOutputID cwKeyOutput)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << cwKeyOutput;

    if ( cwKeyOutput == connectedCWKeyOutput )
    {
        qCDebug(runtime) << "CW keyer output already set to"
                         << CWKey::outputName(connectedCWKeyOutput)
                         << "on instance" << cwKey << "; no driver command needed";
        return true;
    }

    qCDebug(runtime) << "Switching CW keyer output from"
                     << CWKey::outputName(connectedCWKeyOutput) << "to"
                     << CWKey::outputName(cwKeyOutput)
                     << "on instance" << cwKey;
    if ( !cwKey->setOutput(cwKeyOutput) )
    {
        const QString keyError = cwKey->lastError();
        emit cwKeyerError(tr("CW Keyer Output Error"),
                          keyError.isEmpty() ? tr("Cannot switch CW keyer output")
                                             : keyError);
        qWarning() << keyError;
        return false;
    }

    connectedCWKeyOutput = cwKeyOutput;
    qCDebug(runtime) << "CW keyer output switched to"
                     << CWKey::outputName(connectedCWKeyOutput)
                     << "on instance" << cwKey;
    return true;
}

void CWKeyer::__openCWKey(CWKey::CWKeyOutputID cwKeyOutput)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << cwKeyOutput;

    // if cw keys is active then close it
    __closeCWKey();

    CWKeyProfile newProfile = CWKeyProfilesManager::instance()->getCurProfile1();
    if ( newProfile == CWKeyProfile() )
    {
        emit cwKeyerError(tr("No CW Keyer Profile selected"),
                          QString());

        return;
    }

    qCDebug(runtime) << "Opening CW keyer profile" << newProfile.profileName
                     << "model" << newProfile.model
                     << "serial port" << newProfile.portPath
                     << "host" << newProfile.hostname
                     << "network port" << newProfile.netport
                     << "output" << CWKey::outputName(cwKeyOutput);

    switch ( newProfile.model )
    {
    case CWKey::DUMMY_KEYER:
        cwKey = new CWDummyKey(this);
        break;
    case CWKey::WINKEY_KEYER:
        cwKey = new CWWinKey(newProfile.portPath,
                              newProfile.baudrate,
                              newProfile.keyMode,
                              newProfile.defaultSpeed,
                              newProfile.paddleSwap,
                              newProfile.paddleOnlySidetone,
                              newProfile.sidetoneFrequency,
                              this);
        break;
    case CWKey::MORSEOVERCAT:
        cwKey = new CWCatKey(newProfile.keyMode,
                             newProfile.defaultSpeed,
                             this);
        break;
     case CWKey::CWDAEMON_KEYER:
        cwKey = new CWDaemonKey(newProfile.hostname,
                                newProfile.netport,
                                newProfile.keyMode,
                                newProfile.defaultSpeed,
                                newProfile.sidetoneFrequency,
                                this);
        break;
    case CWKey::FLDIGI_KEYER:
       cwKey = new CWFldigiKey(newProfile.hostname,
                               newProfile.netport,
                               newProfile.keyMode,
                               newProfile.defaultSpeed,
                               this);
        break;
    default:
        cwKey = nullptr;
        qWarning() << "Unsupported Key Model " << newProfile.model;
    }

    if ( !cwKey )
    {
        // initialization failed
        emit cwKeyerError(tr("Initialization Error"),
                          tr("Internal Error"));
        return;
    }

    qCDebug(runtime) << "Created CW keyer instance" << cwKey
                     << "for profile" << newProfile.profileName;

    if ( !cwKey->setOutput(cwKeyOutput) )
    {
        const QString keyError = cwKey->lastError();
        emit cwKeyerError(tr("CW Keyer Output Error"),
                          keyError.isEmpty() ? tr("Cannot set CW keyer output")
                                             : keyError);
        qWarning() << keyError;
        __closeCWKey();
        return;
    }

    if ( !cwKey->open() )
    {
        const QString keyError = cwKey->lastError();
        emit cwKeyerError(tr("Connection Error"),
                          keyError.isEmpty() ? tr("Cannot open the Keyer connection")
                                             : keyError);
        qWarning() << keyError;
        __closeCWKey();
        return;
    }

    connect(cwKey, &CWKey::keyError, this, &CWKeyer::keyErrorHandler);
    connect(cwKey, &CWKey::keyChangedWPMSpeed, this, &CWKeyer::cwKeyWPMChangedHandler);
    connect(cwKey, &CWKey::keyEchoText, this, &CWKeyer::cwKeyEchoTextHandler);
    connect(cwKey, &CWKey::keyHWButton1Pressed, this, &CWKeyer::cwKeyHWButton1PressedHandler);
    connect(cwKey, &CWKey::keyHWButton2Pressed, this, &CWKeyer::cwKeyHWButton2PressedHandler);
    connect(cwKey, &CWKey::keyHWButton3Pressed, this, &CWKeyer::cwKeyHWButton3PressedHandler);
    connect(cwKey, &CWKey::keyHWButton4Pressed, this, &CWKeyer::cwKeyHWButton4PressedHandler);

    connectedCWKeyProfile = newProfile;
    connectedCWKeyOutput = cwKeyOutput;

    qCDebug(runtime) << "CW keyer profile opened" << connectedCWKeyProfile.profileName
                     << "instance" << cwKey
                     << "model" << connectedCWKeyProfile.model
                     << "serial port" << connectedCWKeyProfile.portPath
                     << "host" << connectedCWKeyProfile.hostname
                     << "network port" << connectedCWKeyProfile.netport
                     << "output" << CWKey::outputName(connectedCWKeyOutput);

    emit cwKeyConnected(connectedCWKeyProfile.profileName);
}

void CWKeyer::close()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &CWKeyer::closeImpl, Qt::QueuedConnection);
}

bool CWKeyer::canStopSending()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey )
    {
        return false;
    }

    bool ret = cwKey->canStopSending();

    return ret;
}

bool CWKeyer::canEchoChar()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey )
    {
        return false;
    }

    bool ret = cwKey->canEchoChar();

    return ret;
}

bool CWKeyer::rigMustConnected()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey )
    {
        return false;
    }


    bool ret = cwKey->mustRigConnected();

    return ret;
}

bool CWKeyer::canSetSpeed()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey )
    {
        return false;
    }

    bool ret = cwKey->canSetSpeed();

    return ret;
}

void CWKeyer::closeImpl()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";
    QMutexLocker locker(&cwKeyLock);
    qCDebug(runtime) << "Using Key";
    __closeCWKey();
}

void CWKeyer::shutdownImpl()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";
    QMutexLocker locker(&cwKeyLock);
    qCDebug(runtime) << "Using Key";
    __closeCWKey();
    stopTimerImplt();
}

void CWKeyer::__closeCWKey()
{
    FCT_IDENTIFICATION;

    if ( cwKey )
    {
        qCDebug(runtime) << "Closing CW keyer profile" << connectedCWKeyProfile.profileName
                         << "instance" << cwKey
                         << "model" << connectedCWKeyProfile.model
                         << "serial port" << connectedCWKeyProfile.portPath
                         << "host" << connectedCWKeyProfile.hostname
                         << "network port" << connectedCWKeyProfile.netport
                         << "output" << CWKey::outputName(connectedCWKeyOutput);
    }

    connectedCWKeyProfile = CWKeyProfile();
    connectedCWKeyOutput = CWKey::CWKEY_OUTPUT_DEFAULT;

    if ( cwKey )
    {
        cwKey->close();
        delete cwKey;
        cwKey = nullptr;
    }

    emit cwKeyDisconnected();
}

void CWKeyer::setSpeed(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setSpeedImpl",
                              Qt::QueuedConnection,
                              Q_ARG(qint16, wpm));
}

void CWKeyer::setSpeedImpl(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey ) return;

    cwKey->setWPM(wpm);
}

void CWKeyer::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "sendTextImpl",
                              Qt::QueuedConnection,
                              Q_ARG(QString, text));
}

void CWKeyer::sendTextImpl(const QString &text)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey ) return;

    cwKey->sendText(text);
}

void CWKeyer::immediatelyStop()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "immediatelyStopImpl",
                              Qt::QueuedConnection);
}

void CWKeyer::immediatelyStopImpl()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for cwkey mutex";

    QMutexLocker locker(&cwKeyLock);

    qCDebug(runtime) << "Using Key";

    if ( !cwKey ) return;

    cwKey->immediatelyStop();
}

void CWKeyer::stopTimerImplt()
{
    FCT_IDENTIFICATION;

    if ( timer )
    {
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }
}

void CWKeyer::keyErrorHandler(const QString &main, const QString &detail)
{
    FCT_IDENTIFICATION;
    emit cwKeyerError(main, detail);
    close();
}

void CWKeyer::cwKeyWPMChangedHandler(qint32 wpm)
{
    FCT_IDENTIFICATION;

    emit cwKeyWPMChanged(wpm);
}

void CWKeyer::cwKeyEchoTextHandler(const QString &text)
{
    FCT_IDENTIFICATION;

    emit cwKeyEchoText(text);
}

void CWKeyer::cwKeyHWButton1PressedHandler()
{
    emit cwKeyHWButton(1);
}

void CWKeyer::cwKeyHWButton2PressedHandler()
{
    emit cwKeyHWButton(2);
}

void CWKeyer::cwKeyHWButton3PressedHandler()
{
    emit cwKeyHWButton(3);
}

void CWKeyer::cwKeyHWButton4PressedHandler()
{
    emit cwKeyHWButton(4);
}

CWKeyer::CWKeyer(QObject *parent ) :
    QObject(parent),
    cwKey(nullptr),
    connectedCWKeyOutput(CWKey::CWKEY_OUTPUT_DEFAULT),
    timer(nullptr)
{
    FCT_IDENTIFICATION;
}

CWKeyer::~CWKeyer()
{
    FCT_IDENTIFICATION;

    if ( !cwKey && !timer )
        return;

    if ( QThread::currentThread() != thread() )
    {
        qCWarning(runtime) << "Skipping CWKeyer shutdown from non-owner thread";
        return;
    }

    shutdownImpl();
}
