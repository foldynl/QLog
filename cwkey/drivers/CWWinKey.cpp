#include <QMutexLocker>

#include "CWWinKey.h"
#include "core/debug.h"

/* Based on WinKey Spec
 * https://k1el.tripod.com/files/Winkey10.pdf
 * https://k1el.tripod.com/WinkeyUSBman.pdf */

MODULE_IDENTIFICATION("qlog.cwkey.driver.cwwinkey");

CWWinKey::CWWinKey(const QString &portName,
                     const qint32 baudrate,
                     const CWKey::CWKeyModeID mode,
                     const qint32 defaultSpeed,
                     bool paddleSwap,
                     QObject *parent)
    : CWKey(mode, defaultSpeed, parent),
      CWKeySerialInterface(portName, baudrate, 5000),
      isInHostMode(false),
      xoff(false),
      paddleSwap(paddleSwap),
      version(0)
{
    FCT_IDENTIFICATION;

    minWPMRange = defaultSpeed - 15; // Winkey has 31 steps for POT, it means that 15 steps to left
                                     // and 15 steps to right
    if ( minWPMRange <= 0 ) minWPMRange = 1;

    stopSendingCap = true;
    echoCharsCap = true;
    canSetKeySpeed = true;
}

CWWinKey::~CWWinKey()
{
    FCT_IDENTIFICATION;
}

bool CWWinKey::open()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for Command Mutex";
    QMutexLocker locker(&commandMutex);

    __close();

    /***************/
    /* Open Port   */
    /***************/
    qCDebug(runtime) << "Opening Serial" << serial.portName();

    if ( !serial.open(QIODevice::ReadWrite) )
    {
        qWarning() << "Cannot open " << serial.portName() << " Error code " << serial.error();
        return false;
    }

    serial.setReadBufferSize(1); // WinKey Responses are 1B.
                                 // It is important to set correct Buffer size beucase message below
                                 /* https://forum.qt.io/topic/137268/solved-qserialport-readyread-not-emitted-qserialport-waitforreadyread-always-return-false-with-ch340
                                    monegator Jun 16, 2022, 3:36 PM

                                    Hello there,
                                    i opened this thread so that others facing the same issue may find a solution.
                                    In the past we always used USB-UART cables based on FTDI232 chips (TTL-232R-5V-WE).
                                    However, due to a huge price increase in the last couple of years (5€ for a cable became 35€, now dropped to 25€) we decided to ditch
                                    FTDI and bought cables that use the CH340 chip. And why shouldn't we, they cost less than 2€ each.

                                    Our software based on VB6 worked flawlessly, even better (reduced latency) however we had issues with our Qt5 based software (Qt 5.15.2).
                                    Even if the data is available (QSerialPort::bytesAvailable return values greater than zero) the readyRead signal is never emitted, and
                                    waitForReadyRead always return false.

                                    The issue was that readBufferSize was set to zero (default value), and there must be something in the interaction between CH340 driver,
                                    windows COM port object and Qt that prevented the event from being raised, but that was never an issue for FTDI cables.

                                    The solution was to set readBufferSize to 1. Once readBufferSize is different than zero readyRead works again.
                                   */
    serial.setDataTerminalReady(true);
    serial.setRequestToSend(false);

    qCDebug(runtime) << "Serial port has been opened";

    QThread::msleep(200);

    QByteArray cmd;
    /***********************/
    /* Echo Test           */
    /*   Testing whether   */
    /*   the opposite site */
    /*   is Winkey         */
    /***********************/
    qCDebug(runtime) << "Echo Test";

    cmd.resize(3);
    cmd[0] = 0x00;
    cmd[1] = 0x04;
    cmd[2] = 0xF1u;

    if ( sendDataAndWait(cmd) != 3 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
        __close();
        return false;
    }

    if ( receiveDataAndWait(cmd) < 1 )
    {
        qWarning() << "Unexpected size of response or communication error";
        qCDebug(runtime) << lastError();
        __close();
        return false;
    }

    if ( (unsigned char)cmd.at(0) != 0xF1 )
    {
        qWarning() << "Connected device is not the Winkey type";
        lastLogicalError = tr("Connected device is not WinKey");
        __close();
        return false;
    }

    qCDebug(runtime) << "Echo Test OK";

    /********************/
    /* Enable Host Mode */
    /********************/
    qCDebug(runtime) << "Enabling Host Mode";

    cmd.resize(2);
    cmd[0] = 0x00;
    cmd[1] = 0x02;

    if ( sendDataAndWait(cmd) != 2 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
        __close();
        return false;
    }

    /* Based on the WinKey Spec, it is needed to call read:
          The host must wait for this return code before
          any other commands or data can be sent to Winkeyer
    */
    if ( receiveDataAndWait(cmd) < 1 )
    {
        qWarning() << "Unexpected size of response or communication error";
        qCDebug(runtime) << lastError();
        __close();
        return false;
    }

    version = (unsigned char)cmd.at(0);
    qCDebug(runtime) << "Winkey version" << version;

    lastLogicalError = QString();

    qCDebug(runtime) << "Host Mode has been enabled";

    /******************/
    /* Mode Setting   */
    /******************/
    qCDebug(runtime) << "Mode Setting";

    cmd.resize(2);
    cmd[0] = 0x0E;
    cmd[1] = buildWKModeByte();

    if ( sendDataAndWait(cmd) != 2 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
        __close();
        return false;
    }

    //receiveDataAndWait(cmd); /* it is not needed to read here - no response */

    qCDebug(runtime) << "Mode has been set";

    /******************/
    /* WK2 Mode Status*/
    /******************/
    if ( version >= 20 )
    {
        qCDebug(runtime) << "WK2 PB Mode Setting";

        cmd.resize(2);
        cmd[0] = 0x00;
        cmd[1] = 0x0B;

        if ( sendDataAndWait(cmd) != 2 )
        {
            qWarning() << "Unexpected size of write response or communication error";
            qCDebug(runtime) << lastError();
            __close();
            return false;
        }

        // receiveDataAndWait(cmd); /* it is not needed to read here - no response */
        qCDebug(runtime) << "WK2 PB Mode has been set";
    }

    QThread::msleep(200);

    /* Starting Async Flow for WinKey */
    /* From this point, all Serial port functions must be Async */
    connect(&serial, &QSerialPort::readyRead, this, &CWWinKey::handleReadyRead);
    connect(&serial, &QSerialPort::bytesWritten, this, &CWWinKey::handleBytesWritten);
    connect(&serial, &QSerialPort::errorOccurred, this, &CWWinKey::handleError);

    isInHostMode = true;

    /* Force send current status */
    __sendStatusRequest();

    /* Set POT Range */
    __setPOTRange();

    /* Set Default value */
    __setWPM(defaultWPMSpeed);

    return true;
}

