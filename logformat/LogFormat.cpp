#include <QtSql>
#include <QSqlDriver>
#include "LogFormat.h"
#include "AdiFormat.h"
#include "AdxFormat.h"
#include "PotaAdiFormat.h"
#include "JsonFormat.h"
#include "CSVFormat.h"
#include "data/Data.h"
#include "core/debug.h"
#include "data/Gridsquare.h"
#include "data/Callsign.h"
#include "data/BandPlan.h"
#include "models/LogbookModel.h"
#include "core/QSOFilterManager.h"

MODULE_IDENTIFICATION("qlog.logformat.logformat");

LogFormat::LogFormat(QTextStream& stream) :
    QObject(nullptr),
    stream(stream),
    exportedFields("*"),
    duplicateQSOFunc(nullptr)
{
    FCT_IDENTIFICATION;
    this->defaults = nullptr;
}

LogFormat::~LogFormat() {
    FCT_IDENTIFICATION;
}

LogFormat* LogFormat::open(QString type, QTextStream& stream) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<type;

    type = type.toLower();

    if (type == "adi") {
        return open(LogFormat::ADI, stream);
    }
    else if (type == "adx") {
        return open(LogFormat::ADX, stream);
    }
    else if (type == "json") {
        return open(LogFormat::JSON, stream);
    }
    else if (type == "csv") {
        return open(LogFormat::CSV, stream);
    }
    else if (type == "cabrillo") {
        return open(LogFormat::JSON, stream);
    }
    else if (type == "pota") {
        return open(LogFormat::POTA, stream);
    }
    else {
        return nullptr;
    }
}

LogFormat* LogFormat::open(LogFormat::Type type, QTextStream& stream) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<type;

    switch (type) {
    case LogFormat::ADI:
        return new AdiFormat(stream);

    case LogFormat::ADX:
        return new AdxFormat(stream);

    case LogFormat::JSON:
        return new JsonFormat(stream);

    case LogFormat::CSV:
        return new CSVFormat(stream);

    case LogFormat::CABRILLO:
        return nullptr;

    case LogFormat::POTA:
        return new PotaAdiFormat(stream);

    default:
        return nullptr;
    }
}

void LogFormat::setDefaults(QMap<QString, QString>& defaults) {
    FCT_IDENTIFICATION;

    this->defaults = &defaults;
}

void LogFormat::setFilterDateRange(const QDate &start, const QDate &end)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<start << " " << end;
    this->filterStartDate = start;
    this->filterEndDate = end;
}

void LogFormat::setFilterMyCallsign(const QString &myCallsing)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << myCallsing;
    this->filterMyCallsign = myCallsing;
}

void LogFormat::setFilterMyGridsquare(const QString &myGridsquare)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << myGridsquare;
    this->filterMyGridsquare = myGridsquare;
}

void LogFormat::setFilterSentPaperQSL(bool includeNo, bool includeIgnore, bool includeAlreadySent)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << includeNo << includeIgnore << includeAlreadySent;

    this->filterSentPaperQSL << "'R'" << "'Q'";

    if ( includeNo )
    {
        this->filterSentPaperQSL << "'N'";
    }
    if ( includeIgnore )
    {
        this->filterSentPaperQSL << "'I'";
    }
    if ( includeAlreadySent )
    {
        this->filterSentPaperQSL << "'Y'";
    }
}

void LogFormat::setFilterSendVia(const QString &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << value;

    this->filterSendVia = value;
}

void LogFormat::setUserFilter(const QString &value)
{
    FCT_IDENTIFICATION;
    userFilter = value;
}

void LogFormat::setPotaOnly(bool only)
{
    FCT_IDENTIFICATION;
    filterPOTAOnly = only;
}

QString LogFormat::getWhereClause()
{
    FCT_IDENTIFICATION;

    whereClause.clear();

    whereClause << "1 = 1"; //generic filter

    if ( isDateRange() )
        whereClause << "(datetime(start_time) BETWEEN datetime(:start_date) AND datetime(:end_date))";

    if ( !filterMyCallsign.isEmpty() )
        whereClause << "upper(station_callsign) = upper(:stationCallsign)";

    if ( !filterMyGridsquare.isEmpty() )
        whereClause << "upper(my_gridsquare) = upper(:myGridsquare)";

    if ( !filterSentPaperQSL.isEmpty() )
        whereClause << QString("upper(qsl_sent) in  (%1)").arg(filterSentPaperQSL.join(", "));

    if ( !filterSendVia.isEmpty() )
        whereClause << ( ( filterSendVia == " " ) ? "qsl_sent_via is NULL"
                                                  : "upper(qsl_sent_via) = upper(:qsl_sent_via)");

    if ( filterPOTAOnly )
        whereClause << QLatin1String("(my_pota_ref is not NULL OR pota_ref is not NULL OR lower(sig)='pota' OR lower(my_sig)='pota')");

    if ( !userFilter.isEmpty() )
        whereClause << QSOFilterManager::getWhereClause(userFilter);

    return whereClause.join(" AND ");
}

