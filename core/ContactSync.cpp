#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "ContactSync.h"
#include "LogParam.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.contactsync");

namespace {

constexpr const char *SUBDIR_ROOT  = "qlog-sync";
constexpr const char *SUBDIR_NODES = "nodes";

QString currentTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ"));
}

QString journalFileName(int seq)
{
    return QStringLiteral("journal-%1.jsonl").arg(seq, 4, 10, QChar('0'));
}

} // namespace

ContactSync *ContactSync::instance()
{
    static ContactSync inst;
    return &inst;
}

ContactSync::ContactSync(QObject *parent)
    : QObject(parent),
      timer(new QTimer(this)),
      currentSeq(1),
      currentBytes(0),
      busyFlag(0)
{
    timer->setInterval(FLUSH_INTERVAL_MS);
    connect(timer, &QTimer::timeout, this, &ContactSync::onTick);
}

//
// ---- main-thread getters/setters (use the default DB connection) ----
//

bool ContactSync::isEnabled() const
{
    return LogParam::getSyncEnabled();
}

QString ContactSync::folder() const
{
    return LogParam::getSyncFolder();
}

QString ContactSync::selfNodeId() const
{
    QSqlQuery q;
    if ( !q.exec("SELECT value FROM sync_runtime WHERE key = 'self_node_id'") )
        return LogParam::getLogID();
    if ( q.next() )
        return q.value(0).toString();
    return LogParam::getLogID();
}

