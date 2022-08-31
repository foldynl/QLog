#include "CWDummyKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.cwdummykey");

CWDummyKey::CWDummyKey(QObject *parent)
    : CWKey(parent),
      isUsed(false)
{
    FCT_IDENTIFICATION;
}

bool CWDummyKey::open()
{
    FCT_IDENTIFICATION;

    qInfo() << "Key is Connected";

    isUsed = true;

    return true;
}

bool CWDummyKey::close()
{
    FCT_IDENTIFICATION;

    qInfo() << "Key is Disconnected";

    isUsed = false;

    return true;
}

bool CWDummyKey::sendText(QString text)
{
    FCT_IDENTIFICATION;

    if ( isUsed )
    {
        qInfo() << "Sending " << text;
    }

    return true;
}

bool CWDummyKey::setWPM(qint16 wpm)
{
    FCT_IDENTIFICATION;

    if ( isUsed )
    {
        qInfo() << "Setting Speed " << wpm;
    }

    return true;
}

bool CWDummyKey::setMode(CWKeyModeID mode)
{
    FCT_IDENTIFICATION;

    if ( isUsed )
    {
        qInfo() << "Setting Mode " << mode;
    }

    return true;
}

bool CWDummyKey::isConnected()
{
    FCT_IDENTIFICATION;

    return isUsed;
}