void LogFormat::bindWhereClause(QSqlQuery &query)
{
    FCT_IDENTIFICATION;

    if ( isDateRange() )
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        query.bindValue(":start_date", filterStartDate.startOfDay());
        query.bindValue(":end_date", filterEndDate.endOfDay());
#else /* Due to ubuntu 20.04 where qt5.12 is present */
        query.bindValue(":start_date", QDateTime(filterStartDate));
        query.bindValue(":end_date", QDateTime(filterEndDate));
#endif
    }

    if ( !filterMyCallsign.isEmpty() )
    {
        query.bindValue(":stationCallsign", filterMyCallsign);
    }

    if ( !filterMyGridsquare.isEmpty() )
    {
        query.bindValue(":myGridsquare", filterMyGridsquare);
    }

    if ( !filterSendVia.isEmpty() )
    {
        if ( filterSendVia != " " )
        {
            query.bindValue(":qsl_sent_via", filterSendVia);
        }
    }
}

void LogFormat::setExportedFields(const QStringList &fieldsList)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << fieldsList;
    exportedFields = fieldsList;
}

void LogFormat::setUpdateDxcc(bool updateDxcc) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<updateDxcc;

    this->updateDxcc = updateDxcc;
}

void LogFormat::setDuplicateQSOCallback(duplicateQSOBehaviour (*func)(QSqlRecord *, QSqlRecord *))
{
    FCT_IDENTIFICATION;

    duplicateQSOFunc = func;
}

#define RECORDIDX(a) ( (a) - 1 )