bool ContactSync::setSelfNodeId(const QString &nodeId, QString *errorOut)
{
    const QString trimmed = nodeId.trimmed();
    if ( trimmed.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Node ID cannot be empty");
        return false;
    }

    QSqlQuery q;
    if ( !q.prepare("INSERT OR REPLACE INTO sync_runtime (key, value) VALUES ('self_node_id', :v)") )
    {
        if ( errorOut ) *errorOut = q.lastError().text();
        return false;
    }
    q.bindValue(":v", trimmed);
    if ( !q.exec() )
    {
        if ( errorOut ) *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

QDateTime ContactSync::lastFlushTime() const
{
    const QString stored = LogParam::getSyncLastFlush();
    if ( stored.isEmpty() )
        return QDateTime();
    return QDateTime::fromString(stored, Qt::ISODateWithMs);
}

QDateTime ContactSync::lastPullTime() const
{
    const QString stored = LogParam::getSyncLastPull();
    if ( stored.isEmpty() )
        return QDateTime();
    return QDateTime::fromString(stored, Qt::ISODateWithMs);
}

ContactSync::DuplicatePolicy ContactSync::duplicatePolicy() const
{
    return static_cast<DuplicatePolicy>(LogParam::getSyncDuplicatePolicy());
}

void ContactSync::setDuplicatePolicy(DuplicatePolicy policy)
{
    LogParam::setSyncDuplicatePolicy(static_cast<int>(policy));
}

bool ContactSync::setEnabled(bool enabled, QString *errorOut)
{
    if ( enabled )
    {
        snapshotForCycle();
        if ( !ensureFolderLayout(errorOut) )
            return false;
    }
    LogParam::setSyncEnabled(enabled);
    if ( enabled )
        start();
    else
        stop();
    return true;
}

bool ContactSync::setFolder(const QString &path, QString *errorOut)
{
    LogParam::setSyncFolder(path);
    if ( isEnabled() )
    {
        snapshotForCycle();
        return ensureFolderLayout(errorOut);
    }
    return true;
}

void ContactSync::start()
{
    if ( !isEnabled() )
        return;
    if ( !timer->isActive() )
        timer->start();
}

void ContactSync::stop()
{
    timer->stop();
}

int ContactSync::pendingChangesCount() const
{
    if ( !isEnabled() )
        return 0;

    const QString self = selfNodeId();
    if ( self.isEmpty() )
        return 0;

    const QString upsertWm = LogParam::getSyncUpsertWatermark();
    const QString deleteWm = LogParam::getSyncDeleteWatermark();

    int total = 0;

    QString sql1 = QStringLiteral("SELECT COUNT(*) FROM contacts "
                                  "WHERE origin_node = :node "
                                  "  AND updated_at IS NOT NULL");
    if ( !upsertWm.isEmpty() )
        sql1 += QStringLiteral(" AND updated_at > :wm");

    QSqlQuery q1;
    if ( q1.prepare(sql1) )
    {
        q1.bindValue(":node", self);
        if ( !upsertWm.isEmpty() )
            q1.bindValue(":wm", upsertWm);
        if ( q1.exec() && q1.next() )
            total += q1.value(0).toInt();
    }

    QString sql2 = QStringLiteral("SELECT COUNT(*) FROM contacts_tombstones "
                                  "WHERE origin_node = :node");
    if ( !deleteWm.isEmpty() )
        sql2 += QStringLiteral(" AND deleted_at > :wm");

    QSqlQuery q2;
    if ( q2.prepare(sql2) )
    {
        q2.bindValue(":node", self);
        if ( !deleteWm.isEmpty() )
            q2.bindValue(":wm", deleteWm);
        if ( q2.exec() && q2.next() )
            total += q2.value(0).toInt();
    }

    return total;
}

//
// ---- folder layout / journal file helpers (no DB) ----
//

void ContactSync::snapshotForCycle()
{
    cycleFolder = folder();        // LogParam — main-thread safe
    cycleSelf   = selfNodeId();    // sync_runtime via default conn — main-thread safe
}

QString ContactSync::rootDir() const
{
    if ( cycleFolder.isEmpty() )
        return QString();
    return QDir(cycleFolder).filePath(SUBDIR_ROOT);
}

QString ContactSync::nodeDir() const
{
    const QString root = rootDir();
    if ( root.isEmpty() || cycleSelf.isEmpty() )
        return QString();
    return QDir(root).filePath(QStringLiteral("%1/%2").arg(SUBDIR_NODES, cycleSelf));
}

QString ContactSync::manifestPath() const
{
    const QString root = rootDir();
    return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("manifest.json"));
}

QString ContactSync::headPath() const
{
    const QString nd = nodeDir();
    return nd.isEmpty() ? QString() : QDir(nd).filePath(QStringLiteral("HEAD"));
}

QString ContactSync::journalPath(int seq) const
{
    const QString nd = nodeDir();
    return nd.isEmpty() ? QString() : QDir(nd).filePath(journalFileName(seq));
}

QString ContactSync::currentJournalPath() const
{
    return journalPath(currentSeq);
}

bool ContactSync::ensureFolderLayout(QString *errorOut)
{
    FCT_IDENTIFICATION;

    if ( cycleFolder.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Sync folder is not set");
        return false;
    }
    if ( cycleSelf.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Log ID is not set");
        return false;
    }

    QDir dir;
    if ( !dir.mkpath(nodeDir()) )
    {
        if ( errorOut ) *errorOut = tr("Cannot create sync folder: %1").arg(nodeDir());
        return false;
    }

    if ( !writeManifestIfMissing() )
    {
        if ( errorOut ) *errorOut = tr("Cannot write manifest");
        return false;
    }

    return loadHead();
}

bool ContactSync::writeManifestIfMissing()
{
    const QString path = manifestPath();
    if ( path.isEmpty() )
        return false;
    if ( QFile::exists(path) )
        return true;

    QJsonObject obj;
    obj["version"] = JOURNAL_FORMAT_VERSION;
    obj["created_at"] = currentTimestamp();

    QSaveFile f(path);
    if ( !f.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return f.commit();
}

bool ContactSync::loadHead()
{
    const QString path = headPath();
    if ( path.isEmpty() )
        return false;

    if ( !QFile::exists(path) )
    {
        currentSeq = 1;
        currentBytes = 0;
        QFile create(currentJournalPath());
        if ( !create.open(QIODevice::WriteOnly | QIODevice::Append) )
            return false;
        create.close();
        return writeHead();
    }

    QFile f(path);
    if ( !f.open(QIODevice::ReadOnly) )
        return false;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    const QString fileName = obj.value("file").toString();
    currentSeq = 1;
    if ( fileName.startsWith("journal-") && fileName.endsWith(".jsonl") )
    {
        const QString seqStr = fileName.mid(8, fileName.size() - 8 - 6);
        bool ok = false;
        const int parsed = seqStr.toInt(&ok);
        if ( ok && parsed >= 1 )
            currentSeq = parsed;
    }
    currentBytes = QFileInfo(currentJournalPath()).size();
    return true;
}

bool ContactSync::writeHead()
{
    const QString path = headPath();
    if ( path.isEmpty() )
        return false;

    QJsonObject obj;
    obj["file"] = journalFileName(currentSeq);
    obj["bytes"] = static_cast<qint64>(currentBytes);
    obj["updated_at"] = currentTimestamp();

    QSaveFile f(path);
    if ( !f.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return f.commit();
}

bool ContactSync::rotateIfNeeded()
{
    if ( currentBytes < MAX_JOURNAL_BYTES )
        return true;
    currentSeq += 1;
    currentBytes = 0;
    return true;
}

bool ContactSync::appendRecord(const QByteArray &line, QString *errorOut)
{
    if ( !rotateIfNeeded() )
    {
        if ( errorOut ) *errorOut = tr("Cannot rotate journal");
        return false;
    }

    QFile f(currentJournalPath());
    if ( !f.open(QIODevice::WriteOnly | QIODevice::Append) )
    {
        if ( errorOut ) *errorOut = tr("Cannot open journal: %1").arg(f.errorString());
        return false;
    }
    QByteArray payload = line;
    payload.append('\n');
    if ( f.write(payload) != payload.size() )
    {
        if ( errorOut ) *errorOut = tr("Short write to journal");
        return false;
    }
    currentBytes += payload.size();
    return true;
}

//
// ---- worker-side DB helpers (use the passed-in cloned connection) ----
//
// Everything below takes `QSqlDatabase &db` and uses `QSqlQuery q(db)`. For
// now the dispatchers call these synchronously with the default connection,
// but the signature is set up so a future move back to a worker thread can
// pass in a per-call cloned connection without changing the call sites.
//

QSet<QString> ContactSync::contactColumnsOnDb(QSqlDatabase &db) const
{
    // Cache is per-process; the schema can't change at runtime.
    static QSet<QString> cache;
    static bool loaded = false;
    if ( !loaded )
    {
        QSqlQuery q(db);
        if ( q.exec(QStringLiteral("PRAGMA table_info(contacts)")) )
        {
            while ( q.next() )
                cache.insert(q.value(QStringLiteral("name")).toString());
            loaded = !cache.isEmpty();
        }
    }
    return cache;
}

bool ContactSync::flushOnDb(QSqlDatabase &db, int *upsertsOut, int *deletesOut,
                            QString *newUpsertWatermark,
                            QString *newDeleteWatermark, QString *errorOut)
{
    // Read self and current watermarks from the worker's connection. The
    // worker never writes to log_param; new watermark values come back to
    // the main thread via the out-params below, and the queued lambda
    // commits them through LogParam (so its cache stays coherent).

    QString self;
    {
        QSqlQuery q(db);
        if ( q.exec(QStringLiteral("SELECT value FROM sync_runtime "
                                   "WHERE key = 'self_node_id'")) && q.next() )
            self = q.value(0).toString();
    }

    auto readParam = [&db](const QString &name) -> QString
    {
        QSqlQuery q(db);
        if ( !q.prepare(QStringLiteral("SELECT value FROM log_param WHERE name = :n")) )
            return QString();
        q.bindValue(QStringLiteral(":n"), name);
        if ( !q.exec() || !q.next() ) return QString();
        return q.value(0).toString();
    };

    const QString upsertWm = readParam(QStringLiteral("sync/last_journaled_updated_at"));
    const QString deleteWm = readParam(QStringLiteral("sync/last_journaled_deleted_at"));

    // ---- upserts ----
    int upserts = 0;
    QString maxUpsert = upsertWm;
    {
        QString sql = QStringLiteral("SELECT * FROM contacts "
                                     "WHERE origin_node = :node "
                                     "  AND updated_at IS NOT NULL");
        if ( !upsertWm.isEmpty() )
            sql += QStringLiteral(" AND updated_at > :wm");
        sql += QStringLiteral(" ORDER BY updated_at ASC, id ASC");

        QSqlQuery q(db);
        if ( !q.prepare(sql) )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }
        q.bindValue(":node", self);
        if ( !upsertWm.isEmpty() )
            q.bindValue(":wm", upsertWm);
        if ( !q.exec() )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }

        while ( q.next() )
        {
            const QSqlRecord rec = q.record();
            QJsonObject payload;
            for ( int i = 0; i < rec.count(); ++i )
            {
                const QString name = rec.fieldName(i);
                if ( name == QLatin1String("id") )
                    continue;
                const QVariant v = rec.value(i);
                if ( v.isNull() )
                    continue;
                payload.insert(name, QJsonValue::fromVariant(v));
            }

            QJsonObject record;
            record["v"]          = JOURNAL_FORMAT_VERSION;
            record["op"]         = QStringLiteral("upsert");
            record["qso_uuid"]   = rec.value("qso_uuid").toString();
            record["updated_at"] = rec.value("updated_at").toString();
            record["node"]       = self;
            record["payload"]    = payload;

            const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
            if ( !appendRecord(line, errorOut) )
                return false;

            const QString updatedAt = rec.value("updated_at").toString();
            if ( updatedAt > maxUpsert )
                maxUpsert = updatedAt;
            ++upserts;
        }
    }

    // ---- deletes ----
    int deletes = 0;
    QString maxDelete = deleteWm;
    {
        QString sql = QStringLiteral("SELECT qso_uuid, deleted_at, origin_node "
                                     "FROM contacts_tombstones "
                                     "WHERE origin_node = :node");
        if ( !deleteWm.isEmpty() )
            sql += QStringLiteral(" AND deleted_at > :wm");
        sql += QStringLiteral(" ORDER BY deleted_at ASC");

        QSqlQuery q(db);
        if ( !q.prepare(sql) )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }
        q.bindValue(":node", self);
        if ( !deleteWm.isEmpty() )
            q.bindValue(":wm", deleteWm);
        if ( !q.exec() )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }

        while ( q.next() )
        {
            QJsonObject record;
            record["v"]          = JOURNAL_FORMAT_VERSION;
            record["op"]         = QStringLiteral("delete");
            record["qso_uuid"]   = q.value("qso_uuid").toString();
            record["updated_at"] = q.value("deleted_at").toString();
            record["node"]       = self;

            const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
            if ( !appendRecord(line, errorOut) )
                return false;

            const QString deletedAt = q.value("deleted_at").toString();
            if ( deletedAt > maxDelete )
                maxDelete = deletedAt;
            ++deletes;
        }
    }

    if ( !writeHead() )
    {
        if ( errorOut ) *errorOut = tr("Cannot update HEAD");
        return false;
    }

    if ( upsertsOut ) *upsertsOut = upserts;
    if ( deletesOut ) *deletesOut = deletes;
    if ( newUpsertWatermark && upserts > 0 ) *newUpsertWatermark = maxUpsert;
    if ( newDeleteWatermark && deletes > 0 ) *newDeleteWatermark = maxDelete;

    return true;
}

