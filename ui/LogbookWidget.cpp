#include <QtSql>
#include <QMessageBox>
#include <QDesktopServices>
#include <QMenu>
#include <QProgressDialog>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QShortcut>
#include <QEvent>
#include <QKeyEvent>
#include <QProgressDialog>
#include <QActionGroup>

#include "logformat/AdiFormat.h"
#include "models/LogbookModel.h"
#include "models/SqlListModel.h"
#include "service/clublog/ClubLog.h"
#include "LogbookWidget.h"
#include "ui_LogbookWidget.h"
#include "ui/component/StyleItemDelegate.h"
#include "core/debug.h"
#include "models/SqlListModel.h"
#include "ui/ColumnSettingDialog.h"
#include "data/Data.h"
#include "ui/ExportDialog.h"
#include "service/eqsl/Eqsl.h"
#include "ui/PaperQSLDialog.h"
#include "ui/QSODetailDialog.h"
#include "core/MembershipQE.h"
#include "service/GenericCallbook.h"
#include "core/QSOFilterManager.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.logbookwidget");

LogbookWidget::LogbookWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LogbookWidget),
    blockClublogSignals(false),
    lookupDialog(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    searchTypeList.insert(CALLSIGN_SEARCH,
                          SearchDefinition(CALLSIGN_SEARCH,
                                           ui->actionSearchCallsign,
                                           "callsign"));
    searchTypeList.insert(GRIDSQUARE_SEARCH,
                          SearchDefinition(GRIDSQUARE_SEARCH,
                                           ui->actionSearchGrid,
                                           "gridsquare"));

    searchTypeList.insert(POTA_SEARCH,
                          SearchDefinition(POTA_SEARCH,
                                           ui->actionSearchPOTA,
                                           "pota_ref"));

    searchTypeList.insert(SOTA_SEARCH,
                          SearchDefinition(SOTA_SEARCH,
                                           ui->actionSearchSOTA,
                                           "sota_ref"));

    searchTypeList.insert(WWFF_SEARCH,
                          SearchDefinition(WWFF_SEARCH,
                                           ui->actionSearchWWFF,
                                           "wwff_ref"));

    searchTypeList.insert(SIG_SEARCH,
                          SearchDefinition(SIG_SEARCH,
                                           ui->actionSearchSIG,
                                           "sig_intl"));

    searchTypeList.insert(IOTA_SEARCH,
                          SearchDefinition(IOTA_SEARCH,
                                           ui->actionSearchIOTA,
                                           "iota"));

    setupSearchMenu();

    connect(ui->countrySelectFilter, &SmartSearchBox::currentTextChanged,
            this, &LogbookWidget::countryFilterChanged);

    connect(ui->bandSelectFilter, &SmartSearchBox::currentTextChanged,
            this, &LogbookWidget::bandFilterChanged);

    connect(ui->modeSelectFilter, &SmartSearchBox::currentTextChanged,
            this, &LogbookWidget::modeFilterChanged);

    connect(ui->userSelectFilter, &SmartSearchBox::currentTextChanged,
            this, &LogbookWidget::userFilterChanged);

    connect(ui->clubSelectFilter, &SmartSearchBox::currentTextChanged,
            this, &LogbookWidget::clubFilterChanged);

    model = new LogbookModel(this);
    connect(model, &LogbookModel::beforeUpdate, this, &LogbookWidget::handleBeforeUpdate);
    connect(model, &LogbookModel::beforeDelete, this, &LogbookWidget::handleBeforeDelete);
    connect(ui->contactTable, &QTableQSOView::dataCommitted, this, [this](){emit logbookUpdated();});

    /* Callbook Signals registration */
    connect(&callbookManager, &CallbookManager::callsignResult,
            this, &LogbookWidget::callsignFound);

    connect(&callbookManager, &CallbookManager::callsignNotFound,
            this, &LogbookWidget::callsignNotFound);

    connect(&callbookManager, &CallbookManager::loginFailed,
            this, &LogbookWidget::callbookLoginFailed);

    connect(&callbookManager, &CallbookManager::lookupError,
            this, &LogbookWidget::callbookError);

    ui->contactTable->setModel(model);

    QAction *separator = new QAction(ui->contactTable);
    separator->setSeparator(true);

    QAction *separator1 = new QAction(ui->contactTable);
    separator1->setSeparator(true);

    QAction *separator2 = new QAction(ui->contactTable);
    separator2->setSeparator(true);

    ui->contactTable->addAction(ui->actionEditContact);
    ui->contactTable->addAction(ui->actionFilter);
    ui->contactTable->addAction(ui->actionLookup);
    ui->contactTable->addAction(ui->actionSendDXCSpot);
    ui->contactTable->addAction(separator);
    ui->contactTable->addAction(ui->actionExportAs);
    ui->contactTable->addAction(ui->actionCallbookLookup);
    ui->contactTable->addAction(separator1);
    ui->contactTable->addAction(ui->actionDisplayedColumns);
    ui->contactTable->addAction(separator2);
    ui->contactTable->addAction(ui->actionDeleteContact);

    ui->contactTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->contactTable->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &LogbookWidget::showTableHeaderContextMenu);

    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TIME_ON, new TimestampFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TIME_OFF, new TimestampFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CALL, new CallsignDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FREQUENCY, new UnitFormatDelegate("", 6, 0.001, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_BAND, new ComboFormatDelegate(new SqlListModel("SELECT name FROM bands ORDER BY start_freq", " ", ui->contactTable), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MODE, new ComboFormatDelegate(new SqlListModel("SELECT name FROM modes", " ", ui->contactTable), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CONTINENT, new ComboFormatDelegate(QStringList() << " " << Data::getContinentList(), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT_VIA, new ComboFormatDelegate(Data::instance()->qslSentViaEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD_VIA, new ComboFormatDelegate(Data::instance()->qslSentViaEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_RCVD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_SENT_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TX_POWER, new UnitFormatDelegate("W", 3, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_AGE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ALTITUDE, new UnitFormatDelegate("m", 2, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_A_INDEX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_AZ, new UnitFormatDelegate("", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_EL, new UnitFormatDelegate("", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_PATH, new ComboFormatDelegate(Data::instance()->antPathEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DCL_QSLRDATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DCL_QSLSDATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DCL_QSL_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DCL_QSL_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DISTANCE, new DistanceFormatDelegate(1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_AG, new ComboFormatDelegate(Data::instance()->eqslAgEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSLRDATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSLSDATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSL_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSL_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FISTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FISTS_CC, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FORCE_INIT, new ComboFormatDelegate(Data::instance()->boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FREQ_RX, new UnitFormatDelegate("", 6, 0.001, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_BAND_RX, new ComboFormatDelegate(new SqlListModel("SELECT name FROM bands ORDER BY start_freq", " ", ui->contactTable), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_IOTA_ISLAND_ID, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_K_INDEX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MORSE_KEY_TYPE, new ComboFormatDelegate(Data::instance()->morseKeyTypeEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MAX_BURSTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_ALTITUDE, new UnitFormatDelegate("m", 2, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_CQ_ZONE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_DXCC, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_FISTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_IOTA_ISLAND_ID, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_ITU_ZONE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_MORSE_KEY_TYPE, new ComboFormatDelegate(Data::instance()->morseKeyTypeEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NR_BURSTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NR_PINGS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NOTES_INTL, new TextBoxDelegate(this));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NOTES, new TextBoxDelegate(this));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_PROP_MODE, new ComboFormatDelegate(Data::instance()->propagationModesIDList(), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_DOWNLOAD_DATE, new DateFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_DOWNLOAD_STATUS, new ComboFormatDelegate(Data::instance()->downloadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSO_COMPLETE, new ComboFormatDelegate(Data::instance()->qsoCompleteEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSO_RANDOM, new ComboFormatDelegate(Data::instance()->boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_RX_PWR, new UnitFormatDelegate("W", 3, 0.1, ui->contactTable));
    /*https://www.pe0sat.vgnet.nl/satellite/sat-information/modes/ */
    /* use all possible values, do not use only modern modes in sat_modes.json */
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SAT_MODE, new ComboFormatDelegate(QStringList()<<" "<<"VU"<<"VV"<<"UV"<<"UU"<<"US"<<"LU"<<"LS"<<"LX"<<"VS"<<"SX"<<"K"<<"T"<<"A"<<"J"<<"B"<<"S"<<"L", ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SFI, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SILENT_KEY, new ComboFormatDelegate(Data::instance()->boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SRX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_STX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SWL, new ComboFormatDelegate(Data::instance()->boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TEN_TEN, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_UKSMG, new UnitFormatDelegate("", 0, 1, ui->contactTable));

    const QByteArray &logbookState = LogParam::getLogbookState();
    if ( !logbookState.isEmpty() )
        ui->contactTable->horizontalHeader()->restoreState(logbookState);
    else
    {
        /* Hide all */
        for ( int i = 0; i < LogbookModel::COLUMN_LAST_ELEMENT; i++ )
        {
            ui->contactTable->hideColumn(i);
        }
        /* Set a basic set of columns */
        ui->contactTable->showColumn(LogbookModel::COLUMN_TIME_ON);
        ui->contactTable->showColumn(LogbookModel::COLUMN_CALL);
        ui->contactTable->showColumn(LogbookModel::COLUMN_RST_RCVD);
        ui->contactTable->showColumn(LogbookModel::COLUMN_RST_SENT);
        ui->contactTable->showColumn(LogbookModel::COLUMN_FREQUENCY);
        ui->contactTable->showColumn(LogbookModel::COLUMN_MODE);
        ui->contactTable->showColumn(LogbookModel::COLUMN_NAME_INTL);
        ui->contactTable->showColumn(LogbookModel::COLUMN_QTH_INTL);
        ui->contactTable->showColumn(LogbookModel::COLUMN_COMMENT_INTL);
    }
    ui->contactTable->setSortingEnabled(true);
    ui->contactTable->horizontalHeader()->setSectionsMovable(true);
    ui->contactTable->setStyle(new ProxyStyle(ui->contactTable->style()));
    ui->contactTable->installEventFilter(this);
    setDefaultSort();

    ui->bandSelectFilter->blockSignals(true);
    ui->bandSelectFilter->setModel(new SqlListModel("SELECT name FROM bands ORDER BY start_freq",
                                                    tr("All Bands"),
                                                    ui->bandSelectFilter));
    ui->bandSelectFilter->adjustMaxSize();
    ui->bandSelectFilter->setHighlightWhenEnable(true);
    ui->bandSelectFilter->blockSignals(false);

    ui->modeSelectFilter->blockSignals(true);
    ui->modeSelectFilter->setModel(new SqlListModel("SELECT name FROM modes",
                                                    tr("All Modes"),
                                                    ui->modeSelectFilter));
    ui->modeSelectFilter->adjustMaxSize();
    ui->modeSelectFilter->setHighlightWhenEnable(true);
    ui->modeSelectFilter->blockSignals(false);

    ui->countrySelectFilter->blockSignals(true);
    ui->countrySelectFilter->setModel(new SqlListModel("SELECT id, translate_to_locale(name) "
                                                       "FROM dxcc_entities WHERE id IN (SELECT DISTINCT dxcc FROM contacts) "
                                                       "ORDER BY 2 COLLATE LOCALEAWARE ASC;",
                                                       tr("All Countries"),
                                                       ui->countrySelectFilter));
    ui->countrySelectFilter->setModelColumn(1);
    ui->countrySelectFilter->adjustMaxSize();
    ui->countrySelectFilter->setHighlightWhenEnable(true);
    ui->countrySelectFilter->blockSignals(false);

    ui->clubSelectFilter->setHighlightWhenEnable(true);
    refreshClubFilter();

    ui->userSelectFilter->blockSignals(true);
    ui->userSelectFilter->setModel(new SqlListModel("SELECT filter_name "
                                                    "FROM qso_filters "
                                                    "ORDER BY filter_name COLLATE LOCALEAWARE ASC",
                                                    tr("No User Filter"),
                                                    ui->userSelectFilter));
    ui->userSelectFilter->adjustMaxSize();
    ui->userSelectFilter->setHighlightWhenEnable(true);
    ui->userSelectFilter->blockSignals(false);

    clublog = new ClubLogUploader(this);

    restoreFilters();
}

void LogbookWidget::filterSelectedCallsign()
{
    FCT_IDENTIFICATION;

    const QModelIndexList &modeList = ui->contactTable->selectionModel()->selectedRows();
    if ( modeList.count() > 0 )
    {
        const QSqlRecord &record = model->record(modeList.first().row());
        filterCallsign(record.value("callsign").toString());
    }
}

void LogbookWidget::filterCountryBand(const QString &countryName,
                                      const QString &band,
                                      const QString &addlFilter)
{
    FCT_IDENTIFICATION;

    ui->countrySelectFilter->blockSignals(true);
    ui->bandSelectFilter->blockSignals(true);
    ui->userSelectFilter->blockSignals(true);
    ui->modeSelectFilter->blockSignals(true);
    ui->clubSelectFilter->blockSignals(true);

    ui->countrySelectFilter->setCurrentText(countryName);

    ui->bandSelectFilter->setCurrentText(band);

    //user wants to see only selected band and country
    ui->userSelectFilter->setCurrentText(""); //suppress user-defined filter
    ui->modeSelectFilter->setCurrentText(""); //suppress mode filter
    ui->clubSelectFilter->setCurrentText(""); //suppress club filter

    // set additional filter
    externalFilter = addlFilter;

    ui->clubSelectFilter->blockSignals(false);
    ui->userSelectFilter->blockSignals(false);
    ui->modeSelectFilter->blockSignals(false);
    ui->countrySelectFilter->blockSignals(false);
    ui->bandSelectFilter->blockSignals(false);

    filterTable();
}

void LogbookWidget::lookupSelectedCallsign()
{
    FCT_IDENTIFICATION;

    const QModelIndexList &modeList = ui->contactTable->selectionModel()->selectedRows();
    if ( modeList.count() > 0)
    {
        const QSqlRecord &record = model->record(modeList.first().row());
        QDesktopServices::openUrl(GenericCallbook::getWebLookupURL(record.value("callsign").toString()));
    }
}

void LogbookWidget::actionCallbookLookup()
{
    FCT_IDENTIFICATION;

    callbookLookupBatch = ui->contactTable->selectionModel()->selectedRows();
    ui->contactTable->clearSelection();

    if ( callbookLookupBatch.count() > 100 )
    {
        callbookLookupBatch.clear();
        QMessageBox::warning(this, tr("QLog Warning"), tr("Each batch supports up to 100 QSOs."));
        return;
    }

    lookupDialog = new QProgressDialog(tr("QSOs Update Progress"),
                                       tr("Cancel"),
                                       0, callbookLookupBatch.count(),
                                       this);

    connect(lookupDialog, &QProgressDialog::canceled, this, [this]()
    {
        qCDebug(runtime)<< "Operation canceled";
        callbookManager.abortQuery();
        finishQSOLookupBatch();
    });

    lookupDialog->setValue(0);
    lookupDialog->setWindowModality(Qt::WindowModal);
    lookupDialog->show();

    queryNextQSOLookupBatch();
}

void LogbookWidget::queryNextQSOLookupBatch()
{
    FCT_IDENTIFICATION;

    if ( callbookLookupBatch.isEmpty() )
    {
        finishQSOLookupBatch();
        return;
    }

    currLookupIndex = callbookLookupBatch.takeFirst();
    callbookManager.queryCallsign(model->data(model->index(currLookupIndex.row(), LogbookModel::COLUMN_CALL), Qt::DisplayRole).toString());
}

void LogbookWidget::finishQSOLookupBatch()
{
    FCT_IDENTIFICATION;

    callbookLookupBatch.clear();
    currLookupIndex = QModelIndex();
    if ( lookupDialog )
    {
        lookupDialog->done(QDialog::Accepted);
        lookupDialog->deleteLater();
        lookupDialog = nullptr;
    }
}

void LogbookWidget::updateQSORecordFromCallbook(const CallbookResponseData& data)
{
    FCT_IDENTIFICATION;

    auto getCurrIndexColumnValue = [&](const LogbookModel::ColumnID id)
    {
        return model->data(model->index(currLookupIndex.row(), id), Qt::EditRole).toString();
    };

    auto setModelData = [&](const LogbookModel::ColumnID id, const QVariant &value)
    {
        return model->setData(model->index(currLookupIndex.row(), id), value, Qt::EditRole);
    };

    if ( getCurrIndexColumnValue(LogbookModel::COLUMN_CALL) != data.call)
    {
        qWarning() << "Callsigns don't match - skipping. QSO " << model->data(model->index(currLookupIndex.row(), LogbookModel::COLUMN_CALL), Qt::DisplayRole).toString()
                   << "data " << data.call;
        return;
    }

    const QString fnamelname = QString("%1 %2").arg(data.fname, data.lname);
    const QString &nameValue = getCurrIndexColumnValue(LogbookModel::COLUMN_NAME_INTL);

    const LogbookModel::EditStrategy originEditStrategy = model->editStrategy();

    model->setEditStrategy(QSqlTableModel::OnManualSubmit);

    if ( nameValue.isEmpty()
         || data.name_fmt.contains(nameValue)
         || fnamelname.contains(nameValue)
         || data.nick.contains(nameValue) )
    {
        QString name = data.name_fmt;
        if ( name.isEmpty() )
            name = ( data.fname.isEmpty() && data.lname.isEmpty() ) ? data.nick
                                                                    : fnamelname;
        setModelData(LogbookModel::COLUMN_NAME_INTL, name);
    }

    auto setIfEmpty = [&](const LogbookModel::ColumnID id,
                          const QString &callbookValue,
                          bool containsEnabled = false,
                          bool forceReplace = false)
    {
        if ( callbookValue.isEmpty() ) return;

        const QString &columnValue = getCurrIndexColumnValue(id);

        if ( columnValue.isEmpty()
             || (forceReplace && callbookValue != columnValue)
             || (containsEnabled
                  && callbookValue.contains(columnValue)
                  && callbookValue != columnValue) )
        {
            bool ret = setModelData(id, callbookValue);

            qCDebug(runtime) << "Changing"
                             << LogbookModel::getFieldNameTranslation(id) << callbookValue
                             << ret;
        }
    };

    setIfEmpty(LogbookModel::COLUMN_GRID, data.gridsquare, true);
    setIfEmpty(LogbookModel::COLUMN_QTH_INTL, data.qth);
    setIfEmpty(LogbookModel::COLUMN_DARC_DOK, data.dok);
    setIfEmpty(LogbookModel::COLUMN_IOTA, data.iota);
    setIfEmpty(LogbookModel::COLUMN_EMAIL, data.email);
    setIfEmpty(LogbookModel::COLUMN_COUNTY, data.county);
    setIfEmpty(LogbookModel::COLUMN_QSL_VIA, data.qsl_via);
    setIfEmpty(LogbookModel::COLUMN_WEB, data.url);
    setIfEmpty(LogbookModel::COLUMN_STATE, data.us_state);
    setIfEmpty(LogbookModel::COLUMN_ITUZ, data.ituz, false, true); // always replace if different
    setIfEmpty(LogbookModel::COLUMN_CQZ, data.cqz, false, true);   // always replace if different
    model->submitAll();

    model->setEditStrategy(originEditStrategy);
}

void LogbookWidget::callsignFound(const CallbookResponseData &data)
{
    FCT_IDENTIFICATION;

    updateQSORecordFromCallbook(data);
    currLookupIndex = QModelIndex();
    if ( lookupDialog )
        lookupDialog->setValue(lookupDialog->value() + 1);

    queryNextQSOLookupBatch();
}

void LogbookWidget::callsignNotFound(const QString &call)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << call << "not found";
    if ( lookupDialog )
        lookupDialog->setValue(lookupDialog->value() + 1);

    queryNextQSOLookupBatch();
}

void LogbookWidget::callbookLoginFailed(const QString&callbookString)
{
    FCT_IDENTIFICATION;

    finishQSOLookupBatch();
    QMessageBox::critical(this, tr("QLog Error"), callbookString + " " + tr("Callbook login failed"));
}

void LogbookWidget::callbookError(const QString &error)
{
    FCT_IDENTIFICATION;

    finishQSOLookupBatch();
    QMessageBox::critical(this, tr("QLog Error"), tr("Callbook error: ") + error);
}

void LogbookWidget::setCallsignSearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("Callsign"));

    if ( !callsignSearchValue.isEmpty() )
        ui->searchTextFilter->setText(callsignSearchValue);
}

void LogbookWidget::setGridsquareSearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("Gridsquare"));
}

void LogbookWidget::setPotaSearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("POTA"));
}

void LogbookWidget::setSotaSearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("SOTA"));
}

void LogbookWidget::setWwffSearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("WWFF"));
}

void LogbookWidget::setSigSearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("SIG"));
}

void LogbookWidget::setIOTASearch()
{
    FCT_IDENTIFICATION;

    clearSearchText();
    ui->searchTextFilter->setPlaceholderText(tr("IOTA"));
}

void LogbookWidget::filterCallsign(const QString &call)
{
    FCT_IDENTIFICATION;

    if ( call == callsignSearchValue )
        return;

    callsignSearchValue = call;

    if ( ui->actionSearchCallsign->isChecked() )
    {
        ui->searchTextFilter->blockSignals(true);
        ui->searchTextFilter->setText(call);
        ui->searchTextFilter->blockSignals(false);
    }

    filterTable();
}

void LogbookWidget::clearSearchText()
{
    FCT_IDENTIFICATION;

    if ( ui->searchTextFilter->text().isEmpty() )
        return;

    ui->searchTextFilter->clear();
}

void LogbookWidget::setupSearchMenu()
{
    FCT_IDENTIFICATION;

    QMenu *searchTypeMenu = new QMenu(ui->searchTypeButton);

    searchTypeGroup = new QActionGroup(this);
    searchTypeGroup->setExclusive(true);

    for ( auto it = searchTypeList.cbegin(); it != searchTypeList.cend(); ++it)
    {
        QAction *action = it.value().action;
        if ( action )
        {
            action->setActionGroup(searchTypeGroup);
            searchTypeMenu->addAction(action);
        }
    }

    connect(searchTypeGroup, &QActionGroup::triggered, this, [this](QAction *action)
    {
        saveSearchTextFilter(action);
    });

    ui->searchTypeButton->setMenu(searchTypeMenu);
}
void LogbookWidget::onSearchTextChanged()
{
    FCT_IDENTIFICATION;

    filterTable();
}

void LogbookWidget::bandFilterChanged()
{
    FCT_IDENTIFICATION;

    saveBandFilter();
    filterTable();;
}

void LogbookWidget::saveBandFilter()
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookFilterBand(ui->bandSelectFilter->currentText());
}

void LogbookWidget::restoreBandFilter()
{
    FCT_IDENTIFICATION;

    ui->bandSelectFilter->blockSignals(true);
    ui->bandSelectFilter->setCurrentText(LogParam::getLogbookFilterBand());
    ui->bandSelectFilter->blockSignals(false);
}

void LogbookWidget::modeFilterChanged()
{
    FCT_IDENTIFICATION;

    saveModeFilter();
    filterTable();
}

void LogbookWidget::saveModeFilter()
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookFilterMode(ui->modeSelectFilter->currentText());
}

void LogbookWidget::restoreModeFilter()
{
    FCT_IDENTIFICATION;

    ui->modeSelectFilter->blockSignals(true);
    ui->modeSelectFilter->setCurrentText(LogParam::getLogbookFilterMode());
    ui->modeSelectFilter->blockSignals(false);
}

void LogbookWidget::countryFilterChanged()
{
    FCT_IDENTIFICATION;

    saveCountryFilter();
    filterTable();
}

void LogbookWidget::saveCountryFilter()
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookFilterCountry(ui->countrySelectFilter->currentText());
}

void LogbookWidget::restoreCountryFilter()
{
    FCT_IDENTIFICATION;

    ui->countrySelectFilter->blockSignals(true);
    ui->countrySelectFilter->setCurrentText(LogParam::getLogbookFilterCountry());
    ui->countrySelectFilter->blockSignals(false);
}

void LogbookWidget::userFilterChanged()
{
    FCT_IDENTIFICATION;

    saveUserFilter();
    filterTable();
}

void LogbookWidget::setUserFilter(const QString &filterName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filterName;

    ui->userSelectFilter->setCurrentText(filterName);
}

void LogbookWidget::saveUserFilter()
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookFilterUserFilter(ui->userSelectFilter->currentText());
}

void LogbookWidget::restoreUserFilter()
{
    FCT_IDENTIFICATION;

    ui->userSelectFilter->blockSignals(true);
    ui->userSelectFilter->setCurrentText(LogParam::getLogbookFilterUserFilter());
    ui->userSelectFilter->blockSignals(false);
}

void LogbookWidget::clubFilterChanged()
{
    FCT_IDENTIFICATION;

    saveClubFilter();
    filterTable();
}

void LogbookWidget::refreshClubFilter()
{
    FCT_IDENTIFICATION;

    ui->clubSelectFilter->blockSignals(true);
    const QString &member = ui->clubSelectFilter->currentText();
    ui->clubSelectFilter->setModel(new QStringListModel(QStringList(tr("All Clubs"))
                                                        << MembershipQE::instance()->getEnabledClubLists(),ui->clubSelectFilter));
    ui->clubSelectFilter->adjustMaxSize();
    ui->clubSelectFilter->setCurrentText(member);
    ui->clubSelectFilter->blockSignals(false);
}

void LogbookWidget::refreshUserFilter()
{
    FCT_IDENTIFICATION;

    ui->userSelectFilter->refreshModel();

    filterTable();  // TODO ??? is it needed
}

void LogbookWidget::saveClubFilter()
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookFilterClub(ui->clubSelectFilter->currentText());
}

void LogbookWidget::restoreClubFilter()
{
    FCT_IDENTIFICATION;

    ui->clubSelectFilter->blockSignals(true);
    ui->clubSelectFilter->setCurrentText(LogParam::getLogbookFilterClub());
    ui->clubSelectFilter->blockSignals(false);
}

void LogbookWidget::saveSearchTextFilter(QAction *action)
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookFilterSearchType(action->data().toInt());
}

void LogbookWidget::restoreSearchTextFilter()
{
    FCT_IDENTIFICATION;

    int searchType = LogParam::getLogbookFilterSearchType(SearchType::CALLSIGN_SEARCH);

    const SearchDefinition &def = searchTypeList.value(static_cast<SearchType>(searchType));

    if ( def.action )
    {
        searchTypeGroup->blockSignals(true);
        def.action->setChecked(true);
        def.action->trigger();
        searchTypeGroup->blockSignals(false);
    }
}

void LogbookWidget::restoreFilters()
{
    FCT_IDENTIFICATION;

    restoreSearchTextFilter();
    restoreModeFilter();
    restoreBandFilter();
    restoreCountryFilter();
    restoreClubFilter();
    restoreUserFilter();
    externalFilter = QString();
    clearSearchText();
    filterTable();
}

void LogbookWidget::uploadClublog()
{
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);

    foreach (QModelIndex index, ui->contactTable->selectionModel()->selectedRows()) {
        QSqlRecord record = model->record(index.row());
        adi.exportContact(record);
    }

    stream.flush();

    //clublog->uploadAdif(data);
}

void LogbookWidget::deleteContact()
{
    FCT_IDENTIFICATION;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Delete"), tr("Delete the selected contacts?"),
                                  QMessageBox::Yes|QMessageBox::No);

    if ( reply != QMessageBox::Yes ) return;

    QModelIndexList deletedRowIndexes = ui->contactTable->selectionModel()->selectedRows();

    // Since deletedRowIndexes contains indexes for columns that might be invisible,
    // and scrollToIndex needs an index with a visible column,
    // we obtain the column index from the first record in the table."

    int firstVisibleColumnIndex = ui->contactTable->indexAt(QPoint(0, 0)).column();
    QModelIndex previousIndex = model->index(deletedRowIndexes.first().row()-1, firstVisibleColumnIndex);

    // Clublog does not accept batch DELETE operation
    // ask if an operator wants to continue
    if ( ClubLogBase::isUploadImmediatelyEnabled()
         && deletedRowIndexes.count() > 5 )
    {
        reply = QMessageBox::question(this,
                                      tr("Delete"),
                                      tr("Clublog's <b>Immediately Send</b> supports only one-by-one deletion<br><br>"
                                         "Do you want to continue despite the fact<br>"
                                         "that the DELETE operation will not be sent to Clublog?"),
                                      QMessageBox::Yes|QMessageBox::No);
        if ( reply != QMessageBox::Yes )
            return;
        else
            blockClublogSignals = true;
    }

    //It must be sorted in descending order to delete the correct rows.
    std::sort(deletedRowIndexes.begin(),
              deletedRowIndexes.end(),
              [](const QModelIndex &a, const QModelIndex &b)
    {
        return a.row() > b.row();
    });

    QProgressDialog *progress = new QProgressDialog(tr("Deleting QSOs"),
                                                    tr("Cancel"),
                                                    0,
                                                    deletedRowIndexes.size(),
                                                    this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setValue(0);
    progress->setAttribute(Qt::WA_DeleteOnClose, true);
    progress->setAutoClose(true);
    progress->show();

    // disable Updates and current connection between model and QTableView
    // to improve performance
    // when reconnected model, the column are reordered. That's why we remember them
    // and restore them again after deletion.
    QByteArray state = ui->contactTable->horizontalHeader()->saveState();
    ui->contactTable->setUpdatesEnabled(false);
    ui->contactTable->setModel(nullptr);
    QCoreApplication::processEvents();

    quint32 cnt = 0;

    QSet<uint> removedEntities;
    removedEntities.reserve(deletedRowIndexes.size());

    for ( const QModelIndex &index : static_cast<const QModelIndexList&>(deletedRowIndexes) )
    {
        cnt++;
        removedEntities << model->data(model->index(index.row(), LogbookModel::COLUMN_DXCC), Qt::DisplayRole).toUInt();
        model->removeRow(index.row());

        if ( progress->wasCanceled() )
            break;

        if ( cnt % 50 == 0 )
            progress->setValue(cnt);
    }

    progress->setValue(deletedRowIndexes.size());
    progress->done(QDialog::Accepted);

    // enable connection between model and QTableView
    ui->contactTable->setModel(model);
    ui->contactTable->setUpdatesEnabled(true);
    ui->contactTable->horizontalHeader()->restoreState(state);
    updateTable();
    scrollToIndex(previousIndex);
    blockClublogSignals = false;
    emit deletedEntities(removedEntities);
}

void LogbookWidget::exportContact()
{
    FCT_IDENTIFICATION;

    QList<QSqlRecord>QSOs;
    const QModelIndexList &selectedIndexes = ui->contactTable->selectionModel()->selectedRows();

    if ( selectedIndexes.count() < 1 )
        return;

    for ( const QModelIndex &index : selectedIndexes )
        QSOs << model->record(index.row());

    ExportDialog dialog(QSOs);
    dialog.exec();
}

void LogbookWidget::editContact()
{
    FCT_IDENTIFICATION;

    if ( ui->contactTable->selectionModel()->selectedRows().size() > 1 )
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this,
                                      tr("Update"),
                                      tr("By updating, all selected rows will be affected.<br>The value currently edited in the column will be applied to all selected rows.<br><br>Do you want to edit them?"),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply != QMessageBox::Yes) return;
    }

    ui->contactTable->edit(ui->contactTable->selectionModel()->currentIndex());
}