bool CWWinKey::close()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for Command Mutex";
    QMutexLocker locker(&commandMutex);
    __close();

    return true;
}

bool CWWinKey::sendText(const QString &text)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << text;

    qCDebug(runtime) << "Waiting for Command Mutex";
    QMutexLocker locker(&commandMutex);

    if ( !isInHostMode )
    {
        qCWarning(runtime) << "Key is not in Host Mode";
        emit keyError(tr("Cannot send Text to Rig"), tr("Keyer is not connected"));
        return false;
    }

    qCDebug(runtime) << "Waiting for WriteBuffer Mutex";
    writeBufferMutex.lock();
    qCDebug(runtime) << "Appending input string";
    QString chpString(text);
    chpString.replace("\n", "");
    writeBuffer.append(chpString.toLocal8Bit().data());
    writeBufferMutex.unlock();

    tryAsyncWrite();

    return true;
}

void CWWinKey::tryAsyncWrite()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for WriteBuffer Mutex";
    writeBufferMutex.lock();

    qCDebug(runtime) << "WBuffer Size: " << writeBuffer.size()
                     << "; XOFF: " << xoff;

    if ( writeBuffer.isEmpty() || xoff )
    {
        writeBufferMutex.unlock();
        qCDebug(runtime) << "Skipping write call";
        return;
    }

    qint64 size = writeAsyncData(QByteArray(writeBuffer.constData(),1));
    if ( size != 1 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
    }
    writeBuffer.remove(0, size);
    writeBufferMutex.unlock();

    QCoreApplication::processEvents();
}

void CWWinKey::handleBytesWritten(qint64 bytes)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << bytes;

    tryAsyncWrite();
}