unsigned long LogFormat::runImport(QTextStream& importLogStream,
                                   const StationProfile *defaultStationProfile,
                                   unsigned long *warnings,
                                   unsigned long *errors)
{
    FCT_IDENTIFICATION;

    this->importStart();

    unsigned long count = 0L;
    *errors = 0L;
    *warnings = 0L;
    unsigned long processedRec = 0;

    QSqlQuery dupQuery;
    QSqlQuery insertQuery;

    // It is important to use callsign index here
    if ( ! dupQuery.prepare("SELECT * FROM contacts "
                            "WHERE callsign=upper(:callsign) "
                            "AND upper(mode)=upper(:mode) "
                            "AND upper(band)=upper(:band) "
                            "AND COALESCE(sat_name, '') = COALESCE(:sat_name, '') "
                            "AND ABS(JULIANDAY(start_time)-JULIANDAY(datetime(:startdate)))*24*60<30") )
    {
        qWarning() << "cannot prepare Dup statement";
        return 0;
    }


    QSqlDatabase::database().transaction();

    QSqlTableModel model;
    model.setTable("contacts");
    model.removeColumn(model.fieldIndex("id"));
    QSqlRecord record = model.record();
    duplicateQSOBehaviour dupSetting = LogFormat::ASK_NEXT;

    auto setIfEmpty = [&](int column, const QString &value)
    {
        if ( record.value(RECORDIDX(column)).toString().isEmpty() && !value.isEmpty() )
            record.setValue(RECORDIDX(column), value);
    };

    auto setMyDefaultProfile = [&]()
    {
        setIfEmpty(LogbookModel::COLUMN_MY_DXCC, QString::number(defaultStationProfile->dxcc));
        setIfEmpty(LogbookModel::COLUMN_STATION_CALLSIGN, defaultStationProfile->callsign);
        setIfEmpty(LogbookModel::COLUMN_MY_GRIDSQUARE, defaultStationProfile->locator);
        setIfEmpty(LogbookModel::COLUMN_MY_NAME, defaultStationProfile->operatorName);
        setIfEmpty(LogbookModel::COLUMN_OPERATOR, defaultStationProfile->operatorCallsign);
        setIfEmpty(LogbookModel::COLUMN_MY_CITY_INTL, defaultStationProfile->qthName);
        setIfEmpty(LogbookModel::COLUMN_MY_CITY, Data::removeAccents(defaultStationProfile->qthName));
        setIfEmpty(LogbookModel::COLUMN_MY_IOTA, defaultStationProfile->iota);
        setIfEmpty(LogbookModel::COLUMN_MY_POTA_REF, defaultStationProfile->pota);
        setIfEmpty(LogbookModel::COLUMN_MY_SOTA_REF, defaultStationProfile->sota);
        setIfEmpty(LogbookModel::COLUMN_MY_SIG_INTL, defaultStationProfile->sig);
        setIfEmpty(LogbookModel::COLUMN_MY_SIG, Data::removeAccents(defaultStationProfile->sig));
        setIfEmpty(LogbookModel::COLUMN_MY_SIG_INFO_INTL, defaultStationProfile->sigInfo);
        setIfEmpty(LogbookModel::COLUMN_MY_SIG_INFO, Data::removeAccents(defaultStationProfile->sigInfo));
        setIfEmpty(LogbookModel::COLUMN_MY_VUCC_GRIDS, defaultStationProfile->vucc);
        setIfEmpty(LogbookModel::COLUMN_MY_ITU_ZONE, QString::number(defaultStationProfile->ituz));
        setIfEmpty(LogbookModel::COLUMN_MY_CQ_ZONE, QString::number(defaultStationProfile->cqz));
        setIfEmpty(LogbookModel::COLUMN_MY_COUNTRY_INTL, defaultStationProfile->country);
        setIfEmpty(LogbookModel::COLUMN_MY_COUNTRY, Data::removeAccents(defaultStationProfile->country));
        setIfEmpty(LogbookModel::COLUMN_MY_CNTY, Data::removeAccents(defaultStationProfile->county));
        setIfEmpty(LogbookModel::COLUMN_MY_DARC_DOK, Data::removeAccents(defaultStationProfile->darcDOK));
    };

    auto setMyEntity = [&](const DxccEntity &myEntity)
    {
        // force overwrite
        record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_DXCC), myEntity.dxcc);
        record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_COUNTRY), Data::removeAccents(myEntity.country));
        record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_COUNTRY_INTL), myEntity.country);

        // other DXCC related values ​​are not closely related to DXCC value and could have been filled
        // therefore check if it is present or not.
        setIfEmpty(LogbookModel::COLUMN_MY_ITU_ZONE, QString::number(myEntity.ituz));
        setIfEmpty(LogbookModel::COLUMN_MY_CQ_ZONE, QString::number(myEntity.cqz));
    };

    auto lookupAndSetMyEntityByCallsign = [&] (const QString& recordMyDXCC)
    {
        const DxccEntity &myEntity = Data::instance()->lookupDxcc(recordMyDXCC);

        if ( myEntity.dxcc == 0 )  // My DXCC not found
        {
            writeImportLog(importLogStream,
                           WARNING_SEVERITY,
                           errors,
                           warnings,
                           processedRec,
                           record,
                           tr("Cannot find My DXCC Entity Info"));
        }
        else
        {
            setMyEntity(myEntity);
        }
    };

    if ( !insertQuery.prepare( QSqlDatabase::database().driver()->sqlStatement(QSqlDriver::InsertStatement,
                                                                               "contacts",
                                                                               record,
                                                                               true)) )
    {
        qWarning() << "cannot prepare Insert statement" << insertQuery.lastError();
        return 0;
    }

    while (true)
    {
        record.clearValues();

        if (!this->importNext(record)) break;

        processedRec++;

        /* Compute the Band if missing
         *   Band is one of the mandatory fields
         */

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_BAND)).toString().isEmpty()
             && !record.value(RECORDIDX(LogbookModel::COLUMN_FREQUENCY)).toString().isEmpty() )
        {
            double freq = record.value(RECORDIDX(LogbookModel::COLUMN_FREQUENCY)).toDouble();
            record.setValue(RECORDIDX(LogbookModel::COLUMN_BAND), BandPlan::freq2Band(freq).name);
        }

        // needed later
        const QVariant &call = record.value(RECORDIDX(LogbookModel::COLUMN_CALL));
        const QVariant &mycall = record.value(RECORDIDX(LogbookModel::COLUMN_STATION_CALLSIGN));
        const QVariant &band = record.value(RECORDIDX(LogbookModel::COLUMN_BAND));
        const QVariant &mode = record.value(RECORDIDX(LogbookModel::COLUMN_MODE));
        const QDateTime &start_time = record.value(RECORDIDX(LogbookModel::COLUMN_TIME_ON)).toDateTime();
        const QVariant &sota = record.value(RECORDIDX(LogbookModel::COLUMN_SOTA_REF));
        const QVariant &mysota = record.value(RECORDIDX(LogbookModel::COLUMN_MY_SOTA_REF));
        const QVariant &satName = record.value(RECORDIDX(LogbookModel::COLUMN_SAT_NAME));

        /* checking matching fields if they are not empty */
        if ( !start_time.isValid()
             || call.toString().isEmpty()
             || band.toString().isEmpty()
             || mode.toString().isEmpty())
        {
            writeImportLog(importLogStream,
                           ERROR_SEVERITY,
                           errors,
                           warnings,
                           processedRec,
                           record,
                           tr("A minimal set of fields not present (start_time, call, band, mode, station_callsign)"));
            qWarning() << "Import does not contain minimal set of fields (start_time, call, band, mode, station_callsign)";
            qCDebug(runtime) << record;
            continue;
        }

        if ( processedRec % 1000 == 0)
        {
            emit importPosition(stream.pos());
        }

        if ( isDateRange() )
        {
            if (!inDateRange(start_time.date()))
            {
                writeImportLog(importLogStream,
                               WARNING_SEVERITY,
                               errors,
                               warnings,
                               processedRec,
                               record,
                               tr("Outside the selected Date Range"));
                continue;
            }
        }

        if ( dupSetting != ACCEPT_ALL )
        {
            dupQuery.bindValue(":callsign", call);
            dupQuery.bindValue(":mode", mode);
            dupQuery.bindValue(":band", band);
            dupQuery.bindValue(":startdate", start_time.toTimeZone(QTimeZone::utc()).toString("yyyy-MM-dd hh:mm:ss"));
            dupQuery.bindValue(":sat_name", satName);

            if ( !dupQuery.exec() )
            {
                qWarning() << "Cannot exect DUP statement";
            }

            if ( dupQuery.next() )
            {
                if ( dupSetting == SKIP_ALL)
                {
                    writeImportLog(importLogStream,
                                   WARNING_SEVERITY,
                                   errors,
                                   warnings,
                                   processedRec,
                                   record,
                                   tr("Duplicate"));
                    continue;
                }

                /* Duplicate QSO found */
                if ( duplicateQSOFunc )
                {
                    QSqlRecord dupRecord;
                    dupRecord= dupQuery.record();
                    dupSetting = duplicateQSOFunc(&record, &dupRecord);
                }

                switch ( dupSetting )
                {
                case ACCEPT_ALL:
                case ACCEPT_ONE:
                case ASK_NEXT:
                    break;

                case SKIP_ONE:
                case SKIP_ALL:
                    writeImportLog(importLogStream,
                                   WARNING_SEVERITY,
                                   errors,
                                   warnings,
                                   processedRec,
                                   record,
                                   tr("Duplicate"));
                    continue;
                    break;
                }
            }
        }

        /* Adding information which are important for QLog or QLog knows/compute them */
        /************************/
        /* Add DXCC Entity Info */
        /************************/

        int recordDXCCId = record.value(RECORDIDX(LogbookModel::COLUMN_DXCC)).toInt(); // 0 = NAN or not present
                                                                                       // otherwise = DXCC ID

        if ( recordDXCCId != 0 || updateDxcc )
        {
            const DxccEntity &entity = ( updateDxcc ) ? Data::instance()->lookupDxcc(call.toString())
                                                      : Data::instance()->lookupDxccID(recordDXCCId);

            if ( entity.dxcc == 0 )  // DXCC not found
            {
                writeImportLog(importLogStream,
                               (updateDxcc) ? ERROR_SEVERITY : WARNING_SEVERITY,
                               errors,
                               warnings,
                               processedRec,
                               record,
                               tr("Cannot find DXCC Entity Info"));
                if ( updateDxcc )
                    continue;
            }
            else
            {
                // force overwrite
                record.setValue(RECORDIDX(LogbookModel::COLUMN_DXCC), entity.dxcc);
                record.setValue(RECORDIDX(LogbookModel::COLUMN_COUNTRY), Data::removeAccents(entity.country));
                record.setValue(RECORDIDX(LogbookModel::COLUMN_COUNTRY_INTL), entity.country);

                // other DXCC related values ​​are not closely related to DXCC value and could have been filled
                // therefore check if it is present or not.
                setIfEmpty(LogbookModel::COLUMN_CONTINENT, entity.cont);
                setIfEmpty(LogbookModel::COLUMN_ITUZ, QString::number(entity.ituz));
                setIfEmpty(LogbookModel::COLUMN_CQZ, QString::number(entity.cqz));
            }
        }
        else
        {
            writeImportLog(importLogStream,
                           WARNING_SEVERITY,
                           errors,
                           warnings,
                           processedRec,
                           record,
                           tr("DXCC Info is missing"));
        }

        /************************/
        /* Add My Station Info  */
        /************************/

        int recordMyDXCCId = record.value(RECORDIDX(LogbookModel::COLUMN_MY_DXCC)).toInt(); // 0 = NAN or not present
                                                                                            // otherwise = DXCC ID
        const QString &myCallString = mycall.toString();

        if ( defaultStationProfile )
        {
            // default is enabled

            // Case 1: Both recordMyDXCCId and myCallString are empty
            if (  recordMyDXCCId == 0 && myCallString.isEmpty()  )
            {
                setMyDefaultProfile();
            }
            // Case 2: recordMyDXCCId is empty, myCallString is not
            else if ( recordMyDXCCId == 0 )
            {
                if ( defaultStationProfile->callsign == myCallString )
                    setMyDefaultProfile();
                else
                    lookupAndSetMyEntityByCallsign(myCallString);
            }
            // Case 3: myCallString is empty, recordMyDXCCId is not
            else if ( myCallString.isEmpty() )
            {
                if ( defaultStationProfile->dxcc == recordMyDXCCId )
                    setMyDefaultProfile();
                else
                {
                    // no Station Callsign = ERROR
                    writeImportLog(importLogStream,
                                   ERROR_SEVERITY,
                                   errors,
                                   warnings,
                                   processedRec,
                                   record,
                                   tr("no Station Callsign present"));
                    continue;
                }
            }
             // Case 4: Both recordMyDXCCId and myCallString are not empty
            else
            {
                if ( defaultStationProfile->callsign == myCallString )
                {
                    if ( defaultStationProfile->dxcc == recordMyDXCCId )
                        setMyDefaultProfile();
                    else   
                    {
                        // no Station Callsign = ERROR
                        writeImportLog(importLogStream,
                                       ERROR_SEVERITY,
                                       errors,
                                       warnings,
                                       processedRec,
                                       record,
                                       tr("no Station Callsign present"));
                        continue;
                    }
                }
                else
                    lookupAndSetMyEntityByCallsign(myCallString);
            }
        }
        else
        {
            // default is disabled
            if ( myCallString.isEmpty() )
            {
                // no Station Callsign = ERROR
                writeImportLog(importLogStream,
                               ERROR_SEVERITY,
                               errors,
                               warnings,
                               processedRec,
                               record,
                               tr("no Station Callsign present"));
                continue;
            }
            else
            {
                const DxccEntity &myEntity = ( recordMyDXCCId != 0 ) ? Data::instance()->lookupDxccID(recordMyDXCCId)
                                                                     : Data::instance()->lookupDxcc(myCallString);

                if ( myEntity.dxcc == 0 )  // My DXCC not found
                {
                    writeImportLog(importLogStream,
                                   WARNING_SEVERITY,
                                   errors,
                                   warnings,
                                   processedRec,
                                   record,
                                   tr("Cannot find My DXCC Entity Info"));
                }
                else
                    setMyEntity(myEntity);
            }
        }

        /***********/
        /* Add PFX */
        /***********/

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_PREFIX)).toString().isEmpty() )
        {
            const QString &pfxRef = Callsign(call.toString()).getWPXPrefix();

            if ( !pfxRef.isEmpty() )
            {
                record.setValue(RECORDIDX(LogbookModel::COLUMN_PREFIX), pfxRef);
            }
        }

        /********************/
        /* Compute Distance */
        /********************/
        const QString &gridsquare = record.value(RECORDIDX(LogbookModel::COLUMN_GRID)).toString();
        const QString &my_gridsquare = record.value(RECORDIDX(LogbookModel::COLUMN_MY_GRIDSQUARE)).toString();

        if ( !gridsquare.isEmpty()
             && !my_gridsquare.isEmpty()
             && record.value(RECORDIDX(LogbookModel::COLUMN_DISTANCE)).toString().isEmpty() )
        {
            const Gridsquare grid(gridsquare);
            const Gridsquare my_grid(my_gridsquare);
            double distance;

            if ( my_grid.distanceTo(grid, distance) )
            {
                record.setValue(RECORDIDX(LogbookModel::COLUMN_DISTANCE), distance);
            }
        }

        /*************************/
        /* Compute Alt from SOTA */
        /*************************/
        if ( record.value(RECORDIDX(LogbookModel::COLUMN_ALTITUDE)).toString().isEmpty()
             && !sota.toString().isEmpty() )
        {
            const SOTAEntity &sotaInfo = Data::instance()->lookupSOTA(sota.toString());
            if ( sotaInfo.summitCode.compare(sota.toString(), Qt::CaseInsensitive)
                 && !sotaInfo.summitName.isEmpty() )
            {
                record.setValue(RECORDIDX(LogbookModel::COLUMN_ALTITUDE),sotaInfo.altm);
            }
        }

        /*******************************/
        /* Compute My Alt from My SOTA */
        /*******************************/
        if ( record.value(RECORDIDX(LogbookModel::COLUMN_MY_ALTITUDE)).toString().isEmpty()
             && !mysota.toString().isEmpty() )
        {
            const SOTAEntity &sotaInfo = Data::instance()->lookupSOTA(mysota.toString());
            if ( sotaInfo.summitCode.compare(sota.toString(), Qt::CaseInsensitive)
                 && !sotaInfo.summitName.isEmpty() )
            {
                record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_ALTITUDE),sotaInfo.altm);
            }
        }

        /******************/
        /* PREPARE INSERT */
        /******************/
        // Bind all values
        for ( int i = 0; i < record.count(); i++ )
        {
            insertQuery.bindValue(i, record.value(i));
        }

        if ( ! insertQuery.exec() )
        {
            writeImportLog(importLogStream,
                           ERROR_SEVERITY,
                           errors,
                           warnings,
                           processedRec,
                           record,
                           tr("Cannot insert to database") + " - " + insertQuery.lastError().text());
            qWarning() << "Cannot insert a record to Contact Table - " << insertQuery.lastError();
            qCDebug(runtime) << record;
        }
        else
        {
            writeImportLog(importLogStream,
                           INFO_SEVERITY,
                           errors,
                           warnings,
                           processedRec,
                           record,
                           tr("Imported"));
            count++;
        }
    }

    emit importPosition(stream.pos());
    emit finished(count);

    QSqlDatabase::database().commit();

    this->importEnd();

    return count;
}

