#ifndef CWWINKEY_H
#define CWWINKEY_H

#include <QMutex>
#include "CWKey.h"

class CWWinKey2 : public CWKey
{
public:
    explicit CWWinKey2(const QString &portName,
                       const qint32 baudrate,
                       QObject *parent = nullptr);
    virtual bool open() override;
    virtual bool close() override;
    virtual bool sendText(QString text) override;
    virtual bool setWPM(qint16 wpm) override;
    virtual bool setMode(CWKeyModeID mode) override;
    virtual QString lastError() override;

private:
    virtual qint64 sendBytes(const QByteArray &requestData);
    virtual qint64 receiveBytes(QByteArray &data);

    bool isUsed;
    QMutex portMutex, sendBufferMutex;
    void __close();
    QByteArray writeBuffer;
    bool xoff;
    QSerialPort serial;
    qint32 timeout;

    void tryAsyncWrite();

private slots:

    void handleBytesWritten(qint64 bytes);
    void handleError(QSerialPort::SerialPortError serialPortError);
    void handleReadyRead();

};

#endif // CWWINKEY_H
