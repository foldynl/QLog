#include <QSqlDatabase>
#include <QDir>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QMutexLocker>
#include <QSqlDriver>
#include <QMessageBox>

#include "MembershipQE.h"
#include "core/debug.h"
#include "data/Data.h"
#include "data/Callsign.h"
#include "LogParam.h"

MODULE_IDENTIFICATION("qlog.core.membershipqe");

#define MEMBERLIST_BASE_URL "https://raw.githubusercontent.com/foldynl/hamradio-membeship-lists/main/lists"

ClubInfo::ClubInfo(const QString &callsign,
               const QString &ID,
               const QDate &validFrom,
               const QDate &validTo,
               const QString &club):
   callsign(callsign),
   id(ID),
   validFrom(validFrom),
   validTo(validTo),
   club(club)
{
    FCT_IDENTIFICATION;
}

const QString& ClubInfo::getCallsign() const
{
    return callsign;
}

const QString& ClubInfo::getID() const
{
    return id;
}

const QDate& ClubInfo::getValidFrom() const
{
    return validFrom;
}

const QDate& ClubInfo::getValidTo() const
{
    return validTo;
}

const QString& ClubInfo::getClubInfo() const
{
    return club;
}

MembershipQE::MembershipQE(QObject *parent)
    : QObject{parent},
      idClubQueryValid(false),
      nam(new QNetworkAccessManager(this))
{
    FCT_IDENTIFICATION;

    // this thead will help to obtain club status
    statusQuery.moveToThread(&statusQueryThread);
    statusQueryThread.start();

    connect(&statusQuery, &ClubStatusQuery::status, this, &MembershipQE::statusQueryFinished);
    connect(nam.data(), &QNetworkAccessManager::finished, this, &MembershipQE::onFinishedListDownload);

    // prepare SQL query to increase Club query function performance
    idClubQueryValid = clubQuery.prepare("SELECT DISTINCT callsign, member_id, valid_from, valid_to, clubid FROM membership WHERE callsign = :callsign ORDER BY clubid");
}

MembershipQE::~MembershipQE()
{
    FCT_IDENTIFICATION;

    statusQueryThread.quit();
    statusQueryThread.wait();
}

// this function is called when async club status returns a result
void MembershipQE::statusQueryFinished(const QString &callsign,
                                       QMap<QString, ClubStatusQuery::ClubInfo> statuses)
{
    FCT_IDENTIFICATION;

    emit clubStatusResult(callsign, statuses);
}

void MembershipQE::saveEnabledClubLists(const QStringList &enabledLists)
{
    FCT_IDENTIFICATION;

    LogParam::setEnabledMemberlists(enabledLists);
}

QStringList MembershipQE::getEnabledClubLists()
{
    FCT_IDENTIFICATION;

    return LogParam::getEnabledMemberlists();
}

void MembershipQE::removeClubsFromEnabledClubLists(const QList<QPair<QString, QString>> &toRemove)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << toRemove;
    QStringList current = getEnabledClubLists();

    for ( const QPair<QString, QString>& toRemoveClub : toRemove )
    {
        current.removeAll(toRemoveClub.first);
    }

    saveEnabledClubLists(current);
}

