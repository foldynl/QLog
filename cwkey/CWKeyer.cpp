#include "CWKeyer.h"
#include "cwkey/drivers/CWKey.h"
#include "cwkey/drivers/CWDummyKey.h"
#include "cwkey/drivers/CWWinKey.h"
#include "cwkey/drivers/CWCatKey.h"
#include "cwkey/drivers/CWDaemonKey.h"
#include "cwkey/drivers/CWFldigiKey.h"
#include "core/debug.h"
#include "data/CWKeyProfile.h"

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
        qCDebug(runtime) << "Reconnecting to a new CW Key - " << currCWProfile.profileName << "; Old - " << connectedCWKeyProfile.profileName;
        __openCWKey();
    }
    timer->start(TIME_PERIOD);
    cwKeyLock.unlock();
}

void CWKeyer::open()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, &CWKeyer::openImpl, Qt::QueuedConnection);
}

void CWKeyer::openImpl()
{
    FCT_IDENTIFICATION;

    cwKeyLock.lock();
    __openCWKey();
    cwKeyLock.unlock();
}

void CWKeyer::__openCWKey()
{
    FCT_IDENTIFICATION;

    // if cw keys is active then close it
    __closeCWKey();

    CWKeyProfile newProfile = CWKeyProfilesManager::instance()->getCurProfile1();

    if ( newProfile == CWKeyProfile() )
    {
        emit cwKeyerError(tr("No CW Keyer Profile selected"),
                          QString());

        return;
    }

    qCDebug(runtime) << "Opening profile name: " << newProfile.profileName;

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

    if ( !cwKey->open() )
    {
        emit cwKeyerError(tr("Connection Error"),
                          tr("Cannot open the Keyer connection"));
        qWarning() << cwKey->lastError();
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
    cwKeyLock.lock();
    qCDebug(runtime) << "Using Key";
    __closeCWKey();
    cwKeyLock.unlock();
}

void CWKeyer::__closeCWKey()
{
    FCT_IDENTIFICATION;

    connectedCWKeyProfile = CWKeyProfile();

    if ( cwKey )
    {
        cwKey->close();
        cwKey->deleteLater();
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
    closeImpl();
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
    timer(nullptr)
{
    FCT_IDENTIFICATION;
}

CWKeyer::~CWKeyer()
{
    FCT_IDENTIFICATION;

    if ( cwKey )
    {
        cwKey->deleteLater();
    }
}
