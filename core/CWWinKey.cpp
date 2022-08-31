#include <QMutexLocker>

#include "CWWinKey.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.cwwinkey");

CWWinKey2::CWWinKey2(const QString &portName, const qint32 baudrate, QObject *parent)
    : CWKey(parent),
      isUsed(false),
      xoff(false),
      timeout(2000)
{
    FCT_IDENTIFICATION;
    serial.setPortName(portName);
    if ( ! serial.setBaudRate(baudrate, QSerialPort::AllDirections) )
    {
        qInfo() << "Cannot set Baudrate for Serial port";
    }
    if ( ! serial.setDataBits(QSerialPort::Data8) )
    {
        qInfo() << "Cannot set Data Bits for Serial port";
    }
    if ( ! serial.setParity(QSerialPort::NoParity) )
    {
        qInfo() << "Cannot set Parity for Serial port";
    }
    if ( ! serial.setFlowControl(QSerialPort::NoFlowControl) )
    {
        qInfo() << "Cannot set Flow Control for Serial port";
    }
    if ( ! serial.setStopBits(QSerialPort::TwoStop) )
    {
        qInfo() << "Cannot set Stop Bits for Serial port";
    }
}

bool CWWinKey2::open()
{
    FCT_IDENTIFICATION;

    QByteArray cmd;
    QMutexLocker locker(&portMutex);

    __close(); //close if open

    connect(&serial, &QSerialPort::readyRead, this, &CWWinKey2::handleReadyRead);
    connect(&serial, &QSerialPort::bytesWritten, this, &CWWinKey2::handleBytesWritten);
    connect(&serial, &QSerialPort::errorOccurred, this, &CWWinKey2::handleError);

    /***************/
    /* Open Port   */
    /***************/
    qCDebug(runtime) << "Opening Serial";

    if ( !serial.open(QIODevice::ReadWrite) )
    {
        qInfo() << "Cannot open " << serial.portName() << " Error code " << serial.error();
        return false;
    }

    serial.setDataTerminalReady(true);
    serial.setRequestToSend(false);

    qCDebug(runtime) << "Serial port is opened";

    QThread::msleep(200);

    /********************/
    /* Enable Host Mode */
    /********************/
    qCDebug(runtime) << "Enabling Host Mode";
    cmd.resize(2);
    cmd[0] = 0x00;
    cmd[1] = 0x02;
qCDebug(runtime) << "sending";
    serial.write(cmd);
serial.flush();
    qCDebug(runtime) << "sent";
QCoreApplication::processEvents();
//    if ( sendBytes(cmd) != 2 )
  //  {
    //    qCDebug(runtime) << "Sending error";
      //  return false;
    //}

    //receiveBytes(cmd);
#if 0
    qCDebug(runtime) << "Host Mode enabled";
    QThread::msleep(300);

    /********************/
    /* setting mode Mode */
    /********************/
    qCDebug(runtime) << "Mode Setting";
    cmd.resize(2);
    cmd[0] = 0x0E;
    cmd[1] = 0x84;

    if ( sendBytes(cmd) != 2 )
    {
        qCDebug(runtime) << "Sending error";
        return false;
    }

    receiveBytes(cmd);
    qCDebug(runtime) << "Mode setting done";
    QThread::msleep(300);
#endif
    return true;
}

bool CWWinKey2::close()
{
    FCT_IDENTIFICATION;

    QMutexLocker locker(&portMutex);
    __close();

    return true;
}

bool CWWinKey2::sendText(QString text)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << text;

    sendBufferMutex.lock();
    writeBuffer.append(text.toLocal8Bit().data());
    sendBufferMutex.unlock();

    tryAsyncWrite();

    return true;
}

void CWWinKey2::tryAsyncWrite()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << writeBuffer.isEmpty() << xoff << writeBuffer.size();

    if ( writeBuffer.isEmpty() || xoff ) return;

    sendBufferMutex.lock();
    portMutex.lock();
    if ( serial.write(writeBuffer.data(), 1) > 0 )
    {
        writeBuffer.remove(0, 1);
    }
    else
    {
        qInfo() << serial.error();
    }
    portMutex.unlock();
    sendBufferMutex.unlock();
}

