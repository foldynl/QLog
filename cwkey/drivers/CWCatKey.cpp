#include "CWCatKey.h"
#include "core/debug.h"
#include "rig/Rig.h"

MODULE_IDENTIFICATION("qlog.cwkey.driver.cwcatkey");

CWCatKey::CWCatKey(const CWKey::CWKeyModeID mode,
                   const qint32 defaultSpeed,
                   QObject *parent)
    : CWKey(mode, defaultSpeed, parent),
      isKeyConnected(false)
{
    FCT_IDENTIFICATION;
}

CWCatKey::~CWCatKey()
{
    FCT_IDENTIFICATION;
}

bool CWCatKey::open()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for Command Mutex";
    QMutexLocker locker(&commandMutex);

    __close();

    /********************************/
    /* Test if any rig is connected */
    /********************************/
    if ( !Rig::instance()->isRigConnected() )
    {
        qWarning() << "Rig is not connected";
        lastErrorText = tr("No Rig is connected");
        __close();
        return false;
    }
    /**************************************/
    /* Test if the rig has Morse over CAT */
    /**************************************/
    if ( !Rig::instance()->isMorseOverCatSupported() )
    {
        qWarning() << "Rig does not support Morse over CAT";
        lastErrorText = tr("Rig does not support Morse over CAT");
        __close();
        return false;
    }

    /********************************************/
    /* Rig must be in CW mode                   */
    /* swith the rig to CW mode ?               */
    /* Maybe Yes - we will see users reaction   */
    /********************************************/
    //it has the side-effect that every time you connect,
    // it causes to switch to CW when the key is assigned.
    // So let's disable it and see what's worse
    //Rig::instance()->setMode("CW", QString());

    /*******************/
    /* set default WPM */
    /*******************/
    Rig::instance()->setKeySpeed(defaultWPMSpeed);

    /************************/
    /* Test if hamlib > 4.0 */
    /* Stop sending feature */
    /************************/
#if (HAMLIBVERSION_MAJOR >= 4)
    stopSendingCap = 1;
#endif
    rigMustConnectedCap = true;
    isKeyConnected = true;
    canSetKeySpeed = true;
    lastErrorText = QString();

    connect(Rig::instance(), &Rig::keySpeedChanged, this, &CWCatKey::rigKeySpeedChanged);

    return true;
}

bool CWCatKey::close()
{
    FCT_IDENTIFICATION;
    QMutexLocker locker(&commandMutex);
    isKeyConnected = false;

    return true;
}

QString CWCatKey::lastError()
{
    FCT_IDENTIFICATION;

    return lastErrorText;
}

bool CWCatKey::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    if ( !isKeyConnected )
    {
        qCDebug(runtime) << "Cannot send ";
        emit keyError(tr("Cannot send Text to Rig"), tr("Keyer is not connected"));
        return false;
    }

    if ( !Rig::instance()->isRigConnected() )
    {
        qCDebug(runtime) << "Cannot send";
        emit keyError(tr("Cannot send Text to Rig"), tr("Rig is not connected"));
        return false;
    }

    if ( !Rig::instance()->isMorseOverCatSupported() )
    {
        qCDebug(runtime) << "Cannot send";
        emit keyError(tr("Cannot send Text to Rig"), tr("Rig does not support Morse over CAT"));
        return false;
    }

    QMutexLocker locker(&commandMutex);
    Rig::instance()->sendMorse(text);
    return true;
}

bool CWCatKey::setWPM(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    if ( !isKeyConnected )
    {
        qCDebug(runtime) << "Cannot set WPM ";
        emit keyError(tr("Cannot set Keyer Speed"), tr("Keyer is not connected"));
        return false;
    }

    if ( !Rig::instance()->isRigConnected() )
    {
        qCDebug(runtime) << "Cannot set WPM";
        emit keyError(tr("Cannot set Keyer Speed"), tr("Rig is not connected"));
        return false;
    }

    QMutexLocker locker(&commandMutex);
    Rig::instance()->setKeySpeed(wpm); //cat can echo a new Speed therefore
                                       // emit keyChangedWPMSpeed is not emitted

    return true;
}

bool CWCatKey::immediatelyStop()
{
    FCT_IDENTIFICATION;

    if ( !isKeyConnected )
    {
        qCDebug(runtime) << "Cannot stop";
        emit keyError(tr("Cannot stop Text Sending"), tr("Keyer is not connected"));
        return false;
    }

    if ( !Rig::instance()->isRigConnected() )
    {
        qCDebug(runtime) << "Cannot stop";
        emit keyError(tr("Cannot stop Text Sending"), tr("Rig is not connected"));
        return false;
    }

    if ( !Rig::instance()->isMorseOverCatSupported() )
    {
        qCDebug(runtime) << "Cannot stop";
        emit keyError(tr("Cannot stop Text Sending"), tr("Rig does not support Morse over CAT"));
        return false;
    }

    QMutexLocker locker(&commandMutex);
    Rig::instance()->stopMorse();

    return true;
}

void CWCatKey::__close()
{
    FCT_IDENTIFICATION;

    disconnect(Rig::instance(), &Rig::keySpeedChanged, this, &CWCatKey::rigKeySpeedChanged);

    isKeyConnected = false;
}

void CWCatKey::rigKeySpeedChanged(VFOID, unsigned int wpm)
{
    FCT_IDENTIFICATION;

    emit keyChangedWPMSpeed(wpm);
}