// it is a sync in-thread function to obtain all clubs for an input callsign
// it must be as fast as possible
QList<ClubInfo> MembershipQE::query(const QString &in_callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_callsign;

    QList<ClubInfo> ret;

    if ( !idClubQueryValid )
    {
        qCDebug(runtime) << "Query is not prepared";
        return ret;
    }

    Callsign qCall(in_callsign);

    clubQuery.bindValue(":callsign",( qCall.isValid() ) ? qCall.getBase() : in_callsign.toUpper());

    if ( ! clubQuery.exec() )
    {
        qCDebug(runtime) << "Cannot query callsign clubs "<< clubQuery.lastError().text();
        return ret;
    }

    while ( clubQuery.next() )
    {
        QString callsign = clubQuery.value(0).toString();
        QString memberid = clubQuery.value(1).toString();
        QDate validFrom = QDate::fromString(clubQuery.value(2).toString(), "yyyyMMdd");
        QDate validTo = QDate::fromString(clubQuery.value(3).toString(), "yyyyMMdd");
        QString clubid = clubQuery.value(4).toString();

        qCDebug(runtime) << "Found membership record" << callsign << memberid << validFrom << validTo << clubid;

        ret << ClubInfo(in_callsign, memberid, validTo, validTo, clubid);
    }

    qCDebug(runtime) << "Done";

    return ret;
}
// it is a async query function to obtain club statuses for an input callsign.
// the result is returned via signal MembershipStatusQuery::status
// it can take some time to obtain a result therefore it is solved in an isolated thread
void MembershipQE::asyncQueryDetails(const QString &callsign,
                                     const QString &band,
                                     const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign << band << mode;

    QMetaObject::invokeMethod(&statusQuery, "getClubStatus",
                              Qt::QueuedConnection,
                              Q_ARG(QString, callsign.toUpper()),
                              Q_ARG(QString, band),
                              Q_ARG(QString, mode),
                              Q_ARG(bool, LogParam::getDxccConfirmedByLotwState()),
                              Q_ARG(bool, LogParam::getDxccConfirmedByPaperState()),
                              Q_ARG(bool, LogParam::getDxccConfirmedByEqslState())
                              );
}

void MembershipQE::updateLists()
{
    FCT_IDENTIFICATION;

    if ( updatePlan.size() > 0 )
    {
        qWarning() << "Member Club lists are still downloaded. Aborting this update request.";
    }

    QStringList enabledLists = getEnabledClubLists();

    if ( !removeDisabled(enabledLists) )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Club List Update failed. Cannot remove old records"));
        return;
    }

    if ( !planDownloads(enabledLists) )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Club List Update failed. Cannot plan new downloads"));
        return;
    }

    startPlannedDownload();
}

bool MembershipQE::removeDisabled(const QStringList &enabledLists)
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( ! query.exec(QString("DELETE FROM membership WHERE clubid NOT IN ('%1');").arg(enabledLists.join("','"))))
    {
        qWarning() << "Cannot delete records "<< query.lastError().text();
        Q_UNUSED(QSqlDatabase::database().rollback());
        return false;
    }

    if ( ! query.exec(QString("DELETE FROM membership_versions WHERE clubid NOT IN (SELECT DISTINCT clubid FROM membership);")))
    {
        qWarning() << "Cannot delete records from versioning"<< query.lastError().text();
        Q_UNUSED(QSqlDatabase::database().rollback());
        return false;
    }

    qCDebug(runtime) << "DONE";

    Q_UNUSED(QSqlDatabase::database().commit());

    return true;
}

bool MembershipQE::planDownloads(const QStringList &enabledLists)
{
    FCT_IDENTIFICATION;

    QSqlQuery query;

    if ( ! query.exec(QString("WITH split(word, csv) AS ( SELECT '', '%1'||',' "
                              "                           UNION ALL "
                              "                           SELECT substr(csv, 0, instr(csv, ',')), substr(csv, instr(csv, ',') + 1) "
                              "                           FROM split "
                              "                           WHERE csv != '' ) "
                              "SELECT clubid, filename "
                              "FROM membership_directory d, "
                              "    (SELECT word clubid FROM split "
                              "     WHERE word!='' "
                              "     EXCEPT "
                              "     SELECT clubid  "
                              "     FROM membership_versions l , "
                              "          membership_directory m "
                              "     WHERE (l.clubid = m.short_desc) "
                              "     AND (version >= m.last_update or m.last_update IS NULL)) a "
                              "WHERE a.clubid = d.short_desc;").arg(enabledLists.join(","))))
    {
       qCWarning(runtime) << "Cannot plan download" << query.lastError().text();
       return false;
    }

    while ( query.next() )
    {
        updatePlan << QPair<QString, QString>(query.value(0).toString(),QString(MEMBERLIST_BASE_URL) + "/" + query.value(1).toString());
    }

    qCDebug(runtime)  << "DONE";

    return true;
}

void MembershipQE::startPlannedDownload()
{
    FCT_IDENTIFICATION;

    if ( updatePlan.size() == 0 )
        return;

    qCDebug(runtime) << "Remaining downloads" << updatePlan.size() << updatePlan;

    // to prevent network overload, qlog will donwload files one-by-one
    QPair<QString, QString> nextDownload = updatePlan.at(0);

    QNetworkRequest request(nextDownload.second);
    QString rheader = QString("QLog/%1").arg(VERSION);
    request.setRawHeader("User-Agent", rheader.toUtf8());
    QNetworkReply *reply = nam->get(request);
    reply->setProperty("clubid", nextDownload.first);
}

