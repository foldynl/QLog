#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QScopeGuard>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QUuid>
#include "LOVDatabase.h"
#include "LogParam.h"
#include "FileCompressor.h"
#include "debug.h"
#include "3rdparty/vincentlaucsb/csv-parser/csv.hpp"

MODULE_IDENTIFICATION("qlog.core.lovdatabase");

LOVDatabase::LOVDatabase(const Job &job, QObject *parent) :
    QThread(parent),
    job(job)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << job.source.type;
}

LOVDatabase::~LOVDatabase()
{
    FCT_IDENTIFICATION;

    requestInterruption();
    wait();
}

bool LOVDatabase::execSql(QSqlDatabase &database, const QString &sql)
{
    FCT_IDENTIFICATION;

    QSqlQuery query(database);

    if ( query.exec(sql) )
        return true;

    qCWarning(runtime) << query.lastError() << sql;
    return false;
}

void LOVDatabase::run()
{
    FCT_IDENTIFICATION;

    const QDir directory(job.directory);
    const QString pending = directory.filePath(job.source.packageStem + QStringLiteral(".pending.db"));
    const QString ready = directory.filePath(job.source.packageStem + QStringLiteral(".ready.db"));

    if ( job.source.type == LOVDownloader::UNDEF || QFile::exists(ready) )
        return;

    if ( !removePackage(pending) )
        return;

    QByteArray data = job.payload;

    if ( !job.inputFile.isEmpty() )
    {
        QFile file(job.inputFile);
        if ( !file.open(QIODevice::ReadOnly) )
        {
            qCWarning(runtime) << "Cannot open reference-list input" << file.fileName() << file.errorString();
            return;
        }

        data = file.readAll();
        if ( file.error() != QFile::NoError )
        {
            qCWarning(runtime) << "Cannot read reference-list input" << file.fileName() << file.errorString();
            return;
        }
    }
    if ( data.startsWith(QByteArray::fromHex("1f8b")) || job.source.fileName.endsWith(QStringLiteral(".gz")) )
        data = FileCompressor::gunzip(data);

    if ( data.isEmpty() || isInterruptionRequested() )
        return;

    const QString connectionName = QStringLiteral("lov_prepare_") + QUuid::createUuid().toString();
    bool prepared = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(pending);

        if ( database.open() )
            prepared = buildPackage(database, data);
        else
            qCWarning(runtime) << database.lastError();

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    // Publish only a self-contained, committed database after all SQL handles are gone.
    if ( !prepared || isInterruptionRequested() )
        return;

    QFile package(pending);
    if ( !package.rename(ready) )
    {
        qCWarning(runtime) << "Cannot publish reference-list package" << pending << ready << package.errorString();
        return;
    }

    success = true;

    if ( job.cachePayload && !isInterruptionRequested() )
    {
        QSaveFile cache(directory.filePath(job.source.fileName));
        if ( !cache.open(QIODevice::WriteOnly) || cache.write(data) != data.size() || !cache.commit() )
            qCWarning(runtime) << "Cannot save reference-list cache" << cache.fileName() << cache.errorString();
    }
}

bool LOVDatabase::buildPackage(QSqlDatabase &database, const QByteArray &data)
{
    FCT_IDENTIFICATION;

    if ( !execSql(database, QStringLiteral("PRAGMA journal_mode=DELETE")) || !database.transaction() )
    {
        qCWarning(runtime) << database.lastError();
        return false;
    }

    auto rollback = qScopeGuard([&database]()
    {
        if ( !database.rollback() )
            qCWarning(runtime) << "Cannot roll back reference-list preparation" << database.lastError();
    });

    for ( const QString &sql : job.schemaStatements )
        if ( !execSql(database, sql) )
            return false;

    emit processingSize(data.size());

    QTextStream stream(data);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif

    if ( !parseData(database, stream) || isInterruptionRequested() )
        return false;

    for ( const QString &table : job.source.nonEmptyTables )
    {
        QSqlQuery count(database);
        if ( !count.exec(QStringLiteral("SELECT EXISTS(SELECT 1 FROM %1)").arg(table)) || !count.next() || !count.value(0).toBool() )
        {
            qCWarning(runtime) << "Missing reference-list records" << table << count.lastError();
            return false;
        }
    }

    if ( !execSql(database, QStringLiteral("CREATE TABLE lov_metadata(format_version INTEGER, source_type INTEGER, download_date TEXT)")) )
        return false;

    QSqlQuery metadata(database);
    if ( !metadata.prepare(QStringLiteral("INSERT INTO lov_metadata VALUES(1,?,?)")) )
    {
        qCWarning(runtime) << metadata.lastError();
        return false;
    }

    metadata.addBindValue(job.source.type);
    metadata.addBindValue(job.downloadDate.isValid() ? QVariant(job.downloadDate.toString(Qt::ISODate)) : QVariant());

    if ( !metadata.exec() || !database.commit() )
    {
        qCWarning(runtime) << metadata.lastError() << database.lastError();
        return false;
    }

    rollback.dismiss();
    return true;
}