void CWWinKey::handleReadyRead()
{
    FCT_IDENTIFICATION;

    unsigned char rcvByte;

    qCDebug(runtime) << "Waiting for Port Mutex";
    portMutex.lock();
    qCDebug(runtime) << "Reading from Port";
    serial.read((char*)&rcvByte,1);
    portMutex.unlock();

    qCDebug(runtime) << "\t>>>>>> RCV Async:" << QByteArray::fromRawData((char*)(&rcvByte),1);

    if ( (rcvByte & 0xC0) == 0xC0 )
    {
        qCDebug(runtime) << "\tStatus Information Message:";
        xoff = false;

        if ( rcvByte == 0xC0 ) qCDebug(runtime) << "\t\tIdle";
        else
        {
            if ( version >= 20 && rcvByte & 0x08 )
            {
                qCDebug(runtime) << "\tPushButton Status Byte";
                if ( rcvByte & 0x01 )
                {
                    qCDebug(runtime) << "\t\tButton1 pressed";
                    emit keyHWButton1Pressed();
                }
                if ( rcvByte & 0x02 )
                {
                    qCDebug(runtime) << "\t\tButton2 pressed";
                    emit keyHWButton2Pressed();
                }
                if ( rcvByte & 0x04 )
                {
                    qCDebug(runtime) << "\t\tButton3 pressed";
                    emit keyHWButton3Pressed();
                }
                if ( rcvByte & 0x10 )
                {
                    qCDebug(runtime) << "\t\tButton4 pressed";
                    emit keyHWButton4Pressed();
                }
            }
            else
            {
                qCDebug(runtime) << "\tStatus Byte";

                if ( rcvByte & 0x01 )
                {
                    qCDebug(runtime) << "\t\tBuffer 2/3 full";
                    xoff = true; //slow down in sending Write Buffer - to block tryAsyncWrite
                }
                if ( rcvByte & 0x02 ) qCDebug(runtime) << "\t\tBrk-in";
                if ( rcvByte & 0x04 ) qCDebug(runtime) << "\t\tKey Busy";
                if ( rcvByte & 0x08 ) qCDebug(runtime) << "\t\tTunning";
                if ( rcvByte & 0x0F ) qCDebug(runtime) << "\t\tWaiting";
            }
        }
    }
    else if ( (rcvByte & 0xC0) == 0x80 )
    {
        qint32 potValue = (rcvByte & 0x7F);
        qint32 currentWPM = minWPMRange + potValue;
        qCDebug(runtime) << "\tPot: " << potValue << "; WPM=" << currentWPM;
        setWPM(currentWPM);
    }
    else
    {
        qCDebug(runtime) << "\tEcho Char";
        emit keyEchoText(QString(char(rcvByte)));
    }

    tryAsyncWrite();
}

void CWWinKey::handleError(QSerialPort::SerialPortError serialPortError)
{
    FCT_IDENTIFICATION;

    QString detail = serial.errorString();

    if ( serialPortError == QSerialPort::ReadError )
    {
        qWarning() << "An I/O error occurred while reading: " << detail;
    }
    else if ( serialPortError == QSerialPort::WriteError )
    {
        qWarning() << "An I/O error occurred while writing: " << detail;
    }
    else if ( serialPortError != QSerialPort::NoError )
    {
        qWarning() << "An I/O error occurred: " << detail;
    }

    /* Emit error */
    emit keyError(tr("Communication Error"), detail);
}

bool CWWinKey::setWPM(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    qCDebug(runtime) << "Waiting for Command Mutex";
    QMutexLocker locker(&commandMutex);
    bool ret = __setWPM(wpm);
    if ( ret )
    {
        emit keyChangedWPMSpeed(wpm); //Winkey does not echo a new Speed
                //therefore keyChangedWPMSpeed informs the rest for QLog that
                //Key speed has been changed
    }
    return ret;
}

bool CWWinKey::__setWPM(const qint16 wpm)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wpm;

    if ( !isInHostMode )
    {
        qCWarning(runtime) << "Key is not in Host Mode";
        emit keyError(tr("Cannot set Keyer Speed"), tr("Keyer is not connected"));
        return false;
    }

    QByteArray cmd;
    cmd.resize(2);
    cmd[0] = 0x02;
    cmd[1] = static_cast<char>(wpm);

    qint64 size = writeAsyncData(cmd);
    if ( size != 2 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
        return false;
    }

    return true;
}

QString CWWinKey::lastError()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << serial.error();
    qCDebug(runtime) << lastLogicalError;
    return (lastLogicalError.isEmpty()) ? serial.errorString() : lastLogicalError;
}

bool CWWinKey::immediatelyStop()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for Command Mutex";
    QMutexLocker locker(&commandMutex);

    if ( !isInHostMode )
    {
        qCWarning(runtime) << "Key is not in Host Mode";
        emit keyError(tr("Cannot stop Text Sending"), tr("Keyer is not connected"));
        return false;
    }

    qCDebug(runtime) << "Waiting for WriteBuffer Mutex";
    writeBufferMutex.lock();
    qCDebug(runtime) << "Clearing Buffer";
    writeBuffer.clear();
    writeBufferMutex.unlock();

    QByteArray cmd;
    cmd.resize(3);
    cmd[0] = 0x06; /* Stop */
    cmd[1] = 0x01;
    cmd[2] = 0x0A; /* Clear */

    qint64 size = writeAsyncData(cmd);
    if ( size != 3 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
    }

    return true;
}