#undef RECORDIDX

void LogFormat::runQSLImport(QSLFrom fromService)
{
    FCT_IDENTIFICATION;

    auto reportFormatter = [&](const QDateTime &qsoTime,
                               const QString &callsign,
                               const QString &mode,
                               const QStringList addInfo = QStringList())
    {
        return QString("%0; %1; %2%3 %4").arg(qsoTime.isValid() ? qsoTime.toString(locale.formatDateShortWithYYYY()) : "-",
                                            callsign,
                                            mode,
                                            (addInfo.size() > 0 ) ? ";" : "",
                                            addInfo.join(", "));
    };

    static QRegularExpression reLeadingZero("^0+");
    QSLMergeStat stats = {QStringList(), QStringList(), QStringList(), QStringList(), 0};
    this->importStart();

    QSqlTableModel model;
    model.setTable("contacts");
    QSqlRecord QSLRecord = model.record(0);

    while ( true )
    {
        QSLRecord.clearValues();

        if ( !this->importNext(QSLRecord) ) break;

        stats.qsosDownloaded++;

        if ( stats.qsosDownloaded % 100 == 0 )
        {
            emit importPosition(stream.pos());
        }

        // needed later
        const QVariant &call = QSLRecord.value("callsign");
        const QVariant &band = QSLRecord.value("band");
        const QVariant &mode = QSLRecord.value("mode");
        const QVariant &start_time = QSLRecord.value("start_time");
        const QVariant &satName = QSLRecord.value("sat_name");

        /* checking matching fields if they are not empty */
        if ( !start_time.toDateTime().isValid()
             || call.toString().isEmpty()
             || band.toString().isEmpty()
             || mode.toString().isEmpty() )
        {
            qWarning() << "Import does not contain field start_time or callsign or band or mode ";
            qCDebug(runtime) << QSLRecord;
            stats.errorQSLs.append(reportFormatter(start_time.toDateTime(), call.toString(), mode.toString()));
            continue;
        }

        // It is important to use callsign index here
        QString matchFilter = QString("callsign=upper('%1') AND upper(mode)=upper('%2') AND upper(band)=upper('%3') AND COALESCE(sat_name, '') = upper('%4') AND ABS(JULIANDAY(start_time)-JULIANDAY(datetime('%5')))*24*60<30")
                .arg(call.toString(),
                     mode.toString(),
                     band.toString(),
                     satName.toString(),
                     start_time.toDateTime().toTimeZone(QTimeZone::utc()).toString("yyyy-MM-dd hh:mm:ss"));

        /* set filter */
        model.setFilter(matchFilter);
        model.select();

        if ( model.rowCount() != 1 )
        {
            stats.unmatchedQSLs.append(reportFormatter(start_time.toDateTime(), call.toString(), mode.toString()));
            continue;
        }

        /* we have one row for updating */
        /* lets update it */
        QSqlRecord originalRecord = model.record(0);

        switch ( fromService )
        {
        case LOTW:
        {
            /* https://lotw.arrl.org/lotw-help/developer-query-qsos-qsls/?lang=en */
            // always try to update contact from received QSL
            if ( QSLRecord.value("qsl_rcvd").toString() == 'Y' ) // qsl_rcvd is OK because LoTW sends lotw_qsl_rcvd value in qsl_rcvd
            {
                QStringList updatedFields;
                bool callUpdate = false;
                bool newlyReceived = (QSLRecord.value("qsl_rcvd").toString() != originalRecord.value("lotw_qsl_rcvd").toString());

                qCDebug(runtime) << "Attempt to" << (newlyReceived ? "force " : "") << "update QSO" << call.toString()
                                 << band.toString() << start_time.toString();

                auto conditionUpdate = [&](const QString &contactKey,
                                           const QString &qslKey,
                                           bool forceUpdate = false)
                {
                    if ( !QSLRecord.value(qslKey).toString().isEmpty()
                        && ( forceUpdate || originalRecord.value(contactKey).toString().isEmpty() ) )
                    {
                        qCDebug(runtime) << "Updating:" << contactKey
                                         << "to" << QSLRecord.value(qslKey).toString()
                                         << (forceUpdate ? "force update" : "");
                        updatedFields.append(contactKey + "(" + QSLRecord.value(qslKey).toString()  +")");
                        originalRecord.setValue(contactKey, QSLRecord.value(qslKey));
                        return true;
                    }
                    return false;
                };

                auto conditionUpdateSpecial = [&](const QString &contactKey,
                                                  const QString &qslKey,
                                                  bool forceUpdate = false)
                {
                    QString contactValue = originalRecord.value(contactKey).toString();
                    QString QSLValue = QSLRecord.value(qslKey).toString();
                    contactValue.remove(reLeadingZero);
                    QSLValue.remove(reLeadingZero);

                    if ( !QSLValue.isEmpty()
                        && ( forceUpdate || contactValue != QSLValue ) )
                    {
                        qCDebug(runtime) << "Updating:" << contactKey
                                         << "from" << originalRecord.value(contactKey).toString()
                                         << "to" << QSLValue
                                         << (forceUpdate ? "force update" : "");
                        updatedFields.append(contactKey + "(" + QSLRecord.value(qslKey).toString()  +")");
                        originalRecord.setValue(contactKey, QSLValue);
                        return true;
                    }
                    return false;
                };

                callUpdate |= conditionUpdate("lotw_qsl_rcvd", "qsl_rcvd", newlyReceived);
                callUpdate |= conditionUpdate("lotw_qslrdate", "qsl_rdate", newlyReceived);
                callUpdate |= conditionUpdate("credit_granted", "credit_granted", newlyReceived);
                callUpdate |= conditionUpdate("credit_submitted", "credit_submitted", newlyReceived);
                callUpdate |= conditionUpdate("pfx", "pfx", newlyReceived);
                callUpdate |= conditionUpdate("iota", "iota", newlyReceived);
                callUpdate |= conditionUpdate("vucc_grids", "vucc_grids", newlyReceived);
                callUpdate |= conditionUpdate("state", "state", newlyReceived);
                callUpdate |= conditionUpdate("cnty", "cnty", newlyReceived);
                callUpdate |= conditionUpdateSpecial("ituz", "ituz", newlyReceived);
                callUpdate |= conditionUpdateSpecial("cqz", "cqz", newlyReceived);

                if ( originalRecord.value("qsl_rcvd_via").toString() != "E" )
                {
                    qCDebug(runtime) << "Updating: qsl_rcvd_via from" << originalRecord.value("qsl_rcvd_via").toString() << "to E";
                    originalRecord.setValue("qsl_rcvd_via", "E");
                    updatedFields.append("qsl_rcvd_via (E)");
                    callUpdate |= true;
                }

                const QString origGrig = originalRecord.value("gridsquare").toString();
                const Gridsquare dxNewGrid(QSLRecord.value("gridsquare").toString());

                if ( ( newlyReceived
                       ||  origGrig.isEmpty()
                       || ( origGrig.length() < QSLRecord.value("gridsquare").toString().length()
                            && dxNewGrid.isValid()
                            && dxNewGrid.getGrid().contains(origGrig) ) )
                    && !dxNewGrid.getGrid().isEmpty() )
                {
                    const Gridsquare myGrid(originalRecord.value("my_gridsquare").toString());

                    originalRecord.setValue("gridsquare", dxNewGrid.getGrid());

                    double distance;

                    if ( myGrid.distanceTo(dxNewGrid, distance) )
                    {
                        originalRecord.setValue("distance", QVariant(distance));
                    }
                    qCDebug(runtime) << "Updating: grid from " << origGrig << "to" << dxNewGrid.getGrid();
                    updatedFields.append("gridsquare (" + dxNewGrid.getGrid()  +")");
                    callUpdate |= true;
                }

                if ( callUpdate )
                {
                    qCDebug(runtime) << "Calling update for" << call << band << mode << start_time << satName;
                    if ( !model.setRecord(0, originalRecord) )
                    {
                        qWarning() << "Cannot update a Contact record - " << model.lastError();
                        qCDebug(runtime) << originalRecord;
                    }

                    if ( !model.submitAll() )
                    {
                        qWarning() << "Cannot commit changes to Contact Table - " << model.lastError();
                    }
                    if ( newlyReceived )
                    {
                        const DxccStatus status = Data::instance()->dxccStatus(originalRecord.value("dxcc").toInt(), band.toString(), mode.toString());
                        stats.newQSLs.append(reportFormatter(start_time.toDateTime(), call.toString(), mode.toString(), {tr("DXCC State:") + " " + Data::statusToText(status)}));
                    }
                    else
                        stats.updatedQSOs.append(reportFormatter(start_time.toDateTime(), call.toString(), mode.toString(), updatedFields));
                }
            }
            break;
        }

        case EQSL:
        {
            /* http://www.eqsl.cc/qslcard/DownloadInBox.txt */
            /*   CALL
                 QSO_DATE
                 TIME_ON
                 BAND
                 MODE
                 SUBMODE (tag only present if non-blank)
                 PROP_MODE (tag only present if non-blank)
                 RST_SENT (will be the sender's RST Sent, not yours)
                 RST_RCVD (we do not capture this in uploads, so will normally be 0 length)
                 QSL_SENT (always Y)
                 QSL_SENT_VIA (always E)
                 QSLMSG (if non-null and containing only valid printable ASCII characters)
                 QSLMSG_INTL (if non-null and containing international characters - see ADIF V3 specs)
                 APP_EQSL_SWL (tag only present if sender is SWL and then always Y)
                 APP_EQSL_AG (tag only present if sender has Authenticity Guaranteed status and then always Y)
                 GRIDSQUARE (tag only present if non-blank and at least 4 long)
            */
            // LF: Since I consider this source unreliable, I will not update it here, as I do with LoTW
            // try to update contact from received QSL only in case when contact != Y
            if ( originalRecord.value("eqsl_qsl_rcvd").toString() != 'Y' )
            {
                originalRecord.setValue("eqsl_qsl_rcvd", QSLRecord.value("qsl_sent"));

                originalRecord.setValue("eqsl_qslrdate", QDateTime::currentDateTimeUtc().date().toString("yyyy-MM-dd"));

                Gridsquare dxNewGrid(QSLRecord.value("gridsquare").toString());

                if ( dxNewGrid.isValid()
                     && ( originalRecord.value("gridsquare").toString().isEmpty()
                          ||
                          dxNewGrid.getGrid().contains(originalRecord.value("gridsquare").toString()))
                     )
                {
                    Gridsquare myGrid(originalRecord.value("my_gridsquare").toString());

                    originalRecord.setValue("gridsquare", dxNewGrid.getGrid());

                    double distance;

                    if ( myGrid.distanceTo(dxNewGrid, distance) )
                    {
                        originalRecord.setValue("distance", QVariant(distance));
                    }
                }

                const QString &prevValue = originalRecord.value("qslmsg_rcvd").toString();
                const QString &newValue = QSLRecord.value("qslmsg").toString();

                if ( !newValue.isEmpty() )
                    originalRecord.setValue("qslmsg_rcvd", prevValue.isEmpty() ? "eQSL: " + newValue : prevValue + "| eQSL: " + newValue);

                // temporary removed - ADIF 3.1.5 has no INTL equivalent for qslmsg_rcvd
                //originalRecord.setValue("qslmsg_int", QSLRecord.value("qslmsg_int"));

                originalRecord.setValue("qsl_rcvd_via", QSLRecord.value("qsl_sent_via"));

                if ( !model.setRecord(0, originalRecord) )
                {
                    qWarning() << "Cannot update a Contact record - " << model.lastError();
                    qCDebug(runtime) << originalRecord;
                }

                if ( !model.submitAll() )
                {
                    qWarning() << "Cannot commit changes to Contact Table - " << model.lastError();
                }
                const DxccStatus status = Data::instance()->dxccStatus(originalRecord.value("dxcc").toInt(), band.toString(), mode.toString());
                stats.newQSLs.append(reportFormatter(start_time.toDateTime(), call.toString(), mode.toString(), {tr("DXCC State:") + " " + Data::statusToText(status)}));
            }

            break;
        }

        default:
            qCDebug(runtime) << "Uknown QSL import";
        }
    }

    emit importPosition(stream.pos());

    this->importEnd();

    emit QSLMergeFinished(stats);
}

