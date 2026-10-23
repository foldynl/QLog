#include <QStandardPaths>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include "LOVDownloader.h"
#include "LOVDatabase.h"
#include "LogParam.h"
#include "service/clublog/ClubLog.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.lovdownloader");

LOVDownloader::LOVDownloader(QObject *parent) :
    QObject(parent),
    nam(new QNetworkAccessManager(this))
{
    FCT_IDENTIFICATION;
    connect(nam, &QNetworkAccessManager::finished, this, &LOVDownloader::processReply);
}

LOVDownloader::~LOVDownloader()
{
    FCT_IDENTIFICATION;
    stopUpdates();
}

const QList<LOVDownloader::SourceType> LOVDownloader::sources()
{
    FCT_IDENTIFICATION;
    return {
        CTY,
        CLUBLOGCTY,
        SATLIST,
        SOTASUMMITS,
        WWFFDIRECTORY,
        IOTALIST,
        POTADIRECTORY,
        MEMBERSHIPCONTENTLIST
    };
}

LOVDownloader::SourceDefinition LOVDownloader::sourceDefinition(SourceType type)
{
    FCT_IDENTIFICATION;
    // ClubLog's URL depends on a QString initialized in another translation unit.
    static const QMap<SourceType, SourceDefinition> sourceMapping =
    {
            {CTY, SourceDefinition(CTY,
                                   "https://www.country-files.com/bigcty/cty.csv",
                                   "cty.csv",
                                   "LOV/last_cty_update",
                                   "dxcc_entities_ad1c",
                                   7, "lov_cty", {"dxcc_prefixes_ad1c"})},
            {CLUBLOGCTY, SourceDefinition(CLUBLOGCTY,
                                   ClubLogBase::getCTYUrl(),
                                   "clublog_cty.xml",
                                   "LOV/last_clublogcty_update",
                                   "dxcc_entities_clublog",
                                   7, "lov_clublogcty", {"dxcc_prefixes_clublog"}, {"dxcc_zone_exceptions_clublog"})},
            {SATLIST, SourceDefinition(SATLIST,
                                       "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/satellites/satslist.csv",
                                       "satslist.csv",
                                       "LOV/last_sat_update",
                                       "sat_info",
                                       10, "lov_satlist")},
            {SOTASUMMITS, SourceDefinition(SOTASUMMITS,
                                       "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/SOTA/summitslist.csv",
                                       "summitslist.csv",
                                       "LOV/last_sotasummits_update",
                                       "sota_summits",
                                       30, "lov_sota")},
            {WWFFDIRECTORY, SourceDefinition(WWFFDIRECTORY,
                                       "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/WWFF/wwff_directory.csv",
                                       "wwff_directory.csv",
                                       "LOV/last_wwffdirectory_update",
                                       "wwff_directory",
                                       21, "lov_wwff")},
            {IOTALIST, SourceDefinition(IOTALIST,
                                       "https://www.iota-world.org/islands-on-the-air/downloads/download-file.html?path=groups.json",
                                       "groups.json",
                                       "LOV/last_iota_update",
                                       "iota",
                                       30, "lov_iota")},
            {POTADIRECTORY, SourceDefinition(POTADIRECTORY,
                                       "https://raw.githubusercontent.com/foldynl/hamradio-value-lists/main/lists/POTA/all_parks_ext.csv",
                                       "all_parks_ext.csv",
                                       "LOV/last_pota_update",
                                       "pota_directory",
                                       30, "lov_pota")},
            {MEMBERSHIPCONTENTLIST, SourceDefinition(MEMBERSHIPCONTENTLIST,
                                       "https://raw.githubusercontent.com/foldynl/hamradio-membeship-lists/main/lists/content.csv",
                                       "content.csv",
                                       "LOV/last_membershipcontent_update",
                                       "membership_directory",
                                       7, "lov_membership")}
        };

    return sourceMapping.value(type);
}

QString LOVDownloader::directory()
{
    FCT_IDENTIFICATION;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

bool LOVDownloader::isSourceAvailable(SourceType type)
{
    FCT_IDENTIFICATION;

    const SourceDefinition source = sourceDefinition(type);

    if ( source.type == UNDEF )
        return false;

    for ( const QString &table : source.nonEmptyTables )
    {
        QSqlQuery query;
        if ( !query.exec(QStringLiteral("SELECT EXISTS(SELECT 1 FROM %1)").arg(table)) || !query.next() )
        {
            qCWarning(runtime) << query.lastError();
            return false;
        }

        if ( !query.value(0).toBool() )
            return false;
    }
    return true;
}

void LOVDownloader::update(const SourceType &type, bool force)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << type << force;

    if ( stopping || currentReply || worker )
        return;

    canceled = false;
    const SourceDefinition source = sourceDefinition(type);

    if ( source.type == UNDEF )
    {
        complete(false);
        return;
    }

    const QDir dir(directory());
    const QDate lastDate = LogParam::getLOVaParam(source.lastTimeConfigName);
    const QDate today = QDate::currentDate();
    const bool available = isSourceAvailable(type);

    if ( QFile::exists(dir.filePath(source.packageStem + QStringLiteral(".ready.db")))
         || (!force && available && lastDate.isValid() && lastDate <= today && lastDate.daysTo(today) < source.ageTime) )
    {
        emit noUpdate();
        nextUpdate();
        return;
    }

    attemptedSources.insert(type);

    if ( !force && !available && dir.exists(source.fileName) )
        startWorker(source, QByteArray(), dir.filePath(source.fileName), lastDate);
    else
        download(source);
}