QStringList LOVDatabase::tableColumns(QSqlDatabase &database, const QString &schema, const QString &table)
{
    FCT_IDENTIFICATION;

    QSqlQuery query(database);
    QStringList columns;

    if ( !query.exec(QStringLiteral("PRAGMA %1.table_info(%2)").arg(schema, table)) )
    {
        qCWarning(runtime) << query.lastError();
        return columns;
    }

    while ( query.next() )
        columns << query.value(1).toString();

    if ( query.lastError().isValid() )
    {
        qCWarning(runtime) << query.lastError();
        return QStringList();
    }
    return columns;
}

bool LOVDatabase::validatePackage(QSqlDatabase &database, const LOVDownloader::SourceDefinition &source,
                                 QDate &downloadDate, QStringList &columnLists)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << source.type;
    QSqlQuery metadata(database);

    if ( !metadata.exec(QStringLiteral("SELECT format_version,source_type,download_date FROM lov_update.lov_metadata"))
         || !metadata.next() )
    {
        qCWarning(runtime) << "Cannot read reference-list metadata" << metadata.lastError();
        return false;
    }

    if ( metadata.value(0).toInt() != 1 || metadata.value(1).toInt() != source.type )
    {
        qCWarning(runtime) << "Incompatible reference-list metadata" << source.type;
        return false;
    }

    downloadDate = QDate::fromString(metadata.value(2).toString(), Qt::ISODate);

    if ( !metadata.isNull(2) && !downloadDate.isValid() )
    {
        qCWarning(runtime) << "Invalid reference-list download date" << source.type;
        return false;
    }

    if ( metadata.next() || metadata.lastError().isValid() )
    {
        qCWarning(runtime) << "Invalid reference-list metadata rows" << metadata.lastError();
        return false;
    }

    metadata.finish();

    for ( const QString &table : source.tableNames )
    {
        const QStringList columns = tableColumns(database, QStringLiteral("main"), table);
        if ( columns.isEmpty() || columns != tableColumns(database, QStringLiteral("lov_update"), table) )
        {
            qCWarning(runtime) << "Incompatible reference-list columns" << table;
            return false;
        }

        QStringList escaped;
        for ( const QString &column : columns )
            escaped << database.driver()->escapeIdentifier(column, QSqlDriver::FieldName);

        columnLists << escaped.join(QLatin1Char(','));
    }

    for ( const QString &table : source.nonEmptyTables )
    {
        QSqlQuery count(database);
        if ( !count.exec(QStringLiteral("SELECT EXISTS(SELECT 1 FROM lov_update.%1)").arg(table))
             || !count.next() || !count.value(0).toBool() )
        {
            qCWarning(runtime) << "Missing reference-list records" << table << count.lastError();
            return false;
        }
    }
    return true;
}

bool LOVDatabase::removePackage(const QString &filename)
{
    FCT_IDENTIFICATION;

    for ( const QString &suffix : {QString(), QStringLiteral("-journal"), QStringLiteral("-wal"), QStringLiteral("-shm")} )
    {
        QFile file(filename + suffix);
        if ( file.exists() && !file.remove() )
        {
            qCWarning(runtime) << "Cannot remove reference-list file" << file.fileName() << file.errorString();
            return false;
        }
    }
    return true;
}