void LogbookWidget::displayedColumns()
{
    FCT_IDENTIFICATION;

    ColumnSettingDialog dialog(ui->contactTable);

    dialog.exec();

    saveTableHeaderState();
}

void LogbookWidget::reselectModel()
{
    FCT_IDENTIFICATION;

    model->select();

    // under normal conditions, only 256 records are loaded.
    // This will increase the value of the loaded records.
    while ( model->canFetchMore() && model->rowCount() < 5000 )
        model->fetchMore();

    // it is not possible to use mode->rowCount here because model contains only
    // the first 5000 records (or more) and rowCount has a value 5000 here. Therefore, it is needed
    // to run a QSL stateme with Count. Run it only in case when QTableview does not contain all
    // records from model
    int qsoCount = 0;
    if ( model->canFetchMore() )
    {
        QString countRecordsStmt(QLatin1String("SELECT COUNT(1) FROM contacts"));

        if ( !model->filter().isEmpty() )
            countRecordsStmt.append(QString(" WHERE %1").arg(model->filter()));

        QSqlQuery query(countRecordsStmt);

        qsoCount = query.first() ? query.value(0).toInt() : 0;
    }
    else
        qsoCount = model->rowCount();

    ui->filteredQSOsLabel->setText(tr("Count: %n", "", qsoCount));
}