void LOVDownloader::download(const SourceDefinition &source)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << source.type;

    QNetworkRequest request(QUrl(source.URL));
    request.setRawHeader("User-Agent", QStringLiteral("QLog/%1").arg(VERSION).toUtf8());
    currentReply = nam->get(request);
    currentReply->setProperty("sourceType", source.type);
}

void LOVDownloader::startWorker(const SourceDefinition &source, const QByteArray &payload,
                                const QString &inputFile, const QDate &date, bool cachePayload)
{
    FCT_IDENTIFICATION;

    LOVDatabase::Job job;
    job.source = source;
    job.directory = directory();
    job.payload = payload;
    job.inputFile = inputFile;
    job.downloadDate = date;
    job.cachePayload = cachePayload;
    QSqlQuery schema;

    if ( !schema.prepare(QStringLiteral("SELECT sql FROM sqlite_master WHERE tbl_name=? AND type IN('table','index') AND sql IS NOT NULL ORDER BY type DESC")) )
    {
        qCWarning(runtime) << schema.lastError();
        complete(false);
        return;
    }

    for ( const QString &table : source.tableNames )
    {
        schema.bindValue(0, table);

        if ( !schema.exec() )
        {
            qCWarning(runtime) << schema.lastError();
            complete(false);
            return;
        }
        while ( schema.next() )
            job.schemaStatements << schema.value(0).toString();
    }
    worker = new LOVDatabase(job, this);
    connect(worker, &LOVDatabase::processingSize, this, &LOVDownloader::processingSize);
    connect(worker, &LOVDatabase::progress, this, &LOVDownloader::progress);
    connect(worker, &QThread::finished, this, [this, source, inputFile]()
    {
        const bool success = worker->succeeded();

        if ( !success && !canceled && !stopping )
            qCWarning(runtime) << source.type << worker->errorString();

        worker->deleteLater();
        worker = nullptr;

        if ( !success && source.type == CTY && inputFile != QStringLiteral(":/res/data/cty.csv") && !canceled && !stopping )
            recoverCTY(source, inputFile.isEmpty());
        else if ( !success && source.type != CTY && !inputFile.isEmpty() && !canceled && !stopping )
            download(source);
        else
            complete(success);
    });
    worker->start();
}

void LOVDownloader::recoverCTY(const SourceDefinition &source, bool tryCache)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << tryCache;

    if ( stopping )
        return;

    if ( isSourceAvailable(CTY) )
    {
        complete(false);
        return;
    }

    canceled = false;
    const QString cached = QDir(directory()).filePath(source.fileName);
    if ( tryCache && QFile::exists(cached) )
        startWorker(source, QByteArray(), cached, LogParam::getLOVaParam(source.lastTimeConfigName));
    else
    {
        qCWarning(runtime) << "Using bundled CTY data";
        startWorker(source, QByteArray(), QStringLiteral(":/res/data/cty.csv"), QDate());
    }
}

void LOVDownloader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    const SourceDefinition source = sourceDefinition(static_cast<SourceType>(reply->property("sourceType").toInt()));
    currentReply = nullptr;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool downloaded = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    const QByteArray payload = reply->readAll();
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const bool invalidCTY = source.type == CTY && (contentType.startsWith(QStringLiteral("text/html"), Qt::CaseInsensitive)
                            || payload.left(payload.indexOf('\n')).split(',').size() != 10);
    reply->deleteLater();

    if ( stopping )
        return;

    if ( downloaded && !invalidCTY && !canceled )
        startWorker(source, payload, QString(), QDateTime::currentDateTimeUtc().date(), true);
    else if ( source.type == CTY )
        recoverCTY(source);
    else
    {
        qCWarning(runtime) << "Reference-list download failed" << source.type << status;
        complete(false);
    }
}

void LOVDownloader::complete(bool success)
{
    FCT_IDENTIFICATION;

    if ( stopping )
        return;

    emit finished(success);
    nextUpdate();
}

void LOVDownloader::startUpdates()
{
    FCT_IDENTIFICATION;

    if ( stopping || currentReply || worker || !pendingSources.isEmpty() )
        return;

    for ( const SourceType type : sources() )
        if ( !attemptedSources.contains(type) )
            pendingSources << type;

    nextUpdate();
}

void LOVDownloader::nextUpdate()
{
    FCT_IDENTIFICATION;

    if ( !stopping && !pendingSources.isEmpty() )
    {
        const SourceType type = pendingSources.takeFirst();
        QTimer::singleShot(0, this, [this, type]() { update(type); });
    }
}

void LOVDownloader::abortRequest()
{
    FCT_IDENTIFICATION;

    canceled = true;

    if ( worker )
        worker->requestInterruption();
    if ( currentReply )
        currentReply->abort();
}

void LOVDownloader::stopUpdates()
{
    FCT_IDENTIFICATION;

    stopping = true;
    pendingSources.clear();
    abortRequest();
}