bool LOVDatabase::discardPrepared(const QString &directory)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << directory;

    const QDir target(directory);
    for ( const LOVDownloader::SourceType type : LOVDownloader::sources() )
    {
        const QString basename = target.filePath(LOVDownloader::sourceDefinition(type).packageStem);
        if ( !removePackage(basename + QStringLiteral(".pending.db"))
             || !removePackage(basename + QStringLiteral(".ready.db")) )
            return false;
    }
    return true;
}

LOVDatabase::ApplyResult LOVDatabase::applyReady(const LOVDownloader::SourceDefinition &source,
                                                const QString &directory, QString *error)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << source.type;

    if ( error )
        error->clear();

    const QString filename = QDir(directory).filePath(source.packageStem + QStringLiteral(".ready.db"));
    if ( !QFile::exists(filename) )
        return ApplyResult::NO_PACKAGE;

    QElapsedTimer timer;
    timer.start();
    const auto logDuration = qScopeGuard([&]()
    {
        qCDebug(runtime) << "LOV adoption blocked main thread:" << source.packageStem
                         << timer.elapsed() << "ms";
    });

    QSqlDatabase database = QSqlDatabase::database();
    QSqlQuery attach(database);
    if ( !attach.prepare(QStringLiteral("ATTACH DATABASE ? AS lov_update")) )
    {
        qCWarning(runtime) << attach.lastError();
        return ApplyResult::RETRY;
    }

    attach.addBindValue(filename);

    if ( !attach.exec() )
    {
        qCWarning(runtime) << attach.lastError();
        const QString code = attach.lastError().nativeErrorCode();
        return code == QStringLiteral("11") || code == QStringLiteral("26") ? ApplyResult::INVALID : ApplyResult::RETRY;
    }

    attach.finish();

    ApplyResult result = ApplyResult::INVALID;
    QDate downloadDate;
    QStringList columnLists;
    if ( validatePackage(database, source, downloadDate, columnLists) )
    {
        const QDate lastDate = LogParam::getLOVaParam(source.lastTimeConfigName);
        if ( downloadDate.isValid() && lastDate <= QDate::currentDate() && lastDate > downloadDate )
            result = ApplyResult::NO_PACKAGE;
        else
        {
            result = ApplyResult::RETRY;
            if ( database.transaction() )
            {
                bool copied = true;
                for ( int index = source.tableNames.size() - 1; copied && index >= 0; --index )
                    copied = execSql(database, QStringLiteral("DELETE FROM main.%1").arg(source.tableNames.at(index)));
                for ( int index = 0; copied && index < source.tableNames.size(); ++index )
                    copied = execSql(database, QStringLiteral("INSERT INTO main.%1(%2) SELECT %2 FROM lov_update.%1")
                                             .arg(source.tableNames.at(index), columnLists.at(index)));
                // No event pumping/readback here: LogParam invalidates its cache before commit.
                copied = copied && (!downloadDate.isValid() || LogParam::setLOVParam(source.lastTimeConfigName, downloadDate));
                if ( copied && database.commit() )
                    result = ApplyResult::APPLIED;
                else
                {
                    qCWarning(runtime) << "Cannot adopt reference list" << source.type << database.lastError();
                    if ( !database.rollback() )
                        qCWarning(runtime) << "Cannot roll back reference list" << database.lastError();
                }
            }
            else
                qCWarning(runtime) << database.lastError();
        }
    }

    if ( !execSql(database, QStringLiteral("DETACH DATABASE lov_update")) )
    {
        if ( error )
            *error = tr("Could not close the reference-list database.");
        return ApplyResult::DATABASE_ERROR;
    }

    if ( result == ApplyResult::APPLIED || result == ApplyResult::NO_PACKAGE )
        removePackage(filename);
    else if ( error )
        *error = tr("Could not update the reference list. Existing data have been kept.");
    return result;
}

