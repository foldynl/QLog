#include "CWDummyKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.cwkey.driver.cwdummykey");

CWDummyKey::CWDummyKey(QObject *parent)
    : CWKey(CWKey::IAMBIC_B, 25, parent),
      isUsed(false),
      output(CWKEY_OUTPUT_DEFAULT)
{
    FCT_IDENTIFICATION;
    canSetKeySpeed = true;
}

bool CWDummyKey::open()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Opening Dummy keyer with output" << outputName(output);
    isUsed = true;

    setWPM(defaultWPMSpeed);
    return true;
}

bool CWDummyKey::close()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Disconnecting Dummy keyer";
    isUsed = false;
    return true;
}

bool CWDummyKey::sendText(const QString &text)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << text;

    if ( isUsed )
    {
        qCDebug(runtime) << "Dummy keyer sending on output" << outputName(output)
                         << stripSpeedMarkers(text);
    }

    return true;
}

bool CWDummyKey::setWPM(const qint16 wpm)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << wpm;

    if ( !isUsed )
        return true;

    qCDebug(runtime) << "Setting Dummy keyer speed" << wpm;

    emit keyChangedWPMSpeed(wpm); // dummy does not echo a new Speed
            //therefore keyChangedWPMSpeed informs the rest for QLog that
            //Key speed has been changed
    return true;
}

bool CWDummyKey::setOutput(CWKeyOutputID newOutput)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << newOutput;

    qCDebug(runtime) << "Setting Dummy keyer output to" << outputName(newOutput);
    output = newOutput;
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
        qCDebug(runtime) << "Immediately stopping Dummy keyer";

    return true;
}
