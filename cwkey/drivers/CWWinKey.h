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
                      bool paddleOnlySidetone,
                      qint32 sidetoneFrequency,
                      QObject *parent = nullptr);
    virtual ~CWWinKey();

    virtual bool open() override;
    virtual bool close() override;
    virtual QString lastError() override;

    virtual bool sendText(const QString &text) override;
    virtual bool setWPM(const qint16 wpm) override;
    virtual bool setOutput(CWKeyOutputID output) override;
    virtual bool immediatelyStop() override;

    static QList<QPair<QString, int>> sidetoneFrequencies();
    static int defaultSidetoneFrequency() { return 800; }

private:
    static constexpr unsigned char MIN_OUTPUT_PORT_SELECT_VERSION = 21;
    static constexpr unsigned char MIN_SIDETONE_VERSION = 20;

    bool isInHostMode;
    bool xoff;
    bool paddleSwap;
    bool paddleOnlySidetone;
    qint32 sidetoneFrequency;
    CWKeyOutputID output;

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
    bool __setSidetone(bool enabled);
    bool __setOutput(CWKeyOutputID output);
    bool __immediatelyStop();
    void __close();
    unsigned char version;

private slots:
    void handleBytesWritten(qint64 bytes);
    void handleError(QSerialPort::SerialPortError serialPortError);
    void handleReadyRead();
};

#endif // QLOG_CWKEY_DRIVERS_CWWINKEY_H