long LogFormat::runExport()
{
    FCT_IDENTIFICATION;

    this->exportStart();

    QSqlQuery query;

    QString queryStmt = QString("SELECT %1 FROM contacts WHERE %2 ORDER BY start_time ASC").arg(exportedFields.join(", "), getWhereClause());

    qCDebug(runtime) << queryStmt;

    if ( ! query.prepare(queryStmt) )
    {
        qWarning() << "Cannot prepare select statement";
        return 0;
    }

    bindWhereClause(query);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute select statement" << query.lastError();
        return 0;
    }

    long count = 0L;

    /* following 3 lines are a workaround - SQLite does not
     * return a correct value for QSqlQuery.size
     */
    int rows = (query.last()) ? query.at() + 1 : 0;
    query.first();
    query.previous();

    while (query.next())
    {
        this->exportContact(query.record());
        count++;
        if (count % 100 == 0)
        {
            emit exportProgress((int)(count * 100 / rows));
        }
    }

    emit exportProgress(100);

    this->exportEnd();
    return count;
}

long LogFormat::runExport(const QList<QSqlRecord> &selectedQSOs)
{
    FCT_IDENTIFICATION;

    this->exportStart();

    long count = 0L;
    for (const QSqlRecord &qso: selectedQSOs)
    {
        QSqlRecord contactRecord;

        if ( exportedFields.first() != "*" )
        {
            for ( const QString& fieldName : static_cast<const QStringList&>(exportedFields) )
            {
                contactRecord.append(qso.field(fieldName));
            }
        }
        else
        {
            contactRecord = qso;
        }
        this->exportContact(contactRecord);
        count++;
        if ( count % 10 == 0 )
        {
            emit exportProgress((int)(count * 100 / selectedQSOs.size()));
        }
    }

    emit exportProgress(100);
    emit finished(count);
    this->exportEnd();
    return count;
}