void MembershipQE::onFinishedListDownload(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    QString clubid = reply->property("clubid").toString();

    qCDebug(runtime) << "Received data for Club ID" << clubid
                     << "from" << reply->url().toString();

    if ( updatePlan.size() == 0 )
    {
        // Update plan was canceled
        qCDebug(runtime) << "Download finished for already canceled download - ignore it";
        return;
    }
    if ( updatePlan.at(0).first != clubid)
    {
        qCDebug(runtime) << "Received" << clubid << "but expected" << updatePlan.at(0).first;
        qCDebug(runtime) << "Canceling all downloads";
        removeClubsFromEnabledClubLists(updatePlan);
        updatePlan.clear();
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Critical"),
                             QMessageBox::tr("Unexpected Club List download. Canceling next downloads"));

    }

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool error = false;

    if ( reply->isFinished()
         && reply->error() == QNetworkReply::NoError
         && replyStatusCode >= 200 && replyStatusCode < 300)
    {
        QByteArray data = reply->readAll();        
        if ( ! importData(clubid, data) )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Unexpected Club List content for") + " " + clubid);
            error = true;
        }
    }
    else
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Network error. Cannot download Club List for") + " " + clubid);
        qCDebug(runtime) << "Network Error for club" << clubid << replyStatusCode << reply->error();
        error = true;
    }

    if ( error )
    {
        QList<QPair<QString, QString>> tmp;
        tmp << QPair<QString, QString>(clubid, "");
        removeClubsFromEnabledClubLists(tmp);
    }

    if ( updatePlan.size() > 0 ) updatePlan.removeFirst();

    reply->deleteLater();

    startPlannedDownload();
}

bool MembershipQE::importData(const QString &clubid, const QByteArray &data)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << clubid;

    QTextStream stream(data);

    Q_UNUSED(QSqlDatabase::database().transaction());

    QSqlQuery query;
    QSqlQuery versionQuery;

    qCDebug(runtime) << "Delete Records for a club" << clubid;

    if ( ! query.exec(QString("DELETE FROM membership WHERE clubid = '%1';").arg(clubid)))
    {
        qWarning() << "Cannot delete records for " << clubid << query.lastError().text();
        Q_UNUSED(QSqlDatabase::database().rollback());
        return false;
    }

    QCoreApplication::processEvents();

    qCDebug(runtime) << "Insert data into" << clubid;

    if ( ! query.prepare(QString("INSERT INTO membership(callsign,"
                                 "                       member_id,"
                                 "                       valid_from,"
                                 "                       valid_to,"
                                 "                       clubid"
                                 ")"
                                 " VALUES (:callsign,"
                                 "         :member_id,"
                                 "         :valid_from,"
                                 "         :valid_to,"
                                 "         '%1'"
                                 ")").arg(clubid)) )
    {
        qWarning() << "Cannot prepare Insert statement for membership table" << query.lastError().text();
        Q_UNUSED(QSqlDatabase::database().rollback());
        return false;
    }

    if ( ! versionQuery.prepare(QString("REPLACE INTO membership_versions(clubid, version) "
                                 " VALUES (:clubid, :version)") ) )
    {
        qWarning() << "Cannot prepare Insert statement for membership_version table" << versionQuery.lastError().text();
        Q_UNUSED(QSqlDatabase::database().rollback());
        return false;
    }

    QCoreApplication::processEvents();

    QString line = stream.readLine();
    QStringList fields = line.split(" ");
    int version;

    if ( fields.size() == 2
         && fields.at(0).at(0) == QChar('#')
         && (version = fields.at(1).toInt()) != 0)
    {
        versionQuery.bindValue(":clubid", clubid);
        versionQuery.bindValue(":version", version);

        if ( ! versionQuery.exec() )
        {
            qWarning() << "Membership version insert error " << versionQuery.lastError().text();
            Q_UNUSED(QSqlDatabase::database().rollback());
            return false;
        }

    }
    else
    {
        qCDebug(runtime) << "Unexpected header" << line;
        Q_UNUSED(QSqlDatabase::database().rollback());
        return false;
    }

    // skip CSV header
    stream.readLine();

    while ( !stream.atEnd() )
    {
        line = stream.readLine();
        fields = line.split(',');

        if ( fields.count() < 4 )
        {
            qInfo(runtime) << "Invalid line in the input file " << line << clubid;
            continue;
        }

        query.bindValue(":callsign", fields.at(0).toUpper().simplified());
        query.bindValue(":member_id", fields.at(1).simplified());
        query.bindValue(":valid_from", fields.at(2));
        query.bindValue(":valid_to", fields.at(3));

        if ( ! query.exec() )
        {
            qWarning() << "membership insert error " << query.lastError().text();
            continue;
        }

    }

    qCDebug(runtime) << "DONE";
    Q_UNUSED(QSqlDatabase::database().commit());

    return true;
}

