#ifndef QLOG_CWKEY_DRIVERS_CWWINKEY_H
#define QLOG_CWKEY_DRIVERS_CWWINKEY_H

#include <QMutex>
#include "CWKey.h"

class CWWinKey2 : public CWKey,
                  protected CWKeySerialInterface
{
    Q_OBJECT

public:
    explicit CWWinKey2(const QString &portName,
                       const qint32 baudrate,
                       const CWKey::CWKeyModeID mode,
                       const qint32 defaultSpeed,
                       QObject *parent = nullptr);
    virtual ~CWWinKey2();

    virtual bool open() override;
    virtual bool close() override;
    virtual QString lastError() override;

    virtual bool sendText(const QString &text) override;
    virtual bool setWPM(const qint16 wpm) override;
    virtual bool imediatellyStop() override;

private:

    bool isInHostMode;
    bool xoff;

    QMutex writeBufferMutex;
    QMutex commandMutex;
    QByteArray writeBuffer;
    qint32 minWPMRange;
    QString lastLogicalError;

    void tryAsyncWrite();
    unsigned char buildWKModeByte() const;
    bool __sendStatusRequest();
    bool __setPOTRange();
    bool __setWPM(const qint16 wpm);
    void __close();

private slots:
    void handleBytesWritten(qint64 bytes);
    void handleError(QSerialPort::SerialPortError serialPortError);
    void handleReadyRead();
};

#endif // QLOG_CWKEY_DRIVERS_CWWINKEY_H