void LogbookWidget::updateTable()
{
    FCT_IDENTIFICATION;

    reselectModel();

    // it is called when QSO is inserted/updated/deleted
    // therefore it is needed to refresh country select box
    ui->countrySelectFilter->refreshModel();
    emit logbookUpdated();
}

void LogbookWidget::saveTableHeaderState()
{
    FCT_IDENTIFICATION;

    LogParam::setLogbookState(ui->contactTable->horizontalHeader()->saveState());
}

void LogbookWidget::showTableHeaderContextMenu(const QPoint& point)
{
    FCT_IDENTIFICATION;

    QMenu* contextMenu = new QMenu(this);
    for (int i = 0; i < model->columnCount(); i++)
    {
        const QString &name = model->headerData(i, Qt::Horizontal).toString();
        QAction* action = new QAction(name, contextMenu);
        action->setCheckable(true);
        action->setChecked(!ui->contactTable->isColumnHidden(i));

        connect(action, &QAction::triggered, this, [this, i]()
        {
            ui->contactTable->setColumnHidden(i, !ui->contactTable->isColumnHidden(i));
            saveTableHeaderState();
        });

        contextMenu->addAction(action);
    }

    contextMenu->exec(point);
}

void LogbookWidget::doubleClickColumn(QModelIndex modelIndex)
{
    FCT_IDENTIFICATION;


    /***********************/
    /* show EQSL QSL Image */
    /***********************/
    if ( modelIndex.column() == LogbookModel::COLUMN_EQSL_QSL_RCVD
         && modelIndex.data().toString() == 'Y')
    {
        QProgressDialog* dialog = new QProgressDialog(tr("Downloading eQSL Image"), tr("Cancel"), 0, 0, this);
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setRange(0, 0);
        dialog->setAutoClose(true);
        dialog->show();

        EQSLQSLDownloader *eQSL = new EQSLQSLDownloader(dialog);

        connect(eQSL, &EQSLQSLDownloader::QSLImageFound, this, [dialog, eQSL](QString imgFile)
        {
            dialog->done(0);
            QDesktopServices::openUrl(QUrl::fromLocalFile(imgFile));
            eQSL->deleteLater();
        });

        connect(eQSL, &EQSLQSLDownloader::QSLImageError, this, [this, dialog, eQSL](const QString &error)
        {
            dialog->done(1);
            QMessageBox::critical(this, tr("QLog Error"), tr("eQSL Download Image failed: ") + error);
            eQSL->deleteLater();
        });

        connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
        {
            qCDebug(runtime)<< "Operation canceled";
            eQSL->abortDownload();
            eQSL->deleteLater();
        });

        eQSL->getQSLImage(model->record(modelIndex.row()));
    }
    /**************************/
    /* show Paper QSL Manager */
    /**************************/
    else if ( modelIndex.column() == LogbookModel::COLUMN_QSL_RCVD
         && modelIndex.data().toString() == 'Y' )
    {
        PaperQSLDialog dialog(model->record(modelIndex.row()));
        dialog.exec();
    }
    /**************************************/
    /* show generic QSO Show/Edit Dialog  */
    /**************************************/
    else
    {
        QSODetailDialog dialog(model->record(modelIndex.row()));
        connect(&dialog, &QSODetailDialog::contactUpdated, this, [this](QSqlRecord& record)
        {
            emit contactUpdated(record);
            emit clublogContactUpdated(record);
        });
        dialog.exec();
        updateTable();
        scrollToIndex(modelIndex);
    }
}