ClubStatusQuery::ClubStatusQuery(QObject *parent) :
    QObject(parent),
    dbConnectionName("queryThread"),
    dbConnected(false)
{
    FCT_IDENTIFICATION;
}

ClubStatusQuery::~ClubStatusQuery()
{
    FCT_IDENTIFICATION;
}

void ClubStatusQuery::getClubStatus(const QString &in_callsign,
                                    const QString &in_band,
                                    const QString &in_mode,
                                    bool lowtConfirmed,
                                    bool paperConfirmed,
                                    bool eqslConfirmed)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << in_callsign << in_band << in_mode;
    qCDebug(runtime) << "Waiting for lock";

    QMutexLocker locker(&dbLock);

    qCDebug(runtime)  << "Processing getClubStatus";

    if ( !dbConnected )
    {
        qCDebug(runtime)  << "Opening connection to DB";
        QSqlDatabase db1 = QSqlDatabase::addDatabase("QSQLITE", dbConnectionName);
        db1.setDatabaseName(Data::dbFilename());
        dbConnected = db1.open();
        if ( ! dbConnected)
        {
            qWarning() << "Cannot open DB Connection for Update";
            emit status(in_callsign, QMap<QString, ClubInfo>());
            return;
        }
    }

    QSqlDatabase db1 = QSqlDatabase::database(dbConnectionName);
    QSqlQuery query(db1);
    const Callsign qCall(in_callsign);
    const QString &callModified = ( qCall.isValid() ) ? qCall.getBase() : in_callsign;

    QStringList dxccConfirmedByCond(QLatin1String("0=1")); // if no option is selected then always false

    if ( lowtConfirmed )
        dxccConfirmedByCond << QLatin1String("c.lotw_qsl_rcvd = 'Y'");

    if ( paperConfirmed )
        dxccConfirmedByCond << QLatin1String("c.qsl_rcvd = 'Y'");

    if ( eqslConfirmed )
        dxccConfirmedByCond << QLatin1String("c.eqsl_qsl_rcvd = 'Y'");

    if ( ! query.exec(QString("SELECT DISTINCT clubid, NULL band, NULL mode, "
                              "        NULL confirmed, NULL current_mode, member_id "
                              "FROM membership  WHERE callsign = '%1' "
                              "UNION ALL "
                              "SELECT DISTINCT clubid, c.band, o.dxcc mode, "
                              "                CASE WHEN (%2) THEN 1 ELSE 0 END confirmed, "
                              "               (SELECT modes.dxcc FROM modes WHERE modes.name = '%3' LIMIT 1) current_mode, "
                              "               NULL member_id "
                              "FROM contacts c, "
                              "    contact_clubs_view con2club, "
                              "    modes o "
                              "WHERE con2club.contactid = c.id "
                              "AND o.name = c.mode "
                              "AND con2club.clubid in (SELECT clubid FROM membership a WHERE a.callsign = '%4') order by 1, 3, 2, 4").arg(callModified,
                                                                                                                                          dxccConfirmedByCond.join(" OR "),
                                                                                                                                          in_mode,
                                                                                                                                          callModified)))
    {
       qCWarning(runtime) << "Cannot Get club status" << query.lastError().text();
       emit status(in_callsign, QMap<QString, ClubInfo>());
       return;
    }

    QMap<QString, ClubInfo> retMap;
    QString currentProcessedClub;
    QString currentMemberID;
    bool bandMatched = false;
    bool bandModeMatched = false;
    bool bandModeConfirmedMatched = false;
    bool modeMatched = false;
    unsigned long records = 0L;

    while ( ++records && query.next() )
    {
        const QString &clubid = query.value(0).toString();
        const QString &band = query.value(1).toString();
        const QString &mode = query.value(2).toString();
        const QVariant &confirmed = query.value(3);
        const QString &current_mode = query.value(4).toString();
        const QString &memberID = query.value(5).toString();

        qCDebug(runtime) << "Processing" << currentProcessedClub
                         << clubid
                         << band.isEmpty() << band
                         << mode.isEmpty() << mode
                         << confirmed.toString().isEmpty() << confirmed
                         << current_mode.isEmpty();

        // the select generates starting line for a new club
        // Changing the club
        if ( currentProcessedClub != clubid
             && band.isEmpty()
             && mode.isEmpty()
             && confirmed.toString().isEmpty()
             && current_mode.isEmpty()  )
        {
            if ( !currentProcessedClub.isEmpty() )
            {
                retMap[currentProcessedClub] = ClubInfo(determineClubStatus(bandMatched,
                                                                            bandModeMatched,
                                                                            bandModeConfirmedMatched,
                                                                            modeMatched,
                                                                            records),
                                                        currentMemberID);

            }
            currentProcessedClub = clubid;
            currentMemberID = memberID;
            bandMatched = bandModeMatched = bandModeConfirmedMatched = modeMatched = false;
            records = 0L;
            continue;
        }

        if ( currentProcessedClub == clubid )
        {
            if ( band == in_band )
            {
                bandMatched = true;
                if ( mode == current_mode )
                {
                    bandModeMatched = true;

                    if ( confirmed.toInt() == 1 )
                    {
                        bandModeConfirmedMatched = true;
                    }
                }
            }

            if ( mode == current_mode )
            {
                modeMatched = true;
            }
        }
        else
        {
            qCWarning(runtime) << "Unexpected branch" << currentProcessedClub << clubid;
        }
    }

    if ( !currentProcessedClub.isEmpty() )
    {
        qCDebug(runtime) << "Last Club processing" << currentProcessedClub;
        retMap[currentProcessedClub] = ClubInfo(determineClubStatus(bandMatched,
                                                                    bandModeMatched,
                                                                    bandModeConfirmedMatched,
                                                                    modeMatched, records),
                                                currentMemberID);
    }

    qCDebug(runtime) << "DONE";

    emit status(in_callsign, retMap);
    return;
}

