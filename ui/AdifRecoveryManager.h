#ifndef QLOG_UI_ADIFRECOVERYMANAGER_H
#define QLOG_UI_ADIFRECOVERYMANAGER_H

#include <QMap>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QThread>

#include "core/AdifRecovery.h"
#include "logformat/LogFormat.h"

class AdifRecoveryManager : public QObject
{
    Q_OBJECT

public:
    explicit AdifRecoveryManager(QObject *parent = nullptr);
    ~AdifRecoveryManager();

signals:
    void contactsRecovered(int count);
    void problem(const QString &text);

public slots:
    void reloadSettings();
    void startStartupRecovery();

private slots:
    void startNextScan();
    void scanFinished(const AdifRecoveryScanResult &result);

private:
    const int MAX_AUTOMATIC_CONTACTS = 2000;

    static LogFormat::duplicateQSOBehaviour skipAllDuplicates(QSqlRecord *, QSqlRecord *);

    void importRecoveredContacts(const AdifRecoveryScanResult &result,
                                 const AdifRecoveryConfig &config);
    QMap<QString, QString> qslSentDefaults(const AdifRecoveryConfig &config) const;
    void saveState(const QString &fileKey,
                   const AdifRecoveryConfig &config,
                   qint64 offset,
                   const QString &message = QString());
    void cleanupWorker();
    QSet<QString> stationProfiles() const;
    bool isRunnableConfig(const AdifRecoveryConfig &config,
                          const QSet<QString> &profiles) const;
    void disableMissingProfileConfigs(const QSet<QString> &profiles);
    void rebuildPendingQueue(const QSet<QString> &profiles);

    QList<AdifRecoveryConfig> configs;
    QMap<QString, AdifRecoveryConfig> configByKey;
    QQueue<QString> pendingKeys;
    QThread *workerThread = nullptr;
    AdifRecoveryReaderWorker *worker = nullptr;
    bool reloadAfterScan = false;
};

#endif // QLOG_UI_ADIFRECOVERYMANAGER_H