void CWWinKey2::handleBytesWritten(qint64 bytes)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << bytes;

    tryAsyncWrite();
}

void CWWinKey2::handleReadyRead()
{
    FCT_IDENTIFICATION;

    unsigned char rcvByte;

    portMutex.lock();
    serial.read((char*)&rcvByte,1);
    portMutex.unlock();

    qCDebug(runtime) << "RCV: " << Qt::hex << rcvByte;

    if ( (rcvByte & 0xC0) == 0xC0 )
    {
        qCDebug(runtime) << "Stat received";
        xoff = false;

        if ( rcvByte == 0xC0 )
        {
            qCDebug(runtime) << "Idle";
        }
        else
        {
            if ( rcvByte & 0x01 )
            {
                qCDebug(runtime) << "Buffer 2/3 full";
                xoff = true;
            }
            if ( rcvByte & 0x02 )
            {
                qCDebug(runtime) << "Brk-in";
            }
            if ( rcvByte & 0x04 )
            {
                qCDebug(runtime) << "Key Busy";
            }
            if ( rcvByte & 0x08 )
            {
                qCDebug(runtime) << "Tunning";
            }
            if ( rcvByte & 0x0F )
            {
                qCDebug(runtime) << "Waiting";
            }
        }
    }
    else if ( (rcvByte & 0xC0) == 0x80 )
    {
        qInfo() << "Pot Info";
    }
    else
    {
        qInfo() << "Echo char " << rcvByte;
    }

    tryAsyncWrite();
}

void CWWinKey2::handleError(QSerialPort::SerialPortError serialPortError)
{
    FCT_IDENTIFICATION;

    if ( serialPortError == QSerialPort::ReadError )
    {
        qInfo() << "An I/O error occurred while reading: " << serial.errorString();
    }
    else if ( serialPortError == QSerialPort::WriteError )
    {
        qInfo() << "An I/O error occurred while writing: " << serial.errorString();
    }
    else
    {
        qInfo() << "An I/O error occurred " << serial.errorString();
    }
}

bool CWWinKey2::setWPM(qint16)
{
    FCT_IDENTIFICATION;

    return true;
}

bool CWWinKey2::setMode(CWKeyModeID)
{
    FCT_IDENTIFICATION;
    return true;
}

QString CWWinKey2::lastError()
{
    FCT_IDENTIFICATION;

    return serial.errorString();
}

void CWWinKey2::__close()
{
    FCT_IDENTIFICATION;

    if ( serial.isOpen() )
    {
        QByteArray cmd;

        /********************/
        /* Disabling Host Mode */
        /********************/
        qCDebug(runtime) << "Disabling Host Mode";
        cmd.resize(2);
        cmd[0] = 0x00;
        cmd[1] = 0x03;

        if ( sendBytes(cmd) != 2 )
        {
            qCDebug(runtime) << "Sending error";
        }
        else
        {
            qCDebug(runtime) << "Host Mode disabled";
        }

        QThread::msleep(200);
        serial.close();
        isUsed = false;
    }

    disconnect(&serial, &QSerialPort::bytesWritten, this, &CWWinKey2::handleBytesWritten);
    disconnect(&serial, &QSerialPort::errorOccurred, this, &CWWinKey2::handleError);
    disconnect(&serial, &QSerialPort::readyRead, this, &CWWinKey2::handleReadyRead);
}

qint64 CWWinKey2::sendBytes(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "SND: " << data;

    qint64 ret = serial.write(data);

    if ( !serial.waitForBytesWritten(timeout) )
    {
        qCDebug(runtime) << "Serial Port Timeout";
        return -1;
    }
    serial.flush();
    return ret;
}

qint64 CWWinKey2::receiveBytes(QByteArray &data)
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

    qCDebug(runtime) << "RCV: " << data;
    return data.size();
}