ClubStatusQuery::ClubStatus ClubStatusQuery::determineClubStatus(bool bandMatched, bool bandModeMatched,
                                                      bool bandModeConfirmedMatched, bool modeMatched,
                                                      unsigned long records)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << bandMatched << bandModeMatched << bandModeConfirmedMatched << modeMatched << records;

    if ( !bandMatched && !bandModeMatched
         && !bandModeConfirmedMatched && !modeMatched && records == 1) // records == 1 means that there is only technical row in table
    {
        qCDebug(runtime) << "NewClub";
        return ClubStatus::NewClub;
    }

    if ( !bandMatched )
    {
        if ( !bandModeMatched )
        {
            qCDebug(runtime) << "NewBandModeClub";
            return ClubStatus::NewBandModeClub;
        }
        else
        {
            qCDebug(runtime) << "NewBandClub";
            return ClubStatus::NewBandClub;
        }
    }

    if ( !modeMatched )
    {
        qCDebug(runtime) << "NewModeClub";
        return ClubStatus::NewModeClub;
    }

    if ( ! bandModeMatched )
    {
        qCDebug(runtime) << "NewSlotClub";
        return ClubStatus::NewSlotClub;
    }

    if ( ! bandModeConfirmedMatched )
    {
        qCDebug(runtime) << "WorkedClub";
        return ClubStatus::WorkedClub;
    }
    else
    {
        qCDebug(runtime) << "ConfirmedClub";
        return ClubStatus::ConfirmedClub;
    }

    qCWarning(runtime) << "!!!!!!!! Unknown Status - never !!!!!!!!!!";
    return ClubStatus::UnknownStatusClub;
}
