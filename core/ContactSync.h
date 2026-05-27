#ifndef QLOG_CORE_CONTACTSYNC_H
#define QLOG_CORE_CONTACTSYNC_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QSet>
#include <QAtomicInt>

class QTimer;
class QJsonObject;
class QSqlDatabase;

class ContactSync : public QObject
{
    Q_OBJECT

public:
    // How to handle a remote upsert whose qso_uuid is unknown locally but
    // whose fingerprint (callsign + mode + band + sat_name + start_time
    // ±30 min) matches an existing local row.
    enum class DuplicatePolicy {
        Skip  = 0,  // log dupe and leave both rows independent
        Merge = 1   // converge on the lexicographically-smaller UUID, then
                    // let LWW reconcile fields (default)
    };

    static ContactSync *instance();

    bool    isEnabled() const;
    QString folder() const;
    QString selfNodeId() const;
    QDateTime lastFlushTime() const;
    QDateTime lastPullTime() const;

    bool setEnabled(bool enabled, QString *errorOut = nullptr);
    bool setFolder(const QString &path, QString *errorOut = nullptr);
    bool setSelfNodeId(const QString &nodeId, QString *errorOut = nullptr);

    DuplicatePolicy duplicatePolicy() const;
    void            setDuplicatePolicy(DuplicatePolicy policy);

    int  pendingChangesCount() const;

    void start();
    void stop();

public slots:
    // Run a flush cycle. Called by both the periodic timer and the local-
    // change signals (contactAdded/contactUpdated/Deleted). If a cycle is
    // already running, the call coalesces (the in-flight cycle picks up
    // everything above the watermark anyway).
    void requestFlush();
    void requestPull();

signals:
    void flushed(int upsertCount, int deleteCount);
    void flushFailed(const QString &message);
    void pulled(int appliedCount);
    void pullFailed(const QString &message);
    void busyChanged(bool busy);

private slots:
    void onTick();

private:
    explicit ContactSync(QObject *parent = nullptr);

    // ---- folder helpers (no DB) ----
    QString rootDir() const;
    QString nodeDir() const;
    QString manifestPath() const;
    QString headPath() const;
    QString journalPath(int seq) const;
    QString currentJournalPath() const;

    // ---- worker-side: every method takes the cloned connection it should
    //      use. They never touch the default (main-thread) connection.
    bool ensureFolderLayout(QString *errorOut);
    bool loadHead();
    bool writeHead();
    bool writeManifestIfMissing();
    bool appendRecord(const QByteArray &line, QString *errorOut);
    bool rotateIfNeeded();

    bool flushOnDb(QSqlDatabase &db, int *upsertsOut, int *deletesOut,
                   QString *newUpsertWatermark, QString *newDeleteWatermark,
                   QString *errorOut);

    bool pullOnDb(QSqlDatabase &db, int *appliedOut, QString *errorOut);
    bool pullPeerOnDb(QSqlDatabase &db, const QString &peerNode,
                      const QString &peerDir, int *appliedOut,
                      QString *errorOut);
    bool applyJournalLineOnDb(QSqlDatabase &db, const QByteArray &line,
                              QString *errorOut);
    bool applyUpsertOnDb(QSqlDatabase &db, const QJsonObject &record,
                         QString *errorOut);
    bool applyDeleteOnDb(QSqlDatabase &db, const QString &qsoUuid,
                         const QString &deletedAt, const QString &origin,
                         QString *errorOut);

    QSet<QString> contactColumnsOnDb(QSqlDatabase &db) const;

    // Dispatch helpers — open a per-call cloned SQLite connection, run the
    // work, post results back to this object via queued signals.
    void runFlushCycle();
    void runPullCycle();
    void runFullCycle();   // flush then pull (timer tick)

    void setBusy(bool busy);

    // Cache folder + self for the duration of one flush/pull cycle. Avoids
    // re-querying sync_runtime on every path-helper call. Also positions the
    // class for a future move back to a worker thread, where these would
    // need to be captured on the main thread before dispatch (the path
    // helpers must never call LogParam or QSqlDatabase::database() from a
    // background thread).
    void snapshotForCycle();

    QTimer        *timer;
    int            currentSeq;
    qint64         currentBytes;
    QDateTime      lastFlush;
    QAtomicInt     busyFlag;        // 0 = idle, 1 = cycle running
    QString        cycleFolder;     // snapshot of folder() for this cycle
    QString        cycleSelf;       // snapshot of selfNodeId() for this cycle

    static constexpr qint64 MAX_JOURNAL_BYTES = 4 * 1024 * 1024;
    static constexpr int    FLUSH_INTERVAL_MS = 30000;
    static constexpr int    JOURNAL_FORMAT_VERSION = 1;
};

#endif // QLOG_CORE_CONTACTSYNC_H