void LogbookWidget::handleBeforeUpdate(int row, QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    Q_UNUSED(row);
    emit contactUpdated(record);
}

void LogbookWidget::handleBeforeDelete(int row)
{
    FCT_IDENTIFICATION;

    const QSqlRecord &oldRecord = model->record(row);
    emit contactDeleted(oldRecord);
    if ( !blockClublogSignals )
        emit clublogContactDeleted(oldRecord);
}

void LogbookWidget::focusSearchCallsign()
{
    FCT_IDENTIFICATION;

    ui->searchTextFilter->setFocus();
}

void LogbookWidget::reloadSetting()
{
    FCT_IDENTIFICATION;
    /* Refresh dynamic Club selection combobox */
    refreshClubFilter();
    callbookManager.initCallbooks();
    updateTable();
}

void LogbookWidget::sendDXCSpot()
{
    FCT_IDENTIFICATION;

    const QModelIndexList &selectedIndexes = ui->contactTable->selectionModel()->selectedRows();

    if ( selectedIndexes.count() < 1 )
        return;

    emit sendDXSpotContactReq(model->record(selectedIndexes.at(0).row()));
}

void LogbookWidget::setDefaultSort()
{
    FCT_IDENTIFICATION;

    ui->contactTable->sortByColumn(LogbookModel::COLUMN_TIME_ON, Qt::DescendingOrder);
}

