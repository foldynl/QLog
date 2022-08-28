#include "CWKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.cwkey");

CWKey::CWKey(const QString &portName,
             const qint32 baudrate,
             const qint32 msec,
             QObject *parent) :
    QObject(parent),
    timeout(msec)
{
    FCT_IDENTIFICATION;

    serial.setPortName(portName);
    serial.setBaudRate(baudrate, QSerialPort::AllDirections);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setFlowControl(QSerialPort::NoFlowControl);
    serial.setStopBits(QSerialPort::TwoStop);
}

qint64 CWKey::sendBytes(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data;

    qint64 ret = serial.write(data);

    if ( !serial.waitForBytesWritten(timeout) )
    {
        qCDebug(runtime) << "Serial Port Timeout";
        return -1;
    }

    return ret;
}

qint64 CWKey::receiveBytes(QByteArray &data)
{
    FCT_IDENTIFICATION;

    if ( serial.waitForReadyRead(timeout))
    {
        data = serial.readAll();
        while ( serial.waitForReadyRead(10) )
        {
            data += serial.readAll();
        }
    }
    else
    {
        qCDebug(runtime) << "Serial Port Timeout";
        return -1;
    }

    return data.size();
}

bool CWKey::isConnected()
{
    FCT_IDENTIFICATION;

    bool ret = serial.isOpen();

    qCDebug(runtime) << ret;
    return ret;
}
