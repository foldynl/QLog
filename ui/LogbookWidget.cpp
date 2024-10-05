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

#include "logformat/AdiFormat.h"
#include "models/LogbookModel.h"
#include "models/SqlListModel.h"
#include "core/ClubLog.h"
#include "LogbookWidget.h"
#include "ui_LogbookWidget.h"
#include "ui/StyleItemDelegate.h"
#include "core/debug.h"
#include "models/SqlListModel.h"
#include "ui/ColumnSettingDialog.h"
#include "data/Data.h"
#include "ui/ExportDialog.h"
#include "core/Eqsl.h"
#include "ui/PaperQSLDialog.h"
#include "ui/QSODetailDialog.h"
#include "core/MembershipQE.h"
#include "core/GenericCallbook.h"
#include "core/ClubLog.h"

MODULE_IDENTIFICATION("qlog.ui.logbookwidget");

LogbookWidget::LogbookWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LogbookWidget),
    blockClublogSignals(false)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    model = new LogbookModel(this);
    connect(model, &LogbookModel::beforeUpdate, this, &LogbookWidget::handleBeforeUpdate);
    connect(model, &LogbookModel::beforeDelete, this, &LogbookWidget::handleBeforeDelete);
    connect(ui->contactTable, &QTableQSOView::dataCommitted, this, [this](){emit logbookUpdated();});

    ui->contactTable->setModel(model);

    QAction *separator = new QAction(this);
    separator->setSeparator(true);

    QAction *separator1 = new QAction(this);
    separator1->setSeparator(true);

    ui->contactTable->addAction(ui->actionEditContact);
    ui->contactTable->addAction(ui->actionFilter);
    ui->contactTable->addAction(ui->actionLookup);
    ui->contactTable->addAction(ui->actionSendDXCSpot);
    ui->contactTable->addAction(ui->actionExportAs);
    ui->contactTable->addAction(separator);
    ui->contactTable->addAction(ui->actionDisplayedColumns);
    ui->contactTable->addAction(separator1);
    ui->contactTable->addAction(ui->actionDeleteContact);

    ui->contactTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->contactTable->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &LogbookWidget::showTableHeaderContextMenu);

    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TIME_ON, new TimestampFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TIME_OFF, new TimestampFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CALL, new CallsignDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FREQUENCY, new UnitFormatDelegate("", 6, 0.001, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_BAND, new ComboFormatDelegate(new SqlListModel("SELECT name FROM bands ORDER BY start_freq", " "), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MODE, new ComboFormatDelegate(new SqlListModel("SELECT name FROM modes", " "), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CONTINENT, new ComboFormatDelegate(QStringList()<<" "<< "AF" << "AN" << "AS" << "EU" << "NA" << "OC" << "SA"));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT_VIA, new ComboFormatDelegate(Data::instance()->qslSentViaEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD_VIA, new ComboFormatDelegate(Data::instance()->qslSentViaEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_RCVD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_SENT_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TX_POWER, new UnitFormatDelegate("W", 3, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_AGE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ALTITUDE, new UnitFormatDelegate("m", 2, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_A_INDEX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_AZ, new UnitFormatDelegate("", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_EL, new UnitFormatDelegate("", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_PATH, new ComboFormatDelegate(Data::instance()->antPathEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DISTANCE, new DistanceFormatDelegate(1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSLRDATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSLSDATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSL_RCVD, new ComboFormatDelegate(Data::instance()->qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSL_SENT, new ComboFormatDelegate(Data::instance()->qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FISTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FISTS_CC, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FORCE_INIT, new ComboFormatDelegate(Data::instance()->boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FREQ_RX, new UnitFormatDelegate("", 6, 0.001, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_BAND_RX, new ComboFormatDelegate(new SqlListModel("SELECT name FROM bands ORDER BY start_freq", " "), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMLOGEU_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HAMQTH_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_IOTA_ISLAND_ID, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_K_INDEX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MAX_BURSTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_ALTITUDE, new UnitFormatDelegate("m", 2, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_CQ_ZONE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_DXCC, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_FISTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_IOTA_ISLAND_ID, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_ITU_ZONE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NR_BURSTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NR_PINGS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NOTES_INTL, new TextBoxDelegate(this));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NOTES, new TextBoxDelegate(this));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_PROP_MODE, new ComboFormatDelegate(QStringList()<<" "<< Data::instance()->propagationModesIDList(), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_STATUS, new ComboFormatDelegate(Data::instance()->uploadStatusEnum, ui->contactTable));
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

    QSettings settings;
    const QByteArray &logbookState = settings.value("logbook/state").toByteArray();
    if (!logbookState.isEmpty()) {
        ui->contactTable->horizontalHeader()->restoreState(logbookState);
    }
    else {
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

    ui->contactTable->horizontalHeader()->setSectionsMovable(true);
    ui->contactTable->setStyle(new ProxyStyle(ui->contactTable->style()));
    ui->contactTable->installEventFilter(this);

    ui->bandFilter->blockSignals(true);
    ui->bandFilter->setModel(new SqlListModel("SELECT name FROM bands ORDER BY start_freq", tr("Band"), this));
    ui->bandFilter->blockSignals(false);

    ui->modeFilter->blockSignals(true);
    ui->modeFilter->setModel(new SqlListModel("SELECT name FROM modes", tr("Mode"), this));
    ui->modeFilter->blockSignals(false);

    ui->countryFilter->blockSignals(true);
    countryModel = new SqlListModel("SELECT id, translate_to_locale(name) "
                                    "FROM dxcc_entities WHERE id IN (SELECT DISTINCT dxcc FROM contacts) "
                                    "ORDER BY 2 COLLATE LOCALEAWARE ASC;", tr("Country"), this);
    while (countryModel->canFetchMore())
        countryModel->fetchMore();

    ui->countryFilter->setModel(countryModel);
    ui->countryFilter->setModelColumn(1);
    ui->countryFilter->blockSignals(false);

    refreshClubFilter();

    ui->userFilter->blockSignals(true);
    userFilterModel = new SqlListModel("SELECT filter_name "
                                       "FROM qso_filters "
                                       "ORDER BY filter_name", tr("User Filter"), this);
    while (userFilterModel->canFetchMore())
        userFilterModel->fetchMore();
    ui->userFilter->setModel(userFilterModel);
    ui->userFilter->blockSignals(false);

    clublog = new ClubLog(this);

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

    ui->countryFilter->blockSignals(true);
    ui->bandFilter->blockSignals(true);
    ui->userFilter->blockSignals(true);
    ui->modeFilter->blockSignals(true);
    ui->clubFilter->blockSignals(true);

    if ( ! countryName.isEmpty() )
        ui->countryFilter->setCurrentText(countryName);
    else
        ui->countryFilter->setCurrentIndex(0);

    if ( !band.isEmpty() )
        ui->bandFilter->setCurrentText(band);
    else
        ui->bandFilter->setCurrentIndex(0);

    //user wants to see only selected band and country
    ui->userFilter->setCurrentIndex(0); //suppress user-defined filter
    ui->modeFilter->setCurrentIndex(0); //suppress mode filter
    ui->clubFilter->setCurrentIndex(0); //suppress club filter

    // set additional filter
    externalFilter = addlFilter;

    ui->clubFilter->blockSignals(false);
    ui->userFilter->blockSignals(false);
    ui->modeFilter->blockSignals(false);
    ui->countryFilter->blockSignals(false);
    ui->bandFilter->blockSignals(false);

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

void LogbookWidget::filterCallsign(const QString &call)
{
    FCT_IDENTIFICATION;

    if ( call == ui->callsignFilter->text() )
        return;

    ui->callsignFilter->setText(call);
}

void LogbookWidget::callsignFilterChanged()
{
    FCT_IDENTIFICATION;

    filterTable();
}

void LogbookWidget::bandFilterChanged()
{
    FCT_IDENTIFICATION;

    colorsFilterWidget(ui->bandFilter);
    saveBandFilter();
    filterTable();;
}

void LogbookWidget::saveBandFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("logbook/filters/band", ui->bandFilter->currentText());
}

void LogbookWidget::restoreBandFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    ui->bandFilter->blockSignals(true);
    const QString &value = settings.value("logbook/filters/band").toString();
    if ( !value.isEmpty() )
        ui->bandFilter->setCurrentText(value);
    else
        ui->bandFilter->setCurrentIndex(0);

    colorsFilterWidget(ui->bandFilter);
    ui->bandFilter->blockSignals(false);
}

void LogbookWidget::modeFilterChanged()
{
    FCT_IDENTIFICATION;

    colorsFilterWidget(ui->modeFilter);
    saveModeFilter();
    filterTable();
}

void LogbookWidget::saveModeFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("logbook/filters/mode", ui->modeFilter->currentText());
}

void LogbookWidget::restoreModeFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    ui->modeFilter->blockSignals(true);
    const QString &value = settings.value("logbook/filters/mode").toString();
    if ( !value.isEmpty() )
        ui->modeFilter->setCurrentText(value);
    else
        ui->modeFilter->setCurrentIndex(0);

    colorsFilterWidget(ui->modeFilter);
    ui->modeFilter->blockSignals(false);
}

void LogbookWidget::countryFilterChanged()
{
    FCT_IDENTIFICATION;

    colorsFilterWidget(ui->countryFilter);
    saveCountryFilter();
    filterTable();
}

void LogbookWidget::saveCountryFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("logbook/filters/country", ui->countryFilter->currentText());
}

void LogbookWidget::restoreCountryFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    ui->countryFilter->blockSignals(true);
    const QString &value = settings.value("logbook/filters/country").toString();
    if ( !value.isEmpty() )
        ui->countryFilter->setCurrentText(value);
    else
        ui->countryFilter->setCurrentIndex(0);
    colorsFilterWidget(ui->countryFilter);

    ui->countryFilter->blockSignals(false);
}

void LogbookWidget::userFilterChanged()
{
    FCT_IDENTIFICATION;

    colorsFilterWidget(ui->userFilter);
    saveUserFilter();
    filterTable();
}

void LogbookWidget::saveUserFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("logbook/filters/user", ui->userFilter->currentText());
}

void LogbookWidget::restoreUserFilter()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    ui->userFilter->blockSignals(true);
    const QString &value = settings.value("logbook/filters/user").toString();
    if ( !value.isEmpty() )
        ui->userFilter->setCurrentText(value);
    else
        ui->userFilter->setCurrentIndex(0);

    colorsFilterWidget(ui->userFilter);
    ui->userFilter->blockSignals(false);
}

void LogbookWidget::clubFilterChanged()
{
    FCT_IDENTIFICATION;

    colorsFilterWidget(ui->clubFilter);
    saveClubFilter();
    filterTable();
}

void LogbookWidget::refreshClubFilter()
{
    FCT_IDENTIFICATION;

    ui->clubFilter->blockSignals(true);
    const QString &member = ui->clubFilter->currentText();
    ui->clubFilter->clear();
    ui->clubFilter->addItems(QStringList(tr("Club")) << MembershipQE::instance()->getEnabledClubLists());
    ui->clubFilter->setCurrentText(member);
    ui->clubFilter->blockSignals(false);
    colorsFilterWidget(ui->clubFilter);
}

void LogbookWidget::refreshUserFilter()
{
    FCT_IDENTIFICATION;

    /* Refresh dynamic User Filter selection combobox */
    /* block the signals !!! */
    ui->userFilter->blockSignals(true);
    const QString &userFilterString = ui->userFilter->currentText();
    userFilterModel->refresh();
    ui->userFilter->setCurrentText(userFilterString);
    ui->userFilter->blockSignals(false);
    colorsFilterWidget(ui->userFilter);

    filterTable();
}

void LogbookWidget::saveClubFilter()
{
    QSettings settings;
    settings.setValue("logbook/filters/member", ui->clubFilter->currentText());
}

void LogbookWidget::restoreClubFilter()
{
    QSettings settings;
    ui->clubFilter->blockSignals(true);
    const QString &value = settings.value("logbook/filters/member").toString();
    if ( !value.isEmpty() )
        ui->clubFilter->setCurrentText(value);
    else
        ui->clubFilter->setCurrentIndex(0);

    colorsFilterWidget(ui->clubFilter);
    ui->clubFilter->blockSignals(false);
}

void LogbookWidget::restoreFilters()
{
    FCT_IDENTIFICATION;

    restoreModeFilter();
    restoreBandFilter();
    restoreCountryFilter();
    restoreClubFilter();
    restoreUserFilter();
    externalFilter = QString();
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
    if ( ClubLog::isUploadImmediatelyEnabled()
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

    int cnt = 0;

    for ( const QModelIndex &index : static_cast<const QModelIndexList&>(deletedRowIndexes) )
    {
        cnt++;
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

    /* Refresh country selection combobox */
    /* block the signals !!! */
    ui->countryFilter->blockSignals(true);
    const QString &country = ui->countryFilter->currentText();
    countryModel->refresh();
    ui->countryFilter->setCurrentText(country);
    ui->countryFilter->blockSignals(false);
    colorsFilterWidget(ui->countryFilter);
    emit logbookUpdated();
}

void LogbookWidget::saveTableHeaderState()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QByteArray logbookState = ui->contactTable->horizontalHeader()->saveState();
    settings.setValue("logbook/state", logbookState);
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

        EQSL *eQSL = new EQSL(dialog);

        connect(eQSL, &EQSL::QSLImageFound, this, [dialog, eQSL](QString imgFile)
        {
            dialog->done(0);
            QDesktopServices::openUrl(QUrl::fromLocalFile(imgFile));
            eQSL->deleteLater();
        });

        connect(eQSL, &EQSL::QSLImageError, this, [this, dialog, eQSL](const QString &error)
        {
            dialog->done(1);
            QMessageBox::critical(this, tr("QLog Error"), tr("eQSL Download Image failed: ") + error);
            eQSL->deleteLater();
        });

        connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
        {
            qCDebug(runtime)<< "Operation canceled";
            eQSL->abortRequest();
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

    ui->callsignFilter->setFocus();
}

void LogbookWidget::reloadSetting()
{
    FCT_IDENTIFICATION;
    /* Refresh dynamic Club selection combobox */
    refreshClubFilter();
}

void LogbookWidget::sendDXCSpot()
{
    FCT_IDENTIFICATION;

    const QModelIndexList &selectedIndexes = ui->contactTable->selectionModel()->selectedRows();

    if ( selectedIndexes.count() < 1 )
        return;

    emit sendDXSpotContactReq(model->record(selectedIndexes.at(0).row()));
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

    widget->setStyleSheet( (widget->currentIndex() > 0) ? "QComboBox {color: green}"
                                                        : "");
}

void LogbookWidget::filterTable()
{
    FCT_IDENTIFICATION;

    QStringList filterString;

    const QString &callsignFilterValue = ui->callsignFilter->text();

    if ( !callsignFilterValue.isEmpty() )
        filterString.append(QString("callsign LIKE '%%1%'").arg(callsignFilterValue.toUpper()));

    const QString &bandFilterValue = ui->bandFilter->currentText();

    if ( ui->bandFilter->currentIndex() != 0 && !bandFilterValue.isEmpty())
        filterString.append(QString("band = '%1'").arg(bandFilterValue));

    const QString &modeFilterValue = ui->modeFilter->currentText();

    if ( ui->modeFilter->currentIndex() != 0 && !modeFilterValue.isEmpty() )
        filterString.append(QString("mode = '%1'").arg(modeFilterValue));

    int row = ui->countryFilter->currentIndex();
    const QModelIndex &idx = ui->countryFilter->model()->index(row,0);
    const QVariant &data = ui->countryFilter->model()->data(idx);

    if ( ui->countryFilter->currentIndex() != 0 )
        filterString.append(QString("dxcc = '%1'").arg(data.toInt()));

    if ( ui->clubFilter->currentIndex() != 0 )
        filterString.append(QString("id in (SELECT contactid FROM contact_clubs_view WHERE clubid = '%1')").arg(ui->clubFilter->currentText()));

    if ( ui->userFilter->currentIndex() != 0 )
    {
        QSqlQuery userFilterQuery;
        if ( ! userFilterQuery.prepare("SELECT "
                                     "'(' || GROUP_CONCAT( ' ' || c.name || ' ' || CASE WHEN r.value IS NULL AND o.sql_operator IN ('=', 'like') THEN 'IS' "
                                     "                                                  WHEN r.value IS NULL and r.operator_id NOT IN ('=', 'like') THEN 'IS NOT' "
                                     "                                                  WHEN o.sql_operator = ('starts with') THEN 'like' "
                                     "                                                  ELSE o.sql_operator END || "
                                     "' (' || quote(CASE o.sql_operator WHEN 'like' THEN '%' || r.value || '%' "
                                     "                                  WHEN 'not like' THEN '%' || r.value || '%' "
                                     "                                  WHEN 'starts with' THEN r.value || '%' "
                                     "                                  ELSE r.value END)  || ') ', m.sql_operator) || ')' "
                                     "FROM qso_filters f, qso_filter_rules r, "
                                     "qso_filter_operators o, qso_filter_matching_types m, "
                                     "PRAGMA_TABLE_INFO('contacts') c "
                                     "WHERE f.filter_name = :filterName "
                                     "      AND f.filter_name = r.filter_name "
                                     "      AND o.operator_id = r.operator_id "
                                     "      AND m.matching_id = f.matching_type "
                                     "      AND c.cid = r.table_field_index") )
        {
            qWarning() << "Cannot prepare select statement";
            return;
        }

        userFilterQuery.bindValue(":filterName", ui->userFilter->currentText());

        qCDebug(runtime) << "User filter SQL: " << userFilterQuery.lastQuery();

        if ( userFilterQuery.exec() )
        {
            userFilterQuery.next();
            filterString.append(QString("( ") + userFilterQuery.value(0).toString() + ")");
        }
        else
            qCDebug(runtime) << "User filter error - " << userFilterQuery.lastError().text();
    }

    if ( !externalFilter.isEmpty() )
        filterString.append(QString("( ") + externalFilter + ")");

    model->setFilter(filterString.join(" AND "));
    qCDebug(runtime) << model->query().lastQuery();

    reselectModel();
}

LogbookWidget::~LogbookWidget()
{
    FCT_IDENTIFICATION;

    saveTableHeaderState();
    delete ui;
}
