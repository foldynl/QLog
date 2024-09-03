#include <QtSql>
#include <QSqlDriver>
#include "LogFormat.h"
#include "AdiFormat.h"
#include "AdxFormat.h"
#include "JsonFormat.h"
#include "CSVFormat.h"
#include "data/Data.h"
#include "core/debug.h"
#include "core/Gridsquare.h"
#include "core/Callsign.h"
#include "data/BandPlan.h"
#include "models/LogbookModel.h"

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

QString LogFormat::getWhereClause()
{
    FCT_IDENTIFICATION;

    whereClause.clear();

    whereClause << "1 = 1"; //generic filter

    if ( isDateRange() )
    {
        whereClause << "(start_time BETWEEN :start_date AND :end_date)";
    }

    if ( !filterMyCallsign.isEmpty() )
    {
        whereClause << "upper(station_callsign) = upper(:stationCallsign)";
    }

    if ( !filterMyGridsquare.isEmpty() )
    {
        whereClause << "upper(my_gridsquare) = upper(:myGridsquare)";
    }

    if ( !filterSentPaperQSL.isEmpty() )
    {
        whereClause << QString("upper(qsl_sent) in  (%1)").arg(filterSentPaperQSL.join(", "));
    }

    if ( !filterSendVia.isEmpty() )
    {
        whereClause << ( ( filterSendVia == " " ) ? "qsl_sent_via is NULL"
                                                  : "upper(qsl_sent_via) = upper(:qsl_sent_via)");
    }

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
;
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


        /* checking matching fields if they are not empty */
        if ( !start_time.isValid()
             || call.toString().isEmpty()
             || band.toString().isEmpty()
             || mode.toString().isEmpty()
             || mycall.toString().isEmpty() )
        {
            writeImportLog(importLogStream,
                           ERROR_SEVERITY,
                           processedRec,
                           record,
                           tr("A minimal set of fields not present (start_time, call, band, mode, station_callsign)"));
            qWarning() << "Import does not contain minimal set of fields (start_time, call, band, mode, station_callsign)";
            qCDebug(runtime) << record;
            (*errors)++;
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
                               processedRec,
                               record,
                               tr("Outside the selected Date Range"));
                (*warnings)++;
                continue;
            }
        }

        if ( dupSetting != ACCEPT_ALL )
        {
            dupQuery.bindValue(":callsign", call);
            dupQuery.bindValue(":mode", mode);
            dupQuery.bindValue(":band", band);
            dupQuery.bindValue(":startdate", start_time.toTimeSpec(Qt::UTC).toString("yyyy-MM-dd hh:mm:ss"));

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
                                   processedRec,
                                   record,
                                   tr("Duplicate"));
                    (*warnings)++;
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
                                   processedRec,
                                   record,
                                   tr("Duplicate"));
                    (*warnings)++;
                    continue;
                    break;
                }
            }
        }

        const DxccEntity &entity = Data::instance()->lookupDxcc(call.toString());

        if ( entity.dxcc == 0 )
        {
            writeImportLog(importLogStream,
                           ERROR_SEVERITY,
                           processedRec,
                           record,
                           tr("Cannot find DXCC Entity Info"));
            (*errors)++;
            continue;
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_DXCC)).toString().isEmpty()
             || updateDxcc )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_DXCC), entity.dxcc);
            record.setValue(RECORDIDX(LogbookModel::COLUMN_COUNTRY), Data::removeAccents(entity.country));
            record.setValue(RECORDIDX(LogbookModel::COLUMN_COUNTRY_INTL), entity.country);
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_CONTINENT)).toString().isEmpty()
             || updateDxcc )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_CONTINENT), entity.cont);
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_ITUZ)).toString().isEmpty()
             || updateDxcc )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_ITUZ), QString::number(entity.ituz));
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_CQZ)).toString().isEmpty()
             || updateDxcc )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_CQZ), QString::number(entity.cqz));
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_PREFIX)).toString().isEmpty() )
        {
            const QString &pfxRef = Callsign(call.toString()).getWPXPrefix();

            if ( !pfxRef.isEmpty() )
            {
                record.setValue(RECORDIDX(LogbookModel::COLUMN_PREFIX), pfxRef);
            }
        }

        const QString &gridsquare = record.value(RECORDIDX(LogbookModel::COLUMN_GRID)).toString();
        const QString &my_gridsquare = record.value(RECORDIDX(LogbookModel::COLUMN_MY_GRIDSQUARE)).toString();

        if ( !gridsquare.isEmpty()
             && !my_gridsquare.isEmpty()
             && record.value(RECORDIDX(LogbookModel::COLUMN_DISTANCE)).toString().isEmpty() )
        {
            Gridsquare grid(gridsquare);
            Gridsquare my_grid(my_gridsquare);
            double distance;

            if ( my_grid.distanceTo(grid, distance) )
            {
                record.setValue(RECORDIDX(LogbookModel::COLUMN_DISTANCE), distance);
            }
        }


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

        const DxccEntity &dxccEntity = Data::instance()->lookupDxcc(mycall.toString());

        if ( dxccEntity.dxcc == 0 )
        {
            writeImportLog(importLogStream,
                           ERROR_SEVERITY,
                           processedRec,
                           record,
                           tr("Cannot find own DXCC Entity Info"));
            (*errors)++;
            continue;
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_MY_DXCC)).toString().isEmpty() )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_DXCC), dxccEntity.dxcc);
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_MY_ITU_ZONE)).toString().isEmpty() )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_ITU_ZONE), dxccEntity.ituz);
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_MY_CQ_ZONE)).toString().isEmpty() )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_CQ_ZONE), dxccEntity.cqz);
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_MY_COUNTRY_INTL)).toString().isEmpty() )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_COUNTRY_INTL), dxccEntity.country);
        }

        if ( record.value(RECORDIDX(LogbookModel::COLUMN_MY_COUNTRY)).toString().isEmpty() )
        {
            record.setValue(RECORDIDX(LogbookModel::COLUMN_MY_COUNTRY), Data::removeAccents(dxccEntity.country));
        }

        // Bind all values
        for ( int i = 0; i < record.count(); i++ )
        {
            insertQuery.bindValue(i, record.value(i));
        }

        if ( ! insertQuery.exec() )
        {
            writeImportLog(importLogStream,
                           ERROR_SEVERITY,
                           processedRec,
                           record,
                           tr("Cannot insert to database") + " - " + insertQuery.lastError().text());
            qWarning() << "Cannot insert a record to Contact Table - " << insertQuery.lastError();
            qCDebug(runtime) << record;
            (*errors)++;
        }
        else
        {
            writeImportLog(importLogStream,
                           INFO_SEVERITY,
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

    QSLMergeStat stats = {QStringList(), QStringList(), 0, 0, 0, 0};

    this->importStart();

    QSqlTableModel model;
    model.setTable("contacts");
    QSqlRecord QSLRecord = model.record(0);

    while ( true )
    {
        QSLRecord.clearValues();

        if ( !this->importNext(QSLRecord) ) break;

        stats.qsos_checked++;

        if ( stats.qsos_checked % 100 == 0 )
        {
            emit importPosition(stream.pos());
        }

        // needed later
        const QVariant &call = QSLRecord.value("callsign");
        const QVariant &band = QSLRecord.value("band");
        const QVariant &mode = QSLRecord.value("mode");
        const QVariant &start_time = QSLRecord.value("start_time");

        /* checking matching fields if they are not empty */
        if ( !start_time.toDateTime().isValid()
             || call.toString().isEmpty()
             || band.toString().isEmpty()
             || mode.toString().isEmpty() )
        {
            qWarning() << "Import does not contain field start_time or callsign or band or mode ";
            qCDebug(runtime) << QSLRecord;
            stats.qsos_errors++;
            continue;
        }

        // It is important to use callsign index here
        QString matchFilter = QString("callsign=upper('%1') AND upper(mode)=upper('%2') AND upper(band)=upper('%3') AND ABS(JULIANDAY(start_time)-JULIANDAY(datetime('%4')))*24*60<30")
                .arg(call.toString(),
                     mode.toString(),
                     band.toString(),
                     start_time.toDateTime().toTimeSpec(Qt::UTC).toString("yyyy-MM-dd hh:mm:ss"));

        /* set filter */
        model.setFilter(matchFilter);
        model.select();

        if ( model.rowCount() != 1 )
        {
            stats.qsos_unmatched++;
            stats.unmatchedQSLs.append(call.toString());
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
            if ( !QSLRecord.value("lotw_qsl_rcvd").toString().isEmpty() )
            {
                if ( QSLRecord.value("qsl_rcvd") != originalRecord.value("lotw_qsl_rcvd")
                     && QSLRecord.value("qsl_rcvd").toString() == 'Y' )
                {
                    DxccStatus status = Data::dxccStatus(QSLRecord.value("dxcc").toInt(), band.toString(), mode.toString());
                    QString DxccStatusStr = "";
                    if (status == DxccStatus::NewEntity )
                    {
                        DxccStatusStr = "(New Entity)";
                    }
                    if (status == DxccStatus::NewMode )
                    {
                        DxccStatusStr = "(New Mode " + mode.toString() +")";
                    }
                    if (status == DxccStatus::NewBandMode )
                    {
                        DxccStatusStr = "(New Band/Mode " + band.toString() + " " + mode.toString() + ")";
                    }
                    if (status == DxccStatus::NewSlot )
                    {
                        DxccStatusStr = "(New Slot " + band.toString() + " " + mode.toString() + ")";
                    }

                    originalRecord.setValue("lotw_qsl_rcvd", QSLRecord.value("qsl_rcvd"));

                    originalRecord.setValue("lotw_qslrdate", QSLRecord.value("qsl_rdate"));

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

                    if ( !QSLRecord.value("credit_granted").toString().isEmpty() )
                    {
                        originalRecord.setValue("credit_granted", QSLRecord.value("credit_granted"));
                    }

                    if ( !QSLRecord.value("credit_submitted").toString().isEmpty() )
                    {
                        originalRecord.setValue("credit_submitted", QSLRecord.value("credit_submitted"));
                    }

                    if ( !QSLRecord.value("pfx").toString().isEmpty() )
                    {
                        originalRecord.setValue("pfx", QSLRecord.value("pfx"));
                    }

                    if ( !QSLRecord.value("iota").toString().isEmpty() )
                    {
                        originalRecord.setValue("iota", QSLRecord.value("iota"));
                    }

                    if ( !QSLRecord.value("vucc_grids").toString().isEmpty() )
                    {
                        originalRecord.setValue("vucc_grids", QSLRecord.value("vucc_grids"));
                    }

                    if ( !QSLRecord.value("state").toString().isEmpty() )
                    {
                        originalRecord.setValue("state", QSLRecord.value("state"));
                    }

                    if ( !QSLRecord.value("cnty").toString().isEmpty() )
                    {
                        originalRecord.setValue("cnty", QSLRecord.value("cnty"));
                    }

                    originalRecord.setValue("qsl_rcvd_via", "E");

                    if ( !model.setRecord(0, originalRecord) )
                    {
                        qWarning() << "Cannot update a Contact record - " << model.lastError();
                        qCDebug(runtime) << originalRecord;
                    }

                    if ( !model.submitAll() )
                    {
                        qWarning() << "Cannot commit changes to Contact Table - " << model.lastError();
                    }

                    stats.qsos_updated++;
                    stats.newQSLs.append(call.toString() + " " + DxccStatusStr);
                }
            }
            else
            {
                qCInfo(runtime) << "Malformed Lotw Record " << QSLRecord;
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

                originalRecord.setValue("qslmsg", QSLRecord.value("qslmsg"));

                originalRecord.setValue("qslmsg_int", QSLRecord.value("qslmsg_int"));

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
                stats.qsos_updated++;
                stats.newQSLs.append(call.toString());
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
            for ( const QString& fieldName : qAsConst(exportedFields) )
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

void LogFormat::writeImportLog(QTextStream& errorLogStream, ImportLogSeverity severity,
                               const unsigned long recordNo,
                               const QSqlRecord &record,
                               const QString &msg)
{
    FCT_IDENTIFICATION;

    errorLogStream << QString("[QSO#%1]: ").arg(recordNo)
                   << importLogSeverityToString(severity)
                   << msg
                   << QString(" (%1; %2; %3)").arg(record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateShortWithYYYY()),
                                                   record.value("callsign").toString(),
                                                   record.value("mode").toString())
                   << "\n";
}

