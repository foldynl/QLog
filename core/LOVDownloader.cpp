#include <QStandardPaths>
#include <QDir>
#include <QSqlQuery>
#include <QTimer>
#include <QNetworkReply>
#include <QSqlError>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QApplication>
#include <QRegularExpression>
#include "LogParam.h"

#include "LOVDownloader.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.lovdownloader");

LOVDownloader::LOVDownloader(QObject *parent) :
    QObject(parent),
    currentReply(nullptr),
    abortRequested(false),
    /* https://stackoverflow.com/questions/18144431/regex-to-split-a-csv */
    CSVRe("(?:^|,)(?=[^\"]|(\")?)\"?((?(1)(?:[^\"]|\"\")*|[^,\"]*))\"?(?=,|$)"),
    CTYPrefixSeperatorRe("[\\s;]"),
    CTYPrefixFormatRe("(=?)([A-Z0-9/]+)(?:\\((\\d+)\\))?(?:\\[(\\d+)\\])?$")
{
    FCT_IDENTIFICATION;

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished,
            this, &LOVDownloader::processReply);
}

LOVDownloader::~LOVDownloader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }

    nam->deleteLater();
}

void LOVDownloader::update(const SourceType & sourceType)
{
    FCT_IDENTIFICATION;

    abortRequested = false;

    const SourceDefinition &sourceDef = sourceMapping[sourceType];

    Q_ASSERT(sourceDef.type == sourceType);

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QDate last_update = LogParam::getParam(sourceDef.lastTimeConfigName).toDate();

    if ( dir.exists(sourceDef.fileName)
         && last_update.isValid()
         && last_update.daysTo(QDate::currentDate()) < sourceDef.ageTime )
    {
        if ( isTableFilled(sourceDef.tableName) )
        {
            // nothing to do.
            qCDebug(runtime) << "Not needed to update " << sourceDef.fileName;
            emit noUpdate();
            return;
        }

        qCDebug(runtime) << "using cached " << sourceDef.fileName << " at" << dir.path();
        QTimer::singleShot(0, this, [this, sourceDef]() {loadData(sourceDef);});
    }
    else
    {
        qCDebug(runtime) << sourceDef.fileName << " is too old or not exist - downloading";
        download(sourceDef);
    }
}

void LOVDownloader::abortRequest()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
    abortRequested = true;
}

void LOVDownloader::loadData(const LOVDownloader::SourceDefinition &sourceDef)
{
    FCT_IDENTIFICATION;

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QFile file(dir.filePath(sourceDef.fileName));

    emit processingSize(file.size());
    file.open(QIODevice::ReadOnly);
    QTextStream stream(&file);
    parseData(sourceDef, stream);
    file.close();

    emit finished(true);
}

bool LOVDownloader::isTableFilled(const QString &tableName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << tableName;

    QSqlQuery query(QString("select exists( select 1 from %1)").arg(tableName));
    int i = query.first() ? query.value(0).toInt() : 0;

    qCDebug(runtime) << i;
    return i==1;
}

bool LOVDownloader::deleteTable(const QString &tableName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << tableName;

    QSqlQuery query;
    QString queryStatement("delete from %1");

    if ( ! query.exec(queryStatement.arg(tableName)) )
    {
        qWarning() << "Cannot delete " << tableName  << query.lastError();
        return false;
    }

    return true;
}

void LOVDownloader::download(const LOVDownloader::SourceDefinition &sourceDef)
{
    FCT_IDENTIFICATION;

    QUrl url(sourceDef.URL);
    QNetworkRequest request(url);

    request.setRawHeader("User-Agent", "QLog/1.0 (Qt)");

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = nam->get(request);
    currentReply->setProperty("sourceType", sourceDef.type);

    qCDebug(runtime) << "Downloading " << sourceDef.fileName << "from " << url.toString();
}

void LOVDownloader::parseData(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Parsing file " << sourceDef.fileName;

    switch ( sourceDef.type )
    {
    case CTY:
        parseCTY(sourceDef, data);
        break;
    case SATLIST:
        parseSATLIST(sourceDef, data);
        break;
    case SOTASUMMITS:
        parseSOTASummits(sourceDef, data);
        break;
    case WWFFDIRECTORY:
        parseWWFFDirectory(sourceDef, data);
        break;
    case IOTALIST:
        parseIOTA(sourceDef, data);
        break;
    case POTADIRECTORY:
        parsePOTA(sourceDef, data);
        break;
    case MEMBERSHIPCONTENTLIST:
        parseMembershipContent(sourceDef, data);
        break;

    default:
        qWarning() << "Unssorted type to download" << sourceDef.type << sourceDef.fileName;
    }
}

