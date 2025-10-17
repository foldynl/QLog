#include "CWDummyKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.cwkey.driver.cwdummykey");

CWDummyKey::CWDummyKey(QObject *parent)
    : CWKey(CWKey::IAMBIC_B, 25, parent),
      isUsed(false)
{
    FCT_IDENTIFICATION;
    canSetKeySpeed = true;
}

bool CWDummyKey::open()
{
    FCT_IDENTIFICATION;

    qInfo() << "Key is Connected";

    isUsed = true;

    setWPM(defaultWPMSpeed);
    return true;
}

bool CWDummyKey::close()
{
    FCT_IDENTIFICATION;

    qInfo() << "Key is Disconnected";

    isUsed = false;

    return true;
}

bool CWDummyKey::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    if ( isUsed )
    {
        qInfo() << "Sending " << text;
    }

    return true;
}

bool CWDummyKey::setWPM(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    if ( !isUsed )
        return true;

    qInfo() << "Setting Speed " << wpm;

    emit keyChangedWPMSpeed(wpm); // dummy does not echo a new Speed
            //therefore keyChangedWPMSpeed informs the rest for QLog that
            //Key speed has been changed
    return true;
}

QString CWDummyKey::lastError()
{
    FCT_IDENTIFICATION;

    return QString();
}

bool CWDummyKey::immediatelyStop()
{
    FCT_IDENTIFICATION;

    if ( isUsed )
    {
        qInfo() << "immediately Stop";
    }

    return true;
}

