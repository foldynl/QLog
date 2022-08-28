#include "CWDummyKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.CWKey");

CWDummyKey::CWDummyKey(QObject *parent)
    : CWKey("DUMMY", 9600, 30000, parent),
      isConnected(false)
{
    FCT_IDENTIFICATION;
}

bool CWDummyKey::open()
{
    FCT_IDENTIFICATION;

    qInfo() << "Key is Connected";

    isConnected = true;

    return true;
}

bool CWDummyKey::close()
{
    FCT_IDENTIFICATION;

    qInfo() << "Key is Disconnected";

    isConnected = false;

    return true;
}

bool CWDummyKey::sendText(QString text)
{
    FCT_IDENTIFICATION;

    if ( isConnected )
    {
        qInfo() << "Sending " << text;
    }

    return true;
}

bool CWDummyKey::setWPM(qint16 wpm)
{
    FCT_IDENTIFICATION;

    if ( isConnected )
    {
        qInfo() << "Setting Speed " << wpm;
    }

    return true;
}

bool CWDummyKey::setMode(CWKeyModeID mode)
{
    FCT_IDENTIFICATION;

    if ( isConnected )
    {
        qInfo() << "Setting Mode " << mode;
    }

    return true;
}