void LOVDownloader::parseCTY(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QRegularExpressionMatch matchExp;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable("dxcc_prefixes") )
    {
        qCWarning(runtime) << "dxcc_prefixes delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << sourceDef.tableName << " delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    QSqlQuery insertEntityQuery;

    if ( ! insertEntityQuery.prepare("INSERT INTO dxcc_entities (id,"
                               "                        name,"
                               "                        prefix,"
                               "                        cont,"
                               "                        cqz,"
                               "                        ituz,"
                               "                        lat,"
                               "                        lon,"
                               "                        tz) "
                               " VALUES (               :id,"
                               "                        :name,"
                               "                        :prefix,"
                               "                        :cont,"
                               "                        :cqz,"
                               "                        :ituz,"
                               "                        :lat,"
                               "                        :lon,"
                               "                        :tz)") )
    {
        qWarning() << "cannot prepare Insert statement - Entity";
        abortRequested = true;
    }

    QSqlQuery insertPrefixesQuery;

    if ( ! insertPrefixesQuery.prepare("INSERT INTO dxcc_prefixes ("
                               "                        prefix,"
                               "                        exact,"
                               "                        dxcc,"
                               "                        cqz,"
                               "                        ituz) "
                               " VALUES (               :prefix,"
                               "                        :exact,"
                               "                        :dxcc,"
                               "                        :cqz,"
                               "                        :ituz)") )
    {
        qWarning() << "cannot prepare Insert statement - Prefixes";
        abortRequested = true;
    }

    unsigned int count = 0;

    while ( !data.atEnd() && !abortRequested )
    {
        const QString &line = data.readLine();
        const QStringList &fields = line.split(',');

        if ( fields.count() != 10 )
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
            continue;
        }
        else if ( fields.at(0).startsWith("*") )
            continue;

        qCDebug(runtime) << fields;

        int dxcc_id = fields.at(2).toInt();

        insertEntityQuery.bindValue(":id", dxcc_id);
        insertEntityQuery.bindValue(":prefix", fields.at(0));
        insertEntityQuery.bindValue(":name", fields.at(1));
        insertEntityQuery.bindValue(":cont", fields.at(3));
        insertEntityQuery.bindValue(":cqz", fields.at(4));
        insertEntityQuery.bindValue(":ituz", fields.at(5));
        insertEntityQuery.bindValue(":lat", fields.at(6).toFloat());
        insertEntityQuery.bindValue(":lon", -fields.at(7).toFloat());
        insertEntityQuery.bindValue(":tz", fields.at(8).toFloat());

        if ( ! insertEntityQuery.exec() )
        {
            qWarning() << "DXCC Entity insert error "
                       << insertEntityQuery.lastError().text()
                       << insertEntityQuery.lastQuery();
            qCDebug(runtime) << fields;
            abortRequested = true;
            continue;
        }
        else
            count++;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        const QStringList &prefixList = fields.at(9).split(CTYPrefixSeperatorRe, Qt::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
        const QStringList &prefixList = fields.at(9).split(CTYPrefixSeperatorRe, QString::SkipEmptyParts);
#endif
        qCDebug(runtime) << prefixList;

        QStringList dup;

        for ( auto &prefix : prefixList )
        {
            matchExp = CTYPrefixFormatRe.match(prefix);
            if ( matchExp.hasMatch() )
            {
                // removing duplicities in CTY file.
                const QString &pfx = matchExp.captured(2);

                if ( !dup.contains(pfx) )
                {
                    dup << pfx;
                    insertPrefixesQuery.bindValue(":prefix", pfx);
                    insertPrefixesQuery.bindValue(":exact", !matchExp.captured(1).isEmpty());
                    insertPrefixesQuery.bindValue(":dxcc", dxcc_id);
                    insertPrefixesQuery.bindValue(":cqz", matchExp.captured(3).toInt());
                    insertPrefixesQuery.bindValue(":ituz", matchExp.captured(4).toInt());

                    if ( ! insertPrefixesQuery.exec() )
                    {
                        qWarning() << "DXCC Prefix insert error "
                                   << insertPrefixesQuery.lastError().text()
                                   << insertPrefixesQuery.lastQuery();
                        qCDebug(runtime) << prefix << prefixList;
                        abortRequested = true;
                    }
                }
                else
                    qCDebug(runtime) << "Removing non-unique prefix" << pfx;
            }
            else
                qCDebug(runtime) << "Failed to match " << prefix;
        }

        if ( count% 20 == 0 )
        {
            emit progress(data.pos());
            QCoreApplication::processEvents();
        }

        emit progress(data.pos());
        QCoreApplication::processEvents();
    }

    if ( !abortRequested )
    {
        qCDebug(runtime) << "DXCC update finished:" << count << "entities loaded.";
        QSqlDatabase::database().commit();
    }
    else
    {
        //can be a result of abort
        qCWarning(runtime) << "DXCC update failed - rollback";
        QSqlDatabase::database().rollback();
    }
}

