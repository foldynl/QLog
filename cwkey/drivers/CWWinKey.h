#ifndef QLOG_CWKEY_DRIVERS_CWWINKEY_H
#define QLOG_CWKEY_DRIVERS_CWWINKEY_H

#include <QMutex>
#include "CWKey.h"

class CWWinKey : public CWKey,
                  protected CWKeySerialInterface
{
    Q_OBJECT

public:
    explicit CWWinKey(const QString &portName,
                      const qint32 baudrate,
                      const CWKey::CWKeyModeID mode,
                      const qint32 defaultSpeed,
                      bool paddleSwap,
                      QObject *parent = nullptr);
    virtual ~CWWinKey();

    virtual bool open() override;
    virtual bool close() override;
    virtual QString lastError() override;

    virtual bool sendText(const QString &text) override;
    virtual bool setWPM(const qint16 wpm) override;
    virtual bool immediatelyStop() override;

private:

    bool isInHostMode;
    bool xoff;
    bool paddleSwap;

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
    unsigned char version;

private slots:
    void handleBytesWritten(qint64 bytes);
    void handleError(QSerialPort::SerialPortError serialPortError);
    void handleReadyRead();
};

#endif // QLOG_CWKEY_DRIVERS_CWWINKEY_H
