#ifndef QLOG_CORE_ADIFRECOVERY_H
#define QLOG_CORE_ADIFRECOVERY_H

#include <QDateTime>
#include <QObject>

struct AdifRecoveryConfig
{
    bool enabled = true;
    QString stationProfileName;
    QString qslSentStatusDefault = "Q";
    QString path;
};

struct AdifRecoveryState
{
    QString path;
    qint64 offset = -1;
    QDateTime lastRecoveryAt;
    QString lastMessage;
};

struct AdifRecoveryScanResult
{
    QString fileKey;
    QString path;
    QString adifText;
    qint64 previousOffset = -1;
    qint64 nextOffset = -1;
    qint64 fileSize = -1;
    int contactCount = 0;
    bool reset = false;
    bool tooMany = false;
    QString message;
};

class AdifRecovery
{

public:
    static QString fileKey(const QString &path, const QString &stationProfileName);
    static QString normalizePath(const QString &path);

    static QString serializeConfigList(const QList<AdifRecoveryConfig> &configs);
    static QList<AdifRecoveryConfig> deserializeConfigList(const QString &data);

    static QString serializeState(const AdifRecoveryState &state);
    static AdifRecoveryState deserializeState(const QString &data);
};

class AdifRecoveryReaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit AdifRecoveryReaderWorker(QObject *parent = nullptr);

public slots:
    void readTail(const AdifRecoveryConfig &config,
                  const AdifRecoveryState &state,
                  int maxContacts);
private:
    const char ADIF_TAG_START = '<';
    const char *EOR_TAG = "<eor>";

    bool isAdifWhitespace(char ch)
    {
        return ch > 0 && ch <= ' ';
    }
signals:
    void scanFinished(const AdifRecoveryScanResult &result);
};

Q_DECLARE_METATYPE(AdifRecoveryConfig)
Q_DECLARE_METATYPE(AdifRecoveryState)
Q_DECLARE_METATYPE(AdifRecoveryScanResult)

#endif // QLOG_CORE_ADIFRECOVERY_H
