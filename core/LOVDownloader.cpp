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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QSqlQuery>
#include <QSqlError>
#include <zlib.h>

#include "LogParam.h"
#include "LOVDownloader.h"
#include "debug.h"
#include "data/Data.h"

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
    const QDate &last_update = LogParam::getLOVaParam(sourceDef.lastTimeConfigName);

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

    QString rheader = QString("QLog/%1").arg(VERSION);
    request.setRawHeader("User-Agent", rheader.toUtf8());

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
    case CLUBLOGCTY:
        parseClubLogCTY(sourceDef, data);
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

    if ( ! deleteTable("dxcc_prefixes_ad1c") )
    {
        qCWarning(runtime) << "dxcc_prefixes_ad1c delete failed - rollback";
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

    if ( ! insertEntityQuery.prepare("INSERT INTO dxcc_entities_ad1c (id,"
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

    if ( ! insertPrefixesQuery.prepare("INSERT INTO dxcc_prefixes_ad1c ("
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
                               " VALUES (               ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?,"
                               "                        ?)") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    QVariantList sota_summit_code;
    QVariantList sota_association_name;
    QVariantList sota_region_name;
    QVariantList sota_summit_name;
    QVariantList sota_altm;
    QVariantList sota_altft;
    QVariantList sota_gridref1;
    QVariantList sota_gridref2;
    QVariantList sota_longitude;
    QVariantList sota_latitude;
    QVariantList sota_points;
    QVariantList sota_bonus_points;
    QVariantList sota_valid_from;
    QVariantList sota_valid_to;

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

            sota_summit_code << fields.at(0);
            sota_association_name << fields.at(1);
            sota_region_name << fields.at(2);
            sota_summit_name << fields.at(3);
            sota_altm << fields.at(4);
            sota_altft << fields.at(5);
            sota_gridref1 << fields.at(6);
            sota_gridref2 << fields.at(7);
            sota_longitude << fields.at(8);
            sota_latitude << fields.at(9);
            sota_points << fields.at(10);
            sota_bonus_points << fields.at(11);
            sota_valid_from << fields.at(12);
            sota_valid_to << fields.at(13);

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

    insertQuery.addBindValue(sota_summit_code);
    insertQuery.addBindValue(sota_association_name);
    insertQuery.addBindValue(sota_region_name);
    insertQuery.addBindValue(sota_summit_name);
    insertQuery.addBindValue(sota_altm);
    insertQuery.addBindValue(sota_altft);
    insertQuery.addBindValue(sota_gridref1);
    insertQuery.addBindValue(sota_gridref2);
    insertQuery.addBindValue(sota_longitude);
    insertQuery.addBindValue(sota_latitude);
    insertQuery.addBindValue(sota_points);
    insertQuery.addBindValue(sota_bonus_points);
    insertQuery.addBindValue(sota_valid_from);
    insertQuery.addBindValue(sota_valid_to);

    if ( ! insertQuery.execBatch() )
    {
        qInfo() << "SOTA Summit insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
        abortRequested = true;
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
                             " VALUES (               ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?,"
                             "                        ?) ") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    QVariantList wwff_reference;
    QVariantList wwff_status;
    QVariantList wwff_name;
    QVariantList wwff_program;
    QVariantList wwff_dxcc;
    QVariantList wwff_state;
    QVariantList wwff_county;
    QVariantList wwff_continent;
    QVariantList wwff_iota;
    QVariantList wwff_iaruLocator;
    QVariantList wwff_latitude;
    QVariantList wwff_longitude;
    QVariantList wwff_iucncat;
    QVariantList wwff_valid_from;
    QVariantList wwff_valid_to;

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

            wwff_reference << fields.at(0);
            wwff_status << fields.at(1);
            wwff_name << fields.at(2);
            wwff_program << fields.at(3);
            wwff_dxcc << fields.at(4);
            wwff_state << fields.at(5);
            wwff_county << fields.at(6);
            wwff_continent << fields.at(7);
            wwff_iota << fields.at(8);
            wwff_iaruLocator << fields.at(9);
            wwff_latitude << fields.at(10);
            wwff_longitude << fields.at(11);
            wwff_iucncat << fields.at(12);
            wwff_valid_from << fields.at(13);
            wwff_valid_to << fields.at(14);

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

    insertQuery.addBindValue(wwff_reference);
    insertQuery.addBindValue(wwff_status);
    insertQuery.addBindValue(wwff_name);
    insertQuery.addBindValue(wwff_program);
    insertQuery.addBindValue(wwff_dxcc);
    insertQuery.addBindValue(wwff_state);
    insertQuery.addBindValue(wwff_county);
    insertQuery.addBindValue(wwff_continent);
    insertQuery.addBindValue(wwff_iota);
    insertQuery.addBindValue(wwff_iaruLocator);
    insertQuery.addBindValue(wwff_latitude);
    insertQuery.addBindValue(wwff_longitude);
    insertQuery.addBindValue(wwff_iucncat);
    insertQuery.addBindValue(wwff_valid_from);
    insertQuery.addBindValue( wwff_valid_to);


    if ( ! insertQuery.execBatch() )
    {
        qInfo() << "WWFT Directory insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
        abortRequested = true;
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

    QSqlQuery insertQuery;
    if ( ! insertQuery.prepare("INSERT INTO IOTA(iotaid,"
                             "                 islandname)"
                             " VALUES (?, ?)") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
        return;
    }

    QSqlDatabase::database().transaction();

    if ( ! deleteTable(sourceDef.tableName) )
    {
        qCWarning(runtime) << "IOTA List delete failed - rollback";
        abortRequested = true;
        QSqlDatabase::database().rollback();
        return;
    }

    unsigned int count = 0;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.readAll().toUtf8());

    if ( !jsonDoc.isArray() )
    {
        qCDebug(runtime) << jsonDoc;
        qWarning() << "Unexpected IOTA JSON - aborting";
        abortRequested = true;
    }
    else
    {
        QVariantList iota_id;
        QVariantList iota_islandname;

        const QJsonArray &jsonArray = jsonDoc.array();

        for ( const QJsonValue &value : jsonArray )
        {
            if ( !value.isObject() ) continue;

            const QJsonObject &obj = value.toObject();
            const QJsonValue &refno = obj.value("refno");
            const QJsonValue &name = obj.value("name");

            qCDebug(runtime) << "IOTA Record" << refno << name;

            iota_id << refno;
            iota_islandname << name;

            if ( count%500 == 0 )
            {
                emit progress(data.pos());
                QCoreApplication::processEvents();
            }
            count++;
        }

        insertQuery.addBindValue(iota_id);
        insertQuery.addBindValue(iota_islandname);

        if ( ! insertQuery.execBatch() )
        {
            qInfo() << "IOTA Directory insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
            abortRequested = true;
        }
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
                               " VALUES (?,"
                               "         ?,"
                               "         ?,"
                               "         ?,"
                               "         ?,"
                               "         ?,"
                               "         ?,"
                               "         ?"
                               ")") )
    {
        qWarning() << "cannot prepare Insert statement";
        abortRequested = true;
    }

    QVariantList pota_reference;
    QVariantList pota_name;
    QVariantList pota_active;
    QVariantList pota_entityID;
    QVariantList pota_locationDesc;
    QVariantList pota_latitude;
    QVariantList pota_longitude;
    QVariantList pota_grid;

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

            pota_reference << fields.at(0);
            pota_name << fields.at(1);
            pota_active << fields.at(2);
            pota_entityID << fields.at(3);
            pota_locationDesc << fields.at(4);
            pota_latitude << fields.at(5);
            pota_longitude << fields.at(6);
            pota_grid << fields.at(7);

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

    insertQuery.addBindValue(pota_reference);
    insertQuery.addBindValue(pota_name);
    insertQuery.addBindValue(pota_active);
    insertQuery.addBindValue(pota_entityID);
    insertQuery.addBindValue(pota_locationDesc);
    insertQuery.addBindValue(pota_latitude);
    insertQuery.addBindValue(pota_longitude);
    insertQuery.addBindValue(pota_grid);

    if ( ! insertQuery.execBatch() )
    {
        qInfo() << "POTA Directory insert error " << insertQuery.lastError().text() << insertQuery.lastQuery();
        abortRequested = true;
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

QByteArray LOVDownloader::gunzip(const QByteArray &in)
{
    if ( in.isEmpty() ) return {};

    z_stream strm{};

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    strm.avail_in = in.size();

    // 16 + MAX_WBITS tells zlib to parse gzip header/footer
    if ( inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK ) return {};

    QByteArray out;
    char buf[8192];
    int ret = Z_OK;
    while ( ret == Z_OK )
    {
        strm.next_out = reinterpret_cast<Bytef*>(buf);
        strm.avail_out = sizeof(buf);
        ret = inflate(&strm, Z_NO_FLUSH);

        if (ret == Z_OK || ret == Z_STREAM_END)
            out.append(buf, sizeof(buf) - strm.avail_out);
    }
    inflateEnd(&strm);
    return out;
}

void LOVDownloader::parseClubLogCTY(const SourceDefinition &sourceDef, QTextStream &data)
{
    FCT_IDENTIFICATION;

    if (sourceDef.type != CLUBLOGCTY) return;

    // Read whole text (it’s XML); QXmlStreamReader can also take QIODevice, but we
    // already have a QTextStream here.
    QXmlStreamReader xml(data.readAll());

    // Clean all five tables inside one transaction
    QSqlDatabase::database().transaction();
    auto rollback = [&]()
    {
        qCWarning(runtime) << "ClubLog CTY import failed - rollback";
        QSqlDatabase::database().rollback();
    };

    if ( !deleteTable("dxcc_zone_exceptions_clublog")
        || !deleteTable("dxcc_prefixes_clublog")
        || !deleteTable("dxcc_entities_clublog"))
    {
        rollback();
        return;
    }

    QSqlQuery insEntity, insPrefix,insZone;

    // prepared statements
    if (!insEntity.prepare(
                "INSERT INTO dxcc_entities_clublog(id, name, prefix, deleted, cqz, ituz, cont, lon, lat, start, \"end\")"
                "VALUES(:id, :name, :prefix, :deleted, :cqz, :ituz, :cont, :lon, :lat, :start, :end)"))
    {
        qWarning() << insEntity.lastError(); rollback(); return;
    }

    if (!insPrefix.prepare(
                "INSERT INTO dxcc_prefixes_clublog(prefix, exact, dxcc, cqz, cont, lon, lat, start, \"end\")"
                "VALUES(:prefix, :exact, :dxcc, :cqz, :cont, :lon, :lat, :start, :end)"))
    {
        qWarning() << insPrefix.lastError(); rollback(); return;
    }

    if (!insZone.prepare(
                "INSERT INTO dxcc_zone_exceptions_clublog(record, call, cqz, start, \"end\")"
                "VALUES(:record, :call, :cqz, :start, :end)"))
    {
        qWarning() << insZone.lastError(); rollback(); return;
    }

    auto readText = [&](QXmlStreamReader &x)->QString { return x.readElementText().trimmed(); };
    auto readDate = [&](const QString &s)->QString { return s; }; // store ISO8601 text as-is

    // Cursor down to <clublog>
    while (!xml.atEnd() && !(xml.isStartElement() && xml.name() == QStringLiteral("clublog"))) xml.readNext();

    if (xml.atEnd())
    {
        qWarning() << "ClubLog: <clublog> not found";
        rollback(); return;
    }

    quint32 readOp = 0;

    auto updateReadProgress = [&]()
    {
        readOp++;
        if ( readOp % 200 == 0 )
        {
            emit progress(xml.characterOffset());
            QCoreApplication::processEvents();
        }
    };

    // Iterate children of <clublog>
    while (!xml.atEnd())
    {
        xml.readNext();

        if ( !xml.isStartElement() ) continue;

        const QString &top = xml.name().toString();

        if ( top == "entities" )
        {
            // <entities><entity>...</entity>...</entities>
            while (!(xml.isEndElement() && xml.name() == QStringLiteral("entities")))
            {
                xml.readNext();

                updateReadProgress();

                if ( xml.isStartElement() && xml.name() == QStringLiteral("entity") )
                {
                    // parse one entity
                    int adif = 0; QString name, prefix, cont; bool deleted=false;
                    int cqz = 0; QString start, end;
                    double lon=0.0, lat=0.0;

                    while ( !(xml.isEndElement() && xml.name() == QStringLiteral("entity")))
                    {
                        xml.readNext();

                        updateReadProgress();

                        if ( !xml.isStartElement() ) continue;

                        const QString &tag = xml.name().toString();

                        if ( tag=="adif" ) adif = readText(xml).toInt();
                        else if ( tag=="name" ) name = readText(xml);
                        else if ( tag=="prefix" ) prefix = readText(xml);
                        else if ( tag=="deleted" ) deleted = (readText(xml).compare("true", Qt::CaseInsensitive) == 0);
                        else if ( tag=="cqz" ) cqz = readText(xml).toInt();
                        else if ( tag=="cont" ) cont = readText(xml);
                        else if ( tag=="long" ) lon = readText(xml).toDouble();
                        else if ( tag=="lat" )  lat = readText(xml).toDouble();
                        else if ( tag=="start" ) start = readDate(readText(xml));
                        else if ( tag=="end" )   end   = readDate(readText(xml));
                        else xml.skipCurrentElement();
                    }

                    insEntity.bindValue(":id", adif);

                    const QString &nameModified = Data::instance()->dxccName(adif);

                    insEntity.bindValue(":name", nameModified.isEmpty() ? name : nameModified);
                    insEntity.bindValue(":prefix", prefix);
                    insEntity.bindValue(":deleted", deleted ? 1 : 0);
                    insEntity.bindValue(":cqz", cqz ? cqz : 0);
                    insEntity.bindValue(":ituz", Data::instance()->dxccITUZ(adif));
                    insEntity.bindValue(":cont", cont.isEmpty()? QVariant(): cont);
                    insEntity.bindValue(":lon",  lon);
                    insEntity.bindValue(":lat",  lat);
                    insEntity.bindValue(":start", start.isEmpty()? QVariant() : start);
                    insEntity.bindValue(":end",   end.isEmpty()?   QVariant() : end);

                    if (!insEntity.exec()) { qWarning() << insEntity.lastError(); rollback(); return; }
                }
            }
        }
        else if (top == "prefixes" || top == "exceptions")
        {
            // these two share the same internal structure: <prefix|exception record='...'>...</...>
            bool isPrefix = (top=="prefixes");

            while (!(xml.isEndElement() && xml.name() == top))
            {
                xml.readNext();

                updateReadProgress();

                if ( xml.isStartElement() && (xml.name() == QStringLiteral("prefix")
                                              || xml.name()==QStringLiteral("exception")))
                {
                    QString call, cont, start, end;
                    int adif=0, cqz=0; double lon=0.0, lat=0.0;

                    while ( !(xml.isEndElement() && (xml.name()==QStringLiteral("prefix")
                                                     || xml.name()==QStringLiteral("exception"))))
                    {
                        xml.readNext();

                        updateReadProgress();

                        if ( !xml.isStartElement() ) continue;

                        const QString tag = xml.name().toString();

                        if ( tag=="call" ) call = readText(xml);
                        else if ( tag=="adif" ) adif = readText(xml).toInt();
                        else if ( tag=="cqz" ) cqz = readText(xml).toInt();
                        else if ( tag=="cont" ) cont = readText(xml);
                        else if ( tag=="long" ) lon = readText(xml).toDouble();
                        else if ( tag=="lat" )  lat = readText(xml).toDouble();
                        else if ( tag=="start" ) start = readDate(readText(xml));
                        else if ( tag=="end" )   end = readDate(readText(xml));
                        else xml.skipCurrentElement();
                    }

                    insPrefix.bindValue(":prefix", call);
                    insPrefix.bindValue(":exact", !isPrefix);
                    insPrefix.bindValue(":dxcc", adif);
                    insPrefix.bindValue(":cqz", cqz ? cqz : 0);
                    insPrefix.bindValue(":ituz", 0);
                    insPrefix.bindValue(":cont", cont.isEmpty()? QVariant() : cont);
                    insPrefix.bindValue(":lon", lon);
                    insPrefix.bindValue(":lat", lat);
                    insPrefix.bindValue(":start", start.isEmpty()? QVariant() : start);
                    insPrefix.bindValue(":end",   end.isEmpty()?   QVariant() : end);

                    if (!insPrefix.exec())
                    {
                        qWarning() << insPrefix.lastError();
                        rollback();
                        return;
                    }
                }
            }
        }
        else if ( top == "zone_exceptions" )
        {
            while ( !(xml.isEndElement() && xml.name() == QStringLiteral("zone_exceptions")) )
            {
                xml.readNext();

                updateReadProgress();

                if ( xml.isStartElement() && xml.name()==QStringLiteral("zone_exception") )
                {
                    bool ok=false; quint32 rec = xml.attributes().value("record").toUInt(&ok);
                    QString call, start, end; int zone=0;

                    while ( !(xml.isEndElement() && xml.name()==QStringLiteral("zone_exception")) )
                    {
                        xml.readNext();

                        updateReadProgress();

                        if (!xml.isStartElement()) continue;

                        const QString &tag = xml.name().toString();
                        if ( tag=="call" ) call = readText(xml);
                        else if ( tag=="zone" ) zone = readText(xml).toInt();
                        else if ( tag=="start" ) start = readDate(readText(xml));
                        else if ( tag=="end" )   end   = readDate(readText(xml));
                        else xml.skipCurrentElement();
                    }
                    insZone.bindValue(":record", rec);
                    insZone.bindValue(":call", call);
                    insZone.bindValue(":cqz", zone);
                    insZone.bindValue(":start", start);
                    insZone.bindValue(":end",   end);
                    if (!insZone.exec())
                    {
                        qWarning() << insZone.lastError();
                        rollback();
                        return;
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
        qWarning() << "ClubLog XML error:" << xml.errorString();
        rollback();
        return;
    }

    QSqlDatabase::database().commit();
    qCDebug(runtime) << "ClubLog CTY import finished.";
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
        if (sourceType == CLUBLOGCTY)
        {
            QByteArray maybeXml = gunzip(data);
            if ( !maybeXml.isEmpty() )
                data = maybeXml;
        }
        file.write(data);
        file.flush();
        file.close();
        reply->deleteLater();

        LogParam::setLOVParam(sourceDef.lastTimeConfigName, QDateTime::currentDateTimeUtc().date());
        loadData(sourceDef);
    }
    else
    {
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;
        qCDebug(runtime) << "Failed to download " << sourceDef.fileName;

        reply->deleteLater();
        emit finished(false);
    }
}
