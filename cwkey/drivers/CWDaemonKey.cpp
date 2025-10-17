#include "CWDaemonKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.cwkey.driver.cwdaemonkey");

CWDaemonKey::CWDaemonKey(const QString &hostname,
                         const quint16 port,
                         const CWKey::CWKeyModeID mode,
                         const qint32 defaultSpeed,
                         QObject *parent) :
    CWKey(mode, defaultSpeed, parent),
    CWKeyUDPInterface(hostname, port),
    isOpen(false),
    ESCChar(27)
{
    FCT_IDENTIFICATION;

    stopSendingCap = true;
    canSetKeySpeed = true;
    printKeyCaps();
}

bool CWDaemonKey::open()
{
    FCT_IDENTIFICATION;

    isOpen = isSocketReady();

    return isOpen;
}

bool CWDaemonKey::close()
{
    FCT_IDENTIFICATION;

    isOpen = false;

    return true;
}

QString CWDaemonKey::lastError()
{
    FCT_IDENTIFICATION;
    qCDebug(runtime) << socket.error();
    qCDebug(runtime) << lastLogicalError;
    return (lastLogicalError.isEmpty()) ? socket.errorString() : lastLogicalError;
}

bool CWDaemonKey::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << text;

    if ( text.isEmpty() )
        return true;

    if ( !isOpen )
    {
        qCWarning(runtime) << "Key is not connected";
        lastLogicalError = tr("Keyer is not connected");
        emit keyError(tr("Cannot send Text"), lastLogicalError);
        return false;
    }

    QString chpString(text);
    chpString.replace("\n", "");

    //do not solve possible socket errors - it is the UDP connection
    return (sendData(chpString.toLatin1()) > 0) ? true : false;
}

bool CWDaemonKey::setWPM(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    if ( !isOpen )
    {
        qCWarning(runtime) << "Key is not connected";
        lastLogicalError = tr("Keyer is not connected");
        emit keyError(tr("Cannot set Keyer Speed"), lastLogicalError);
        return false;
    }

    QString sentString(ESCChar + QString("2") + QString::number(wpm));
    emit keyChangedWPMSpeed(wpm);
    return (sendData(sentString.toLatin1()) > 0) ? true : false;
}

bool CWDaemonKey::immediatelyStop()
{
    FCT_IDENTIFICATION;

    if ( !isOpen )
    {
        qCWarning(runtime) << "Key is not connected";
        lastLogicalError = tr("Keyer is not connected");
        emit keyError(tr("Cannot stop Text Sending"), lastLogicalError);
        return false;
    }

    QString sentString(ESCChar + QString("4"));
    return (sendData(sentString.toLatin1()) > 0) ? true : false;
}