void LogbookWidget::scrollToIndex(const QModelIndex &index, bool selectItem)
{
    FCT_IDENTIFICATION;

    if ( index == QModelIndex() )
        return;

    // is index visible ?
    if ( ui->contactTable->visualRect(index).isEmpty() )
    {
        while ( model->canFetchMore() && ui->contactTable->visualRect(index).isEmpty() )
            model->fetchMore();

        if ( model->canFetchMore() )
            model->fetchMore(); // one more fetch
    }

    ui->contactTable->scrollTo(index, QAbstractItemView::PositionAtCenter);
    if ( selectItem )
        ui->contactTable->selectRow(index.row());
}

void LogbookWidget::adjusteComboMinSize(QComboBox *combo)
{
    FCT_IDENTIFICATION;

    if (combo->count() <= 0 )
        return;

    QFontMetrics fontMetrics(combo->font());
    combo->setMinimumWidth(fontMetrics.horizontalAdvance(combo->itemText(0)) + 35);
}

bool LogbookWidget::eventFilter(QObject *obj, QEvent *event)
{
    //FCT_IDENTIFICATION;

    if ( event->type() == QEvent::KeyPress && obj == ui->contactTable )
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        // Block SelectAll
        if ( QKeySequence(keyEvent->modifiers() | keyEvent->key()) == QKeySequence::SelectAll )
            return true;
    }

    return QObject::eventFilter(obj, event);
}