bool ContactSync::applyUpsertOnDb(QSqlDatabase &db, const QJsonObject &rec,
                                  QString *errorOut)
{
    const QString qsoUuid   = rec.value("qso_uuid").toString();
    const QString updatedAt = rec.value("updated_at").toString();
    const QJsonObject payload = rec.value("payload").toObject();

    // Read duplicate policy from the worker's own connection so we never
    // touch LogParam from a background thread.
    int policyValue = 1;
    {
        QSqlQuery q(db);
        if ( q.prepare(QStringLiteral("SELECT value FROM log_param "
                                      "WHERE name = 'sync/duplicate_policy'"))
             && q.exec() && q.next() )
        {
            policyValue = q.value(0).toInt();
        }
    }
    const DuplicatePolicy policy = static_cast<DuplicatePolicy>(policyValue);

    // LWW vs existing local row by qso_uuid.
    {
        QSqlQuery check(db);
        check.prepare("SELECT updated_at FROM contacts WHERE qso_uuid = :u");
        check.bindValue(":u", qsoUuid);
        if ( !check.exec() )
        {
            if ( errorOut ) *errorOut = check.lastError().text();
            return false;
        }
        if ( check.next() )
        {
            const QString local = check.value(0).toString();
            if ( !local.isEmpty() && local >= updatedAt )
                return true;

            QStringList sets;
            QVariantMap binds;
            const QSet<QString> cols = contactColumnsOnDb(db);
            for ( auto it = payload.constBegin(); it != payload.constEnd(); ++it )
            {
                const QString &col = it.key();
                if ( col == QLatin1String("id") || col == QLatin1String("qso_uuid") )
                    continue;
                if ( !cols.contains(col) )
                    continue;
                sets << col + " = :" + col;
                binds.insert(col, it.value().toVariant());
            }
            QSqlQuery upd(db);
            const QString sql = "UPDATE contacts SET " + sets.join(", ") +
                                " WHERE qso_uuid = :where_uuid";
            if ( !upd.prepare(sql) )
            {
                if ( errorOut ) *errorOut = upd.lastError().text();
                return false;
            }
            for ( auto it = binds.constBegin(); it != binds.constEnd(); ++it )
                upd.bindValue(":" + it.key(), it.value());
            upd.bindValue(":where_uuid", qsoUuid);
            if ( !upd.exec() )
            {
                if ( errorOut ) *errorOut = upd.lastError().text();
                return false;
            }
            return true;
        }
    }

    // Tombstone wins over older incoming insert.
    {
        QSqlQuery tomb(db);
        tomb.prepare("SELECT deleted_at FROM contacts_tombstones WHERE qso_uuid = :u");
        tomb.bindValue(":u", qsoUuid);
        if ( tomb.exec() && tomb.next() )
        {
            const QString deletedAt = tomb.value(0).toString();
            if ( !deletedAt.isEmpty() && deletedAt >= updatedAt )
                return true;
        }
    }

    // Fingerprint-based duplicate detection (callsign + mode + band +
    // sat_name + start_time ±30 min). Mirrors the ADIF import dupe query.
    {
        const QString fpCall    = payload.value("callsign").toString();
        const QString fpMode    = payload.value("mode").toString();
        const QString fpBand    = payload.value("band").toString();
        const QString fpSat     = payload.value("sat_name").toString();
        const QString fpStart   = payload.value("start_time").toString();

        if ( !fpCall.isEmpty() && !fpMode.isEmpty() && !fpBand.isEmpty()
             && !fpStart.isEmpty() )
        {
            QSqlQuery dup(db);
            if ( dup.prepare("SELECT qso_uuid FROM contacts "
                             "WHERE callsign = upper(:callsign) "
                             "  AND upper(mode) = upper(:mode) "
                             "  AND upper(band) = upper(:band) "
                             "  AND COALESCE(sat_name, '') = COALESCE(:sat_name, '') "
                             "  AND ABS(JULIANDAY(start_time) - "
                             "          JULIANDAY(datetime(:startdate))) * 24 * 60 < 30 "
                             "LIMIT 1") )
            {
                dup.bindValue(":callsign",  fpCall);
                dup.bindValue(":mode",      fpMode);
                dup.bindValue(":band",      fpBand);
                dup.bindValue(":sat_name",  fpSat.isEmpty() ? QVariant() : QVariant(fpSat));
                dup.bindValue(":startdate", fpStart);

                if ( dup.exec() && dup.next() )
                {
                    const QString localUuid = dup.value(0).toString();

                    if ( policy == DuplicatePolicy::Skip )
                    {
                        qCInfo(runtime) << "Sync dup-skip:" << fpCall
                                        << "remote uuid" << qsoUuid
                                        << "local uuid" << localUuid;
                        return true;
                    }

                    if ( qsoUuid < localUuid )
                    {
                        QSqlQuery adopt(db);
                        adopt.prepare("UPDATE contacts SET qso_uuid = :new "
                                      "WHERE qso_uuid = :old");
                        adopt.bindValue(":new", qsoUuid);
                        adopt.bindValue(":old", localUuid);
                        if ( !adopt.exec() )
                        {
                            if ( errorOut ) *errorOut = adopt.lastError().text();
                            return false;
                        }
                        qCInfo(runtime) << "Sync dup-merge: local" << localUuid
                                        << "→ remote" << qsoUuid;
                    }
                    return true;
                }
            }
        }
    }

    // Fresh INSERT with all sync columns pre-set so the AFTER INSERT auto-fill
    // trigger's WHEN-guard skips and preserves the remote node id and ts.
    QStringList cols;
    QStringList placeholders;
    QVariantMap binds;
    const QSet<QString> known = contactColumnsOnDb(db);
    for ( auto it = payload.constBegin(); it != payload.constEnd(); ++it )
    {
        const QString &col = it.key();
        if ( col == QLatin1String("id") )
            continue;
        if ( !known.contains(col) )
            continue;
        cols << col;
        placeholders << ":" + col;
        binds.insert(col, it.value().toVariant());
    }

    QSqlQuery ins(db);
    const QString sql = "INSERT INTO contacts (" + cols.join(", ") + ") "
                        "VALUES (" + placeholders.join(", ") + ")";
    if ( !ins.prepare(sql) )
    {
        if ( errorOut ) *errorOut = ins.lastError().text();
        return false;
    }
    for ( auto it = binds.constBegin(); it != binds.constEnd(); ++it )
        ins.bindValue(":" + it.key(), it.value());
    if ( !ins.exec() )
    {
        if ( errorOut ) *errorOut = ins.lastError().text();
        return false;
    }
    return true;
}