void LOVDownloader::parseSATLIST(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "Satlist delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    QSqlTableModel entityTableModel;
    entityTableModel.setTable(sourceDef.tableName);
    entityTableModel.setEditStrategy(QSqlTableModel::OnManualSubmit);
    QSqlRecord entityRecord = entityTableModel.record();

    int count = 0;

    while ( !data.atEnd() && !abortRequested )
    {
        QString line = data.readLine();
        QStringList fields = line.split(';');

        if ( fields.count() != 8 )
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
            continue;
        }

        qCDebug(runtime) << fields;

        entityRecord.clearValues();
        entityRecord.setValue("name", fields.at(0));
        entityRecord.setValue("number", fields.at(1));
        entityRecord.setValue("uplink", fields.at(2));
        entityRecord.setValue("downlink", fields.at(3));
        entityRecord.setValue("beacon", fields.at(4));
        entityRecord.setValue("mode", fields.at(5));
        entityRecord.setValue("callsign", fields.at(6));
        entityRecord.setValue("status", fields.at(7));

        if ( !entityTableModel.insertRecord(-1, entityRecord) )
        {
            qWarning() << "Cannot insert a record to SATList Table - " << entityTableModel.lastError();
            qCDebug(runtime) << entityRecord;
        }
        else
        {
            count++;
        }
        emit progress(data.pos());
        QCoreApplication::processEvents();
    }

    if ( entityTableModel.submitAll()
         && !abortRequested )
    {
        QSqlDatabase::database().commit();
        qCDebug(runtime) << "Satlist update finished:" << count << "entities loaded.";
    }
    else
    {
        //can be a result of abort
        qCWarning(runtime) << "Satlist update failed - rollback" << entityTableModel.lastError();
        QSqlDatabase::database().rollback();
    }
}