bool LOVDatabase::parseData(QSqlDatabase &database, QTextStream &data)
{
    FCT_IDENTIFICATION;

    csv::CSVFormat format;

    switch ( job.source.type )
    {
    case LOVDownloader::CTY:
        return parseCTY(database, data);
    case LOVDownloader::CLUBLOGCTY:
        return parseClubLogCTY(database, data);
    case LOVDownloader::SATLIST:
        return parseDelimited(database, data, QLatin1Char(';'));
    case LOVDownloader::MEMBERSHIPCONTENTLIST:
        return parseDelimited(database, data, QLatin1Char(','));
    case LOVDownloader::IOTALIST:
        return parseIOTA(database, data);
    case LOVDownloader::SOTASUMMITS:
        format.delimiter(',').quote('"').header_row(1);
        return parseCSVGeneric(database, data,
            "INSERT INTO sota_summits(summit_code,association_name,region_name,summit_name,altm,altft,gridref1,gridref2,longitude,latitude,points,bonus_points,valid_from,valid_to) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            {"SummitCode","AssociationName","RegionName","SummitName","AltM","AltFt","GridRef1","GridRef2","Longitude","Latitude","Points","BonusPoints","ValidFrom","ValidTo"}, format, "SOTA Summits List");
    case LOVDownloader::WWFFDIRECTORY:
        format.delimiter(',').quote('"').trim({' '});
        return parseCSVGeneric(database, data,
            "INSERT INTO wwff_directory(reference,status,name,program,dxcc,state,county,continent,iota,iaruLocator,latitude,longitude,iucncat,valid_from,valid_to) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            {"reference","status","name","program","dxcc","state","county","continent","iota","iaruLocator","latitude","longitude","IUCNcat","validFrom","validTo"}, format);
    case LOVDownloader::POTADIRECTORY:
        format.delimiter(',').quote('"');
        return parseCSVGeneric(database, data,
            "INSERT INTO pota_directory(reference,name,active,entityID,locationDesc,latitude,longitude,grid) VALUES(?,?,?,?,?,?,?,?)",
            {"reference","name","active","entityId","locationDesc","latitude","longitude","grid"}, format);
    default:
        return false;
    }
}

bool LOVDatabase::parseDelimited(QSqlDatabase &database, QTextStream &data, QChar delimiter)
{
    FCT_IDENTIFICATION;

    const bool satellite = job.source.type == LOVDownloader::SATLIST;
    const int columns = satellite ? 8 : 5;
    QSqlQuery insert(database);

    if ( !insert.prepare(satellite
            ? QStringLiteral("INSERT INTO sat_info(name,number,uplink,downlink,beacon,mode,callsign,status) VALUES(?,?,?,?,?,?,?,?)")
            : QStringLiteral("INSERT INTO membership_directory(short_desc,long_desc,filename,last_update,num_records) VALUES(?,?,?,?,?)")) )
    {
        qCWarning(runtime) << insert.lastError();
        return false;
    }

    int count = 0;
    while ( !data.atEnd() && !isInterruptionRequested() )
    {
        const QString line = data.readLine();
        if ( line.trimmed().isEmpty() )
            continue;

        const QStringList fields = line.split(delimiter);
        if ( fields.size() != columns || fields.first().isEmpty() )
            return false;

        for ( int column = 0; column < columns; ++column )
            insert.bindValue(column, fields.at(column));

        if ( !insert.exec() )
        {
            qCWarning(runtime) << insert.lastError();
            return false;
        }

        if ( ++count % 500 == 0 )
            emit progress(data.pos());
    }
    return count > 0 && !isInterruptionRequested();
}

bool LOVDatabase::parseIOTA(QSqlDatabase &database, QTextStream &data)
{
    FCT_IDENTIFICATION;
    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(data.readAll().toUtf8(), &jsonError);
    if ( jsonError.error != QJsonParseError::NoError || !document.isArray() )
        return false;

    QSqlQuery insert(database);
    if ( !insert.prepare(QStringLiteral("INSERT INTO iota(iotaid,islandname) VALUES(?,?)")) )
    {
        qCWarning(runtime) << insert.lastError();
        return false;
    }

    int count = 0;
    for ( const QJsonValue &value : document.array() )
    {
        if ( isInterruptionRequested() )
            return false;

        const QJsonObject row = value.toObject();
        if ( !value.isObject() || !row.value("refno").isString() || row.value("refno").toString().isEmpty() || !row.value("name").isString() )
            return false;

        insert.bindValue(0, row.value("refno").toString());
        insert.bindValue(1, row.value("name").toString());
        if ( !insert.exec() )
        {
            qCWarning(runtime) << insert.lastError();
            return false;
        }

        if ( ++count % 500 == 0 )
            emit progress(data.pos());
    }
    return count > 0;
}

bool LOVDatabase::isCSVComplete(const QByteArray &text, const csv::CSVFormat &format) const
{
    FCT_IDENTIFICATION;

    const char delimiter = format.get_delim();
    const char quote = format.get_quote_char();
    QByteArray trim;

    for ( const char character : format.get_trim_chars() )
        trim.append(character);

    bool quoted = false;
    bool fieldStart = true;
    const int INTERRUPT_CHECK_INTERVAL = 4096;
    const int start = text.startsWith("\xef\xbb\xbf") ? 3 : 0;

    for ( qint64 index = start; index < text.size(); ++index )
    {
        if ( index % INTERRUPT_CHECK_INTERVAL == 0 && isInterruptionRequested() )
            return false;

        const char character = text.at(index);
        if ( quoted )
        {
            if ( character != quote )
                continue;
            if ( index + 1 < text.size() && text.at(index + 1) == quote )
                ++index;
            else if ( index + 1 == text.size() || text.at(index + 1) == delimiter
                      || text.at(index + 1) == '\r' || text.at(index + 1) == '\n' )
                quoted = false;
        }
        else if ( character == delimiter || character == '\r' || character == '\n' )
            fieldStart = true;
        else if ( fieldStart && !trim.contains(character) )
        {
            quoted = character == quote;
            fieldStart = false;
        }
    }
    return !quoted;
}

bool LOVDatabase::parseCSVGeneric(QSqlDatabase &database, QTextStream &data,
                                  const QString &insertSQL, const QStringList &columns,
                                  csv::CSVFormat format, const QString &header) try
{
    FCT_IDENTIFICATION;

    const QByteArray text = data.readAll().toUtf8();
    if ( !header.isEmpty() && !text.contains(header.toUtf8()) )
        return false;

    // The CSV parser checks row width but accepts EOF inside a quoted field.
    if ( !isCSVComplete(text, format) )
    {
        if ( !isInterruptionRequested() )
            qCWarning(runtime) << "Unterminated reference-list CSV field" << job.source.type;
        return false;
    }

    format.variable_columns(csv::VariableColumnPolicy::THROW);
    csv::CSVReader reader = csv::parse_unsafe(csv::string_view(text.constData(), size_t(text.size())), format);
    const std::vector<std::string> names = reader.get_col_names();
    std::vector<std::string> selected;

    for ( const QString &column : columns )
    {
        const std::string name = column.toStdString();
        if ( std::find(names.begin(), names.end(), name) == names.end() )
            return false;

        selected.push_back(name);
    }
    QSqlQuery insert(database);
    if ( !insert.prepare(insertSQL) )
    {
        qCWarning(runtime) << insert.lastError();
        return false;
    }

    const int CHUNK = 5000;
    const int rows = qMax(1, static_cast<int>(text.count('\n')));
    QVector<QVariantList> values(columns.size());

    for ( QVariantList &column : values )
        column.reserve(CHUNK);

    auto flush = [&]()
    {
        for ( int column = 0; column < values.size(); ++column )
            insert.bindValue(column, values.at(column));

        if ( !insert.execBatch() )
        {
            qCWarning(runtime) << insert.lastError();
            return false;
        }

        for ( QVariantList &column : values )
            column.clear();
        return true;
    };

    int count = 0;
    for ( csv::CSVRow &row : reader )
    {
        if ( isInterruptionRequested() || row[selected.front()].get<std::string>().empty() )
            return false;

        for ( int column = 0; column < values.size(); ++column )
            values[column] << QString::fromStdString(row[selected[column]].get<std::string>());

        if ( ++count % CHUNK == 0 )
        {
            if ( !flush() )
                return false;

            emit progress(static_cast<qint64>(count) * text.size() / rows);
        }
    }
    return count > 0 && !isInterruptionRequested() && (count % CHUNK == 0 || flush());
}
catch ( const std::exception &exception )
{
    qCWarning(runtime) << "Invalid reference-list CSV" << exception.what();
    return false;
}

bool LOVDatabase::parseCTY(QSqlDatabase &database, QTextStream &data)
{
    FCT_IDENTIFICATION;

    const QRegularExpression separator("[\\s;]");
    const QRegularExpression prefixFormat("^(=?)([A-Z0-9/]+)(?:\\((\\d+)\\))?(?:\\[(\\d+)\\])?$");

    QSqlQuery entity(database), prefix(database);
    if ( !entity.prepare("INSERT INTO dxcc_entities_ad1c(id,name,prefix,cont,cqz,ituz,lat,lon,tz) "
                         "VALUES(:id,:name,:prefix,:cont,:cqz,:ituz,:lat,:lon,:tz)") )
    {
        qCWarning(runtime) << entity.lastError();
        return false;
    }

    if ( !prefix.prepare("INSERT INTO dxcc_prefixes_ad1c(prefix,exact,dxcc,cqz,ituz) "
                         "VALUES(:prefix,:exact,:dxcc,:cqz,:ituz)") )
    {
        qCWarning(runtime) << prefix.lastError();
        return false;
    }

    int count = 0;
    while ( !data.atEnd() && !isInterruptionRequested() )
    {
        const QString line = data.readLine();
        if ( line.trimmed().isEmpty() )
            continue;

        const QStringList fields = line.split(',');
        if ( fields.size() != 10 )
            return false;

        if ( fields.at(0).startsWith('*') )
            continue;

        bool validId = false;
        const int dxccId = fields.at(2).toInt(&validId);
        if ( !validId || dxccId <= 0 || fields.at(0).isEmpty() || fields.at(1).isEmpty() )
            return false;

        entity.bindValue(":id", dxccId);
        entity.bindValue(":prefix", fields.at(0));
        entity.bindValue(":name", fields.at(1));
        entity.bindValue(":cont", fields.at(3));
        entity.bindValue(":cqz", fields.at(4));
        entity.bindValue(":ituz", fields.at(5));
        entity.bindValue(":lat", fields.at(6).toFloat());
        entity.bindValue(":lon", -fields.at(7).toFloat());
        entity.bindValue(":tz", fields.at(8).toFloat());

        if ( !entity.exec() )
        {
            qCWarning(runtime) << entity.lastError();
            return false;
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        const QStringList prefixes = fields.at(9).split(separator, Qt::SkipEmptyParts);
#else
        const QStringList prefixes = fields.at(9).split(separator, QString::SkipEmptyParts);
#endif
        QStringList seen;
        for ( const QString &token : prefixes )
        {
            if ( isInterruptionRequested() )
                return false;

            const QRegularExpressionMatch match = prefixFormat.match(token);

            if ( !match.hasMatch() )
                return false;

            const QString value = match.captured(2);
            // CTY may repeat a prefix within the same entity.
            if ( seen.contains(value) )
                continue;

            seen << value;
            prefix.bindValue(":prefix", value);
            prefix.bindValue(":exact", !match.captured(1).isEmpty());
            prefix.bindValue(":dxcc", dxccId);
            prefix.bindValue(":cqz", match.captured(3).toInt());
            prefix.bindValue(":ituz", match.captured(4).toInt());

            if ( !prefix.exec() )
            {
                qCWarning(runtime) << prefix.lastError();
                return false;
            }
        }

        if ( ++count % 20 == 0 )
            emit progress(data.pos());
    }
    return count > 0 && !isInterruptionRequested();
}

bool LOVDatabase::parseClubLogCTY(QSqlDatabase &database, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QFile staticFile(QStringLiteral(":/res/data/dxcc.json"));
    if ( !staticFile.open(QIODevice::ReadOnly) )
        return false;

    QMap<int, QVariantMap> staticInfo;
    const QJsonDocument staticDocument = QJsonDocument::fromJson(staticFile.readAll());
    if ( !staticDocument.isArray() )
        return false;

    for ( const QJsonValue &value : staticDocument.array() )
    {
        const QVariantMap row = value.toObject().toVariantMap();
        staticInfo.insert(row.value("id").toInt(), row);
    }

    QXmlStreamReader xml(data.readAll());
    QSqlQuery insEntity(database), insPrefix(database), insZone(database);

    if ( !insEntity.prepare(
                "INSERT INTO dxcc_entities_clublog(id, name, prefix, deleted, cqz, ituz, cont, lon, lat, start, \"end\")"
                "VALUES(:id, :name, :prefix, :deleted, :cqz, :ituz, :cont, :lon, :lat, :start, :end)") )
    {
        qCWarning(runtime) << insEntity.lastError();
        return false;
    }

    if ( !insPrefix.prepare(
                "INSERT INTO dxcc_prefixes_clublog(prefix, exact, dxcc, cqz, ituz, cont, lon, lat, start, \"end\")"
                "VALUES(:prefix, :exact, :dxcc, :cqz, :ituz, :cont, :lon, :lat, :start, :end)") )
    {
        qCWarning(runtime) << insPrefix.lastError();
        return false;
    }

    if ( !insZone.prepare(
                "INSERT INTO dxcc_zone_exceptions_clublog(record, call, cqz, start, \"end\")"
                "VALUES(:record, :call, :cqz, :start, :end)") )
    {
        qCWarning(runtime) << insZone.lastError();
        return false;
    }

    auto readText = [](QXmlStreamReader &x) { return x.readElementText().trimmed(); };

    while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isStartElement() && xml.name() == QStringLiteral("clublog")) )
        xml.readNext();

    if ( xml.atEnd() )
    {
        qCWarning(runtime) << "ClubLog: <clublog> not found";
        return false;
    }

    quint32 readOp = 0;

    auto updateReadProgress = [&]()
    {
        readOp++;
        if ( readOp % 200 == 0 )
            emit progress(xml.characterOffset());
    };

    while ( !xml.atEnd() && !isInterruptionRequested() )
    {
        xml.readNext();

        if ( !xml.isStartElement() )
            continue;

        const QString top = xml.name().toString();

        if ( top == "entities" )
        {
            while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isEndElement() && xml.name() == QStringLiteral("entities")) )
            {
                xml.readNext();

                updateReadProgress();

                if ( xml.isStartElement() && xml.name() == QStringLiteral("entity") )
                {
                    int adif = 0;
                    QString name, prefix, cont;
                    bool deleted = false;
                    int cqz = 0;
                    QString start, end;
                    double lon = 0.0, lat = 0.0;

                    while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isEndElement() && xml.name() == QStringLiteral("entity")) )
                    {
                        xml.readNext();

                        updateReadProgress();

                        if ( !xml.isStartElement() )
                            continue;

                        const QString tag = xml.name().toString();

                        if ( tag == "adif" )
                            adif = readText(xml).toInt();
                        else if ( tag == "name" )
                            name = readText(xml);
                        else if ( tag == "prefix" )
                            prefix = readText(xml);
                        else if ( tag == "deleted" )
                            deleted = (readText(xml).compare("true", Qt::CaseInsensitive) == 0);
                        else if ( tag == "cqz" )
                            cqz = readText(xml).toInt();
                        else if ( tag == "cont" )
                            cont = readText(xml);
                        else if ( tag == "long" )
                            lon = readText(xml).toDouble();
                        else if ( tag == "lat" )
                            lat = readText(xml).toDouble();
                        else if ( tag == "start" )
                            start = readText(xml);
                        else if ( tag == "end" )
                            end = readText(xml);
                        else
                            xml.skipCurrentElement();
                    }

                    if ( adif <= 0 || name.isEmpty() )
                        return false;
                    insEntity.bindValue(":id", adif);

                    const QString nameModified = staticInfo.value(adif).value("name").toString();

                    insEntity.bindValue(":name", nameModified.isEmpty() ? name : nameModified);
                    insEntity.bindValue(":prefix", prefix);
                    insEntity.bindValue(":deleted", deleted ? 1 : 0);
                    insEntity.bindValue(":cqz", cqz);
                    insEntity.bindValue(":ituz", staticInfo.value(adif).value("ituz").toInt());
                    insEntity.bindValue(":cont", cont.isEmpty() ? QVariant() : cont);
                    insEntity.bindValue(":lon", lon);
                    insEntity.bindValue(":lat", lat);
                    insEntity.bindValue(":start", start.isEmpty() ? QVariant() : start);
                    insEntity.bindValue(":end", end.isEmpty() ? QVariant() : end);

                    if ( !insEntity.exec() )
                    {
                        qCWarning(runtime) << insEntity.lastError();
                        return false;
                    }
                }
            }
        }
        else if ( top == "prefixes" || top == "exceptions" )
        {
            // Exceptions share the prefix fields, but match an exact call.
            const bool isPrefix = top == "prefixes";

            while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isEndElement() && xml.name() == top) )
            {
                xml.readNext();

                updateReadProgress();

                if ( xml.isStartElement() && (xml.name() == QStringLiteral("prefix")
                                              || xml.name() == QStringLiteral("exception")) )
                {
                    QString call, cont, start, end;
                    int adif = 0, cqz = 0;
                    double lon = 0.0, lat = 0.0;
                    bool validAdif = false;

                    while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isEndElement() && (xml.name() == QStringLiteral("prefix")
                                                     || xml.name() == QStringLiteral("exception"))) )
                    {
                        xml.readNext();

                        updateReadProgress();

                        if ( !xml.isStartElement() )
                            continue;

                        const QString tag = xml.name().toString();

                        if ( tag == "call" )
                            call = readText(xml);
                        else if ( tag == "adif" )
                            adif = readText(xml).toInt(&validAdif);
                        else if ( tag == "cqz" )
                            cqz = readText(xml).toInt();
                        else if ( tag == "cont" )
                            cont = readText(xml);
                        else if ( tag == "long" )
                            lon = readText(xml).toDouble();
                        else if ( tag == "lat" )
                            lat = readText(xml).toDouble();
                        else if ( tag == "start" )
                            start = readText(xml);
                        else if ( tag == "end" )
                            end = readText(xml);
                        else
                            xml.skipCurrentElement();
                    }

                    if ( !validAdif || adif < 0 || (isPrefix && adif == 0) || call.isEmpty() )
                        return false;
                    insPrefix.bindValue(":prefix", call);
                    insPrefix.bindValue(":exact", !isPrefix);
                    insPrefix.bindValue(":dxcc", adif);
                    insPrefix.bindValue(":cqz", cqz);
                    insPrefix.bindValue(":ituz", 0);
                    insPrefix.bindValue(":cont", cont.isEmpty() ? QVariant() : cont);
                    insPrefix.bindValue(":lon", lon);
                    insPrefix.bindValue(":lat", lat);
                    insPrefix.bindValue(":start", start.isEmpty() ? QVariant() : start);
                    insPrefix.bindValue(":end", end.isEmpty() ? QVariant() : end);

                    if ( !insPrefix.exec() )
                    {
                        qCWarning(runtime) << insPrefix.lastError();
                        return false;
                    }
                }
            }
        }
        else if ( top == "zone_exceptions" )
        {
            while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isEndElement() && xml.name() == QStringLiteral("zone_exceptions")) )
            {
                xml.readNext();

                updateReadProgress();

                if ( xml.isStartElement() && xml.name() == QStringLiteral("zone_exception") )
                {
                    bool ok = false;
                    const quint32 rec = xml.attributes().value("record").toUInt(&ok);
                    QString call, start, end;
                    int zone = 0;

                    while ( !xml.atEnd() && !isInterruptionRequested() && !(xml.isEndElement() && xml.name() == QStringLiteral("zone_exception")) )
                    {
                        xml.readNext();

                        updateReadProgress();

                        if ( !xml.isStartElement() )
                            continue;

                        const QString tag = xml.name().toString();
                        if ( tag == "call" )
                            call = readText(xml);
                        else if ( tag == "zone" )
                            zone = readText(xml).toInt();
                        else if ( tag == "start" )
                            start = readText(xml);
                        else if ( tag == "end" )
                            end = readText(xml);
                        else
                            xml.skipCurrentElement();
                    }
                    if ( !ok || call.isEmpty() || zone <= 0 )
                        return false;
                    insZone.bindValue(":record", rec);
                    insZone.bindValue(":call", call);
                    insZone.bindValue(":cqz", zone);
                    insZone.bindValue(":start", start);
                    insZone.bindValue(":end", end);
                    if ( !insZone.exec() )
                    {
                        qCWarning(runtime) << insZone.lastError();
                        return false;
                    }
                }
            }
        }
        else
            xml.skipCurrentElement();

        updateReadProgress();
    }

    if ( xml.hasError() )
    {
        qCWarning(runtime) << "ClubLog XML error:" << xml.errorString();
        return false;
    }

    return !isInterruptionRequested();
}