void LogbookWidget::colorsFilterWidget(QComboBox *widget)
{
    FCT_IDENTIFICATION;

    widget->setStyleSheet( (widget->currentIndex() > 0) ? "QComboBox {border: 2px solid red; border-radius: 4px; padding: 2px;}"
                                                        : "");
}

void LogbookWidget::filterTable()
{
    FCT_IDENTIFICATION;

    QStringList filterString;
    QString searchText = ui->searchTextFilter->text();

    // an external request from Callsign search is always used (the request is sent by the NewContact Widget)
    if ( !ui->actionSearchCallsign->isChecked() && !callsignSearchValue.isEmpty() )
        filterString.append(QString("callsign LIKE '%%1%'").arg(callsignSearchValue.toUpper()));

    for ( auto it = searchTypeList.cbegin(); it != searchTypeList.cend(); it++)
    {
        const SearchDefinition &def = it.value();
        if ( !def.action ) continue;
        if ( def.action->isChecked() && !searchText.isEmpty() )
            filterString.append(QString("%1 LIKE '%%2%'").arg(def.dbColumn, searchText.toUpper()));
    }

    const QString &bandFilterValue = ui->bandSelectFilter->currentText();

    if ( ui->bandSelectFilter->currentIndex() != 0 && !bandFilterValue.isEmpty())
        filterString.append(QString("band = '%1'").arg(bandFilterValue));

    const QString &modeFilterValue = ui->modeSelectFilter->currentText();

    if ( ui->modeSelectFilter->currentIndex() != 0 && !modeFilterValue.isEmpty() )
        filterString.append(QString("mode = '%1'").arg(modeFilterValue));

    bool OK = false;
    int countryCode = ui->countrySelectFilter->currentValue(1).toInt(&OK);

    if ( OK && countryCode > 0 )
        filterString.append(QString("dxcc = '%1'").arg(countryCode));

    if ( ui->clubSelectFilter->currentIndex() != 0 )
        filterString.append(QString("id in (SELECT contactid FROM contact_clubs_view WHERE clubid = '%1')").arg(ui->clubSelectFilter->currentText()));

    if ( ui->userSelectFilter->currentIndex() != 0 )
        filterString.append(QSOFilterManager::instance()->getWhereClause(ui->userSelectFilter->currentText()));

    if ( !externalFilter.isEmpty() )
        filterString.append(QString("( ") + externalFilter + ")");

    model->setFilter(filterString.join(" AND "));
    qCDebug(runtime) << model->query().lastQuery();

    reselectModel();
}

LogbookWidget::~LogbookWidget()
{
    FCT_IDENTIFICATION;


    if ( lookupDialog )
    {
        callbookManager.abortQuery();
        finishQSOLookupBatch();
    }
    delete ui;
}

void LogbookWidget::finalizeBeforeAppExit()
{
    FCT_IDENTIFICATION;

    saveTableHeaderState();
}