void LOVDownloader::parseSOTASummits(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "SOTA Summits delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    int count = 0;

    QSqlQuery insertQuery;

    if ( ! insertQuery.prepare("INSERT INTO sota_summits(summit_code,"
                               "                        association_name,"
                               "                        region_name,"
                               "                        summit_name,"
                               "                        altm,"
                               "                        altft,"
                               "                        gridref1,"
                               "                        gridref2,"
                               "                        longitude,"
                               "                        latitude,"
                               "                        points,"
                               "                        bonus_points,"
                               "                        valid_from,"
                               "                        valid_to) "
                               " VALUES (               :summit_code,"
                               "                        :association_name,"
                               "                        :region_name,"
                               "                        :summit_name,"
                               "                        :altm,"
                               "                        :altft,"
                               "                        :gridref1,"
                               "                        :gridref2,"
                               "                        :longitude,"
                               "                        :latitude,"
                               "                        :points,"
                               "                        :bonus_points,"
                               "                        :valid_from,"
                               "                        :valid_to)") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    while ( !data.atEnd() && !abortRequested )
    {
        QString line = data.readLine();
        if ( count == 0 || count ==1 )
        {
            QString checkingString = (count == 0 ) ?
                                      "SOTA Summits List" :
                                      "SummitCode,AssociationName,RegionName,"
                                      "SummitName,AltM,AltFt,GridRef1,GridRef2,"
                                      "Longitude,Latitude,Points,BonusPoints,"
                                      "ValidFrom,ValidTo,ActivationCount,"
                                      "ActivationDate,ActivationCall";
            //read the first line
            if ( !line.contains(checkingString) )
            {
                qCDebug(runtime) << line;
                qWarning() << "Unexpected header for SOTA Summit CSV file - aborting";
                abortRequested = true;
            }
            count++;
            continue;
        }

        QRegularExpressionMatchIterator i = CSVRe.globalMatch(line);
        QStringList fields;

        while ( i.hasNext() )
        {
            QRegularExpressionMatch match = i.next();
            fields << match.captured(2);
        }

        if ( fields.size() >= 14 )
        {
            qCDebug(runtime) << fields;

            insertQuery.bindValue(":summit_code", fields.at(0));
            insertQuery.bindValue(":association_name", fields.at(1));
            insertQuery.bindValue(":region_name", fields.at(2));
            insertQuery.bindValue(":summit_name", fields.at(3));
            insertQuery.bindValue(":altm", fields.at(4));
            insertQuery.bindValue(":altft", fields.at(5));
            insertQuery.bindValue(":gridref1", fields.at(6));
            insertQuery.bindValue(":gridref2", fields.at(7));
            insertQuery.bindValue(":longitude", fields.at(8));
            insertQuery.bindValue(":latitude", fields.at(9));
            insertQuery.bindValue(":points", fields.at(10));
            insertQuery.bindValue(":bonus_points", fields.at(11));
            insertQuery.bindValue(":valid_from", fields.at(12));
            insertQuery.bindValue(":valid_to", fields.at(13));

            if ( ! insertQuery.exec() )
            {
                qInfo() << "SOTA Summit insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
                abortRequested = true;
                continue;
            }

            if ( count%10000 == 0 )
            {
                emit progress(data.pos());
                QCoreApplication::processEvents();
            }
        }
        else
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
        }
        count++;
    }

    if ( !abortRequested )
    {
        QSqlDatabase::database().commit();
        qCDebug(runtime) << "SOTA Summits update finished:" << count << "entities loaded.";
    }
    else
    {
        qCWarning(runtime) << "SOTA Summits update failed - rollback";
        QSqlDatabase::database().rollback();
    }
}

void LOVDownloader::parseWWFFDirectory(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "WWFT Directory delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    int count = 0;

    QSqlQuery insertQuery;

    if ( ! insertQuery.prepare("INSERT INTO wwff_directory(reference,"
                               "                        status,"
                               "                        name,"
                               "                        program,"
                               "                        dxcc,"
                               "                        state,"
                               "                        county,"
                               "                        continent,"
                               "                        iota,"
                               "                        iaruLocator,"
                               "                        latitude,"
                               "                        longitude,"
                               "                        iucncat,"
                               "                        valid_from,"
                               "                        valid_to) "
                               " VALUES (               :reference,"
                               "                        :status,"
                               "                        :name,"
                               "                        :program,"
                               "                        :dxcc,"
                               "                        :state,"
                               "                        :county,"
                               "                        :continent,"
                               "                        :iota,"
                               "                        :iaruLocator,"
                               "                        :latitude,"
                               "                        :longitude,"
                               "                        :iucncat,"
                               "                        :valid_from,"
                               "                        :valid_to) ") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    while ( !data.atEnd() && !abortRequested )
    {
        QString line = data.readLine();
        if ( count == 0 )
        {
            QString checkingString = "reference,status,"
                                     "name,program,dxcc,state,"
                                     "county,continent,iota,"
                                     "iaruLocator,latitude,"
                                     "longitude,IUCNcat,validFrom,"
                                     "validTo,notes,lastMod,changeLog,"
                                     "reviewFlag,specialFlags,website,"
                                     "country,region";
            //read the first line
            if ( !line.contains(checkingString) )
            {
                qCDebug(runtime) << line;
                qWarning() << "Unexpected header for WWFF Directory CSV file - aborting";
                abortRequested = true;
            }
            count++;
            continue;
        }

        QRegularExpressionMatchIterator i = CSVRe.globalMatch(line);
        QStringList fields;

        while ( i.hasNext() )
        {
            QRegularExpressionMatch match = i.next();
            fields << match.captured(2);
        }

        if ( fields.size() >= 15 )
        {
            qCDebug(runtime) << fields;

            insertQuery.bindValue(":reference", fields.at(0));
            insertQuery.bindValue(":status", fields.at(1));
            insertQuery.bindValue(":name", fields.at(2));
            insertQuery.bindValue(":program", fields.at(3));
            insertQuery.bindValue(":dxcc", fields.at(4));
            insertQuery.bindValue(":state", fields.at(5));
            insertQuery.bindValue(":county", fields.at(6));
            insertQuery.bindValue(":continent", fields.at(7));
            insertQuery.bindValue(":iota", fields.at(8));
            insertQuery.bindValue(":iaruLocator", fields.at(9));
            insertQuery.bindValue(":latitude", fields.at(10));
            insertQuery.bindValue(":longitude", fields.at(11));
            insertQuery.bindValue(":iucncat", fields.at(12));
            insertQuery.bindValue(":valid_from", fields.at(13));
            insertQuery.bindValue(":valid_to", fields.at(14));

            if ( ! insertQuery.exec() )
            {
                qInfo() << "WWFT Directory insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
                abortRequested = true;
                continue;
            }

            if ( count%10000 == 0 )
            {
                emit progress(data.pos());
                QCoreApplication::processEvents();
            }
        }
        else
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
        }
        count++;
    }

    if ( !abortRequested )
    {
        QSqlDatabase::database().commit();
        qCDebug(runtime) << "WWFT Directory update finished:" << count << "entities loaded.";
    }
    else
    {
        qCWarning(runtime) << "WWFT Directory update failed - rollback";
        QSqlDatabase::database().rollback();
    }
}