void CWWinKey::__close()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Waiting for WriteBuffer Mutex";
    writeBufferMutex.lock();
    qCDebug(runtime) << "Clearing Buffer";
    writeBuffer.clear();
    writeBufferMutex.unlock();

    if ( serial.isOpen() )
    {
        /* Switch to Sync Mode */
        disconnect(&serial, &QSerialPort::bytesWritten, this, &CWWinKey::handleBytesWritten);
        disconnect(&serial, &QSerialPort::errorOccurred, this, &CWWinKey::handleError);
        disconnect(&serial, &QSerialPort::readyRead, this, &CWWinKey::handleReadyRead);

        if ( isInHostMode )
        {
            QByteArray cmd;

            /***********************/
            /* clear buffer        */
            /***********************/
            qCDebug(runtime) << "Clearing Buffer";
            cmd.resize(3);
            cmd[0] = 0x06; /* Stop */
            cmd[1] = 0x01;
            cmd[2] = 0x0A; /* Clear */

            if ( sendDataAndWait(cmd) != 3 )
            {
                qWarning() << "Unexpected size of write response or communication error";
                qCDebug(runtime) << lastError();
            }
            else
            {
                qCDebug(runtime) << "Buffered has been cleared";
            }

            /***********************/
            /* Disabling Host Mode */
            /***********************/
            qCDebug(runtime) << "Disabling Host Mode";
            cmd.resize(2);
            cmd[0] = 0x00;
            cmd[1] = 0x03;

            if ( sendDataAndWait(cmd) != 2 )
            {
                qWarning() << "Unexpected size of write response or communication error";
                qCDebug(runtime) << lastError();
            }
            else
            {
                qCDebug(runtime) << "Host Mode has been disabled";
            }
        }
        QThread::msleep(200);
        serial.setDataTerminalReady(false);
        serial.close();
    }
    else
    {
        qCDebug(runtime) << "Port is already closed";
    }

    isInHostMode = false;
    xoff = false;
    version = 0;
    lastLogicalError = QString();
}

unsigned char CWWinKey::buildWKModeByte() const
{
    FCT_IDENTIFICATION;

    /*
       7   (MSB) Disable Paddle watchdog
       6   Paddle Echoback (1=Enabled, 0=Disabled)
       5,4 Key Mode: 00 = Iambic B
                     01 = Iambic A
                     10 = Ultimatic
                     11 = Bug Mode
       3   Paddle Swap (1=Swap, 0=Normal)
       2   Serial Echoback (1=Enabled, 0=Disabled)
       1   Autospace (1=Enabled, 0=Disabled)
       0   (LSB) CT Spacing when=1, Normal Wordspace when=0
     */

    unsigned char settingByte = 0;

    settingByte |= 1 << 7;  // Disabled Paddle Watchdog
    settingByte |= 0 << 6;  // Paddle Echoback - Even Disable, characters are sent - K3NG Keyer

    switch (keyMode)        // Key Mode
    {
    case IAMBIC_B:
    case LAST_MODE:
        //no action 00
        break;
    case IAMBIC_A:
        settingByte |= 1 << 4;
        break;
    case ULTIMATE:
        settingByte |= 1 << 5;
        break;
    case SINGLE_PADDLE:
        settingByte |= 1 << 5;
        settingByte |= 1 << 4;
        break;
    }

    settingByte |= paddleSwap << 3;   // Paddle Swap Normal
    settingByte |= 1 << 2;  // Serial Echoback Enabled - must be
    //1     = 0             // Autospace Disabled
    //0     = 0             // Normal Wordspace

    return settingByte;
}

bool CWWinKey::__sendStatusRequest()
{
    FCT_IDENTIFICATION;

    if ( !isInHostMode )
    {
        qCWarning(runtime) << "Key is not in Host Mode";
        return false;
    }

    QByteArray cmd;
    cmd.resize(1);
    cmd[0] = 0x15;

    qint64 size = writeAsyncData(cmd);
    if ( size != 1 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
    }

    return true;
}

bool CWWinKey::__setPOTRange()
{
    FCT_IDENTIFICATION;

    if ( !isInHostMode )
    {
        qCWarning(runtime) << "Key is not in Host Mode";
        return false;
    }

    if ( defaultWPMSpeed <= 0 )
    {
        qCDebug(runtime) << "Default WPM Speed is negative" << defaultWPMSpeed;
        return false;
    }


    qCDebug(runtime) << "Key POT Range will be" << minWPMRange << minWPMRange + 31;

    QByteArray cmd;
    cmd.resize(4);
    cmd[0] = 0x05;
    cmd[1] = minWPMRange;
    cmd[2] = 31;
    cmd[3] = 0xFFu;

    qint64 size = writeAsyncData(cmd);
    if ( size != 4 )
    {
        qWarning() << "Unexpected size of write response or communication error";
        qCDebug(runtime) << lastError();
    }

    return true;
}