bool ContactSync::applyDeleteOnDb(QSqlDatabase &db, const QString &qsoUuid,
                                  const QString &deletedAt,
                                  const QString &origin, QString *errorOut)
{
    QSqlQuery check(db);
    check.prepare("SELECT updated_at FROM contacts WHERE qso_uuid = :u");
    check.bindValue(":u", qsoUuid);
    if ( !check.exec() )
    {
        if ( errorOut ) *errorOut = check.lastError().text();
        return false;
    }
    if ( check.next() )
    {
        const QString local = check.value(0).toString();
        if ( !local.isEmpty() && local > deletedAt )
            return true;
        QSqlQuery del(db);
        del.prepare("DELETE FROM contacts WHERE qso_uuid = :u");
        del.bindValue(":u", qsoUuid);
        if ( !del.exec() )
        {
            if ( errorOut ) *errorOut = del.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery tomb(db);
    tomb.prepare("INSERT OR REPLACE INTO contacts_tombstones "
                 "(qso_uuid, deleted_at, origin_node) VALUES (:u, :d, :o)");
    tomb.bindValue(":u", qsoUuid);
    tomb.bindValue(":d", deletedAt);
    tomb.bindValue(":o", origin);
    if ( !tomb.exec() )
    {
        if ( errorOut ) *errorOut = tomb.lastError().text();
        return false;
    }
    return true;
}

bool ContactSync::applyJournalLineOnDb(QSqlDatabase &db, const QByteArray &line,
                                       QString *errorOut)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if ( parseErr.error != QJsonParseError::NoError )
    {
        if ( errorOut ) *errorOut = parseErr.errorString();
        return false;
    }
    const QJsonObject rec = doc.object();

    if ( rec.value("v").toInt() != JOURNAL_FORMAT_VERSION )
    {
        if ( errorOut ) *errorOut = tr("unknown record version");
        return false;
    }

    const QString op       = rec.value("op").toString();
    const QString qsoUuid  = rec.value("qso_uuid").toString();
    const QString updatedAt = rec.value("updated_at").toString();
    const QString origin   = rec.value("node").toString();

    if ( qsoUuid.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("missing qso_uuid");
        return false;
    }

    if ( op == QLatin1String("upsert") )
        return applyUpsertOnDb(db, rec, errorOut);

    if ( op == QLatin1String("delete") )
        return applyDeleteOnDb(db, qsoUuid, updatedAt, origin, errorOut);

    if ( errorOut ) *errorOut = tr("unknown op: %1").arg(op);
    return false;
}

bool ContactSync::pullPeerOnDb(QSqlDatabase &db, const QString &peerNode,
                               const QString &peerDir, int *appliedOut,
                               QString *errorOut)
{
    QString lastFile;
    qint64  lastOffset = 0;
    {
        QSqlQuery q(db);
        if ( q.prepare("SELECT last_file, last_offset FROM sync_peers WHERE node = :n") )
        {
            q.bindValue(":n", peerNode);
            if ( q.exec() && q.next() )
            {
                lastFile   = q.value(0).toString();
                lastOffset = q.value(1).toLongLong();
            }
        }
    }

    QDir peerD(peerDir);
    const QStringList journals = peerD.entryList(QStringList() << "journal-*.jsonl",
                                                 QDir::Files, QDir::Name);

    int applied = 0;
    QString    finalFile   = lastFile;
    qint64     finalOffset = lastOffset;

    for ( const QString &name : journals )
    {
        if ( !lastFile.isEmpty() && name < lastFile )
            continue;

        QFile f(peerD.filePath(name));
        if ( !f.open(QIODevice::ReadOnly) )
            continue;

        const qint64 startAt = (name == lastFile) ? lastOffset : 0;
        if ( !f.seek(startAt) )
        {
            f.close();
            continue;
        }

        while ( !f.atEnd() )
        {
            const QByteArray line = f.readLine();
            if ( line.trimmed().isEmpty() )
                break;

            QString err;
            if ( applyJournalLineOnDb(db, line, &err) )
                ++applied;
            else
                qCWarning(runtime) << "Skipping record from" << peerNode << ":" << err;

            finalFile   = name;
            finalOffset = f.pos();
        }
        f.close();
    }

    QSqlQuery upd(db);
    if ( upd.prepare("INSERT OR REPLACE INTO sync_peers "
                     "(node, last_file, last_offset, last_seen_at) "
                     "VALUES (:n, :f, :o, :t)") )
    {
        upd.bindValue(":n", peerNode);
        upd.bindValue(":f", finalFile);
        upd.bindValue(":o", static_cast<qlonglong>(finalOffset));
        upd.bindValue(":t", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if ( !upd.exec() && errorOut )
            *errorOut = upd.lastError().text();
    }

    if ( appliedOut ) *appliedOut = applied;
    return true;
}

bool ContactSync::pullOnDb(QSqlDatabase &db, int *appliedOut, QString *errorOut)
{
    QString self;
    {
        QSqlQuery q(db);
        if ( q.exec("SELECT value FROM sync_runtime WHERE key = 'self_node_id'")
             && q.next() )
            self = q.value(0).toString();
    }

    const QString nodesPath = QDir(rootDir()).filePath(SUBDIR_NODES);
    QDir nodesDir(nodesPath);
    if ( !nodesDir.exists() )
    {
        if ( appliedOut ) *appliedOut = 0;
        return true;
    }

    int totalApplied = 0;
    QStringList errors;

    const QStringList peers = nodesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for ( const QString &peer : peers )
    {
        if ( peer == self )
            continue;
        int applied = 0;
        QString err;
        if ( !pullPeerOnDb(db, peer, nodesDir.filePath(peer), &applied, &err) )
            errors << QString("%1: %2").arg(peer, err);
        totalApplied += applied;
    }

    if ( appliedOut ) *appliedOut = totalApplied;
    if ( !errors.isEmpty() )
    {
        if ( errorOut ) *errorOut = errors.join(QStringLiteral("; "));
        return false;
    }
    return true;
}

//
// ---- main-thread dispatchers ----
//

void ContactSync::setBusy(bool busy)
{
    emit busyChanged(busy);
}

void ContactSync::runFlushCycle()
{
    // NOTE: Temporarily reverted to synchronous-on-main-thread to isolate
    // whether the worker thread / connection cloning was the root cause of
    // the "SET - Cannot exec an insert parameter statement" errors. If this
    // version runs cleanly we'll know it's a threading issue.

    if ( !isEnabled() )
        return;
    if ( !busyFlag.testAndSetAcquire(0, 1) )
        return;
    setBusy(true);

    snapshotForCycle();

    QString err;
    if ( !ensureFolderLayout(&err) )
    {
        busyFlag.storeRelease(0);
        setBusy(false);
        emit flushFailed(err);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    int upserts = 0, deletes = 0;
    QString newUpsertWm, newDeleteWm;
    const bool ok = flushOnDb(db, &upserts, &deletes,
                              &newUpsertWm, &newDeleteWm, &err);

    if ( ok )
    {
        if ( !newUpsertWm.isEmpty() && !LogParam::setSyncUpsertWatermark(newUpsertWm) )
            qCWarning(runtime) << "ContactSync: setSyncUpsertWatermark failed";
        if ( !newDeleteWm.isEmpty() && !LogParam::setSyncDeleteWatermark(newDeleteWm) )
            qCWarning(runtime) << "ContactSync: setSyncDeleteWatermark failed";
        lastFlush = QDateTime::currentDateTimeUtc();
        if ( !LogParam::setSyncLastFlush(lastFlush.toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastFlush failed";
        emit flushed(upserts, deletes);
    }
    else
    {
        emit flushFailed(err);
    }
    busyFlag.storeRelease(0);
    setBusy(false);
}

void ContactSync::runPullCycle()
{
    if ( !isEnabled() )
        return;
    if ( !busyFlag.testAndSetAcquire(0, 1) )
        return;
    setBusy(true);

    snapshotForCycle();

    QString err;
    if ( !ensureFolderLayout(&err) )
    {
        busyFlag.storeRelease(0);
        setBusy(false);
        emit pullFailed(err);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    int applied = 0;
    const bool ok = pullOnDb(db, &applied, &err);

    if ( ok )
    {
        if ( !LogParam::setSyncLastPull(
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastPull failed";
        emit pulled(applied);
    }
    else
    {
        emit pullFailed(err);
    }
    busyFlag.storeRelease(0);
    setBusy(false);
}

void ContactSync::runFullCycle()
{
    if ( !isEnabled() )
        return;
    if ( !busyFlag.testAndSetAcquire(0, 1) )
        return;
    setBusy(true);

    snapshotForCycle();

    QString preErr;
    if ( !ensureFolderLayout(&preErr) )
    {
        busyFlag.storeRelease(0);
        setBusy(false);
        emit flushFailed(preErr);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    int upserts = 0, deletes = 0, applied = 0;
    QString newUpsertWm, newDeleteWm;
    QString flushErr, pullErr;

    const bool flushOk = flushOnDb(db, &upserts, &deletes,
                                   &newUpsertWm, &newDeleteWm, &flushErr);
    const bool pullOk  = pullOnDb(db, &applied, &pullErr);

    if ( flushOk )
    {
        if ( !newUpsertWm.isEmpty() && !LogParam::setSyncUpsertWatermark(newUpsertWm) )
            qCWarning(runtime) << "ContactSync: setSyncUpsertWatermark failed";
        if ( !newDeleteWm.isEmpty() && !LogParam::setSyncDeleteWatermark(newDeleteWm) )
            qCWarning(runtime) << "ContactSync: setSyncDeleteWatermark failed";
        lastFlush = QDateTime::currentDateTimeUtc();
        if ( !LogParam::setSyncLastFlush(lastFlush.toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastFlush failed";
        emit flushed(upserts, deletes);
    }
    else
    {
        emit flushFailed(flushErr);
    }

    if ( pullOk )
    {
        if ( !LogParam::setSyncLastPull(
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastPull failed";
        emit pulled(applied);
    }
    else
    {
        emit pullFailed(pullErr);
    }

    busyFlag.storeRelease(0);
    setBusy(false);
}

void ContactSync::requestFlush()
{
    runFlushCycle();
}

void ContactSync::requestPull()
{
    runPullCycle();
}

void ContactSync::onTick()
{
    runFullCycle();
}