void LOVDownloader::parseIOTA(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "IOTA List delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    int count = 0;

    QSqlQuery insertQuery;

    if ( ! insertQuery.prepare("INSERT INTO IOTA(iotaid,"
                               "                 islandname)"
                               " VALUES (:iotaid,"
                               "         :islandname)") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    while ( !data.atEnd() && !abortRequested )
    {
        QString line = data.readLine();
        if ( count == 0 )
        {
            QString checkingString = "iotaid, islandname";
            //read the first line
            if ( !line.contains(checkingString) )
            {
                qCDebug(runtime) << line;
                qWarning() << "Unexpected header for IOTA CSV file - aborting";
                abortRequested = true;
            }
            count++;
            continue;
        }

        QRegularExpressionMatchIterator i = CSVRe.globalMatch(line);
        QStringList fields;

        while ( i.hasNext() )
        {
            QRegularExpressionMatch match = i.next();
            fields << match.captured(2);
        }

        if ( fields.size() >= 2 )
        {
            qCDebug(runtime) << fields;

            insertQuery.bindValue(":iotaid", fields.at(0));
            insertQuery.bindValue(":islandname", fields.at(1));

            if ( ! insertQuery.exec() )
            {
                qInfo() << "IOTA Directory insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
                abortRequested = true;
                continue;
            }

            if ( count%100 == 0 )
            {
                emit progress(data.pos());
                QCoreApplication::processEvents();
            }
        }
        else
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
        }
        count++;
    }

    if ( !abortRequested )
    {
        QSqlDatabase::database().commit();
        qCDebug(runtime) << "IOTA update finished:" << count << "entities loaded.";
    }
    else
    {
        qCWarning(runtime) << "IOTA update failed - rollback";
        QSqlDatabase::database().rollback();
    }
}