bool LogFormat::isDateRange() {
    FCT_IDENTIFICATION;

    return !filterStartDate.isNull() && !filterEndDate.isNull();
}

bool LogFormat::inDateRange(QDate date) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<date;

    return date >= filterStartDate && date <= filterEndDate;
}

QString LogFormat::importLogSeverityToString(ImportLogSeverity severity)
{
    switch ( severity )
    {
    case ERROR_SEVERITY:
        return tr("Error") + " - ";
        break;
    case WARNING_SEVERITY:
        return tr("Warning") + " - ";
        break;
    case INFO_SEVERITY:
    default: //NOTHING
        ;
    }

    return QString();
}

void LogFormat::writeImportLog(QTextStream &errorLogStream, ImportLogSeverity severity, const QString &msg)
{
    FCT_IDENTIFICATION;

    errorLogStream << importLogSeverityToString(severity) << msg << "\n";
}

void LogFormat::writeImportLog(QTextStream& errorLogStream,
                               ImportLogSeverity severity,
                               unsigned long *error,
                               unsigned long *warning,
                               const unsigned long recordNo,
                               const QSqlRecord &record,
                               const QString &msg)
{
    FCT_IDENTIFICATION;

    errorLogStream << QString("[QSO#%1]: ").arg(recordNo)
                   << importLogSeverityToString(severity)
                   << msg
                   << QString(" (%1; %2; %3)").arg(record.value("start_time").toDateTime().toTimeZone(QTimeZone::utc()).toString(locale.formatDateShortWithYYYY()),
                                                   record.value("callsign").toString(),
                                                   record.value("mode").toString())
                   << "\n";
    switch (severity)
    {
    case WARNING_SEVERITY: (*warning)++; break;
    case ERROR_SEVERITY: (*error)++; break;
    case INFO_SEVERITY: break;
    }
}