void LOVDownloader::parsePOTA(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "POTA List delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    int count = 0;

    QSqlQuery insertQuery;

    if ( ! insertQuery.prepare("INSERT INTO POTA_DIRECTORY(reference,"
                               "                           name,"
                               "                           active,"
                               "                           entityID,"
                               "                           locationDesc,"
                               "                           latitude,"
                               "                           longitude,"
                               "                           grid"
                               ")"
                               " VALUES (:reference,"
                               "         :name,"
                               "         :active,"
                               "         :entityID,"
                               "         :locationDesc,"
                               "         :latitude,"
                               "         :longitude,"
                               "         :grid"
                               ")") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    while ( !data.atEnd() && !abortRequested )
    {
        QString line = data.readLine();
        if ( count == 0 )
        {
            QString checkingString = "\"reference\",\"name\",\"active\",\"entityId\",\"locationDesc\",\"latitude\",\"longitude\",\"grid\"";
            //read the first line
            if ( !line.contains(checkingString) )
            {
                qCDebug(runtime) << line;
                qWarning() << "Unexpected header for POTA CSV file - aborting";
                abortRequested = true;
            }
            count++;
            continue;
        }

        QRegularExpressionMatchIterator i = CSVRe.globalMatch(line);
        QStringList fields;

        while ( i.hasNext() )
        {
            QRegularExpressionMatch match = i.next();
            fields << match.captured(2);
        }

        if ( fields.size() >= 8 )
        {
            qCDebug(runtime) << fields;

            insertQuery.bindValue(":reference", fields.at(0));
            insertQuery.bindValue(":name", fields.at(1));
            insertQuery.bindValue(":active", fields.at(2));
            insertQuery.bindValue(":entityID", fields.at(3));
            insertQuery.bindValue(":locationDesc", fields.at(4));
            insertQuery.bindValue(":latitude", fields.at(5));
            insertQuery.bindValue(":longitude", fields.at(6));
            insertQuery.bindValue(":grid", fields.at(7));

            if ( ! insertQuery.exec() )
            {
                qInfo() << "POTA Directory insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
                abortRequested = true;
                continue;
            }

            if ( count%3000 == 0 )
            {
                emit progress(data.pos());
                QCoreApplication::processEvents();
            }
        }
        else
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
        }
        count++;
    }

    if ( !abortRequested )
    {
        QSqlDatabase::database().commit();
        qCDebug(runtime) << "POTA update finished:" << count << "entities loaded.";
    }
    else
    {
        qCWarning(runtime) << "POTA update failed - rollback";
        QSqlDatabase::database().rollback();
    }
}

void LOVDownloader::parseMembershipContent(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "Membership Directory delete failed - rollback";
        QSqlDatabase::database().rollback();
        return;
    }

    QSqlTableModel entityTableModel;
    entityTableModel.setTable(sourceDef.tableName);
    entityTableModel.setEditStrategy(QSqlTableModel::OnManualSubmit);
    QSqlRecord entityRecord = entityTableModel.record();

    int count = 0;

    while ( !data.atEnd() && !abortRequested )
    {
        QString line = data.readLine();
        QStringList fields = line.split(',');

        if ( fields.count() != 5 )
        {
            qCDebug(runtime) << "Invalid line in the input file " << line;
            continue;
        }

        qCDebug(runtime) << fields;

        entityRecord.clearValues();
        entityRecord.setValue("short_desc", fields.at(0));
        entityRecord.setValue("long_desc", fields.at(1));
        entityRecord.setValue("filename", fields.at(2));
        entityRecord.setValue("last_update", fields.at(3));
        entityRecord.setValue("num_records", fields.at(4));

        if ( !entityTableModel.insertRecord(-1, entityRecord) )
        {
            qWarning() << "Cannot insert a record to Membership Directory Table - " << entityTableModel.lastError();
            qCDebug(runtime) << entityRecord;
        }
        else
        {
            count++;
        }
        emit progress(data.pos());
        QCoreApplication::processEvents();
    }

    if ( entityTableModel.submitAll()
         && !abortRequested )
    {
        QSqlDatabase::database().commit();
        qCDebug(runtime) << "Membership Directory update finished:" << count << "entities loaded.";
    }
    else
    {
        //can be a result of abort
        qCWarning(runtime) << "Membership Directory update failed - rollback" << entityTableModel.lastError();
        QSqlDatabase::database().rollback();
    }

}

void LOVDownloader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    currentReply = nullptr;

    QByteArray data = reply->readAll();
    uint sourceTypeNum = reply->property("sourceType").toUInt();

    qCDebug(runtime) << "Received Source type " << sourceTypeNum;

    SourceType sourceType = static_cast<SourceType>(sourceTypeNum);
    SourceDefinition sourceDef = sourceMapping[sourceType];

    Q_ASSERT(sourceDef.type == sourceType);

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->isFinished()
         && reply->error() == QNetworkReply::NoError
         && replyStatusCode >= 200 && replyStatusCode < 300)
    {
        qCDebug(runtime) << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        qCDebug(runtime) << reply->header(QNetworkRequest::KnownHeaders::LocationHeader);

        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

        QFile file(dir.filePath(sourceDef.fileName));
        file.open(QIODevice::WriteOnly);
        file.write(data);
        file.flush();
        file.close();
        reply->deleteLater();

        LogParam::setParam(sourceDef.lastTimeConfigName, QDateTime::currentDateTimeUtc().date());
        loadData(sourceDef);
    }
    else
    {
        qCDebug(runtime) << "Failed to download " << sourceDef.fileName;

        reply->deleteLater();
        emit finished(false);
    }
}
