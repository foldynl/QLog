#include <QtSql>
#include <QMessageBox>
#include <QDesktopServices>
#include <QMenu>
#include "logformat/AdiFormat.h"
#include "models/LogbookModel.h"
#include "models/SqlListModel.h"
#include "core/ClubLog.h"
#include "LogbookWidget.h"
#include "ui_LogbookWidget.h"
#include "core/StyleItemDelegate.h"
#include "core/debug.h"
#include "models/SqlListModel.h"
#include "ui/ColumnSettingDialog.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.logbookwidget");

LogbookWidget::LogbookWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LogbookWidget)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    model = new LogbookModel(this);
    ui->contactTable->setModel(model);

    ui->contactTable->addAction(ui->actionEditContact);
    ui->contactTable->addAction(ui->actionFilter);
    ui->contactTable->addAction(ui->actionLookup);
    ui->contactTable->addAction(ui->actionDisplayedColumns);
    ui->contactTable->addAction(ui->actionUploadClublog);
    ui->contactTable->addAction(ui->actionDeleteContact);

    //ui->contactTable->sortByColumn(1, Qt::DescendingOrder);

    ui->contactTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->contactTable->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &LogbookWidget::showTableHeaderContextMenu);

    QMap<QString, QString> qslSentEnum;
    qslSentEnum["Y"] = tr("Yes");
    qslSentEnum["N"] = tr("No");
    qslSentEnum["R"] = tr("Requested");
    qslSentEnum["Q"] = tr("Queued");
    qslSentEnum["I"] = tr("Invalid");

    QMap<QString, QString> qslSentViaEnum;
    qslSentViaEnum["B"] = tr("Bureau");
    qslSentViaEnum["D"] = tr("Direct");
    qslSentViaEnum["E"] = tr("Electronic");
    qslSentViaEnum[" "] = tr("No Value");

    QMap<QString, QString> qslRcvdEnum;
    qslRcvdEnum["Y"] = tr("Yes");
    qslRcvdEnum["N"] = tr("No");
    qslRcvdEnum["R"] = tr("Requested");
    qslRcvdEnum["I"] = tr("Invalid");

    QMap<QString, QString> uploadStatusEnum;
    uploadStatusEnum["Y"] = tr("Yes");
    uploadStatusEnum["N"] = tr("No");
    uploadStatusEnum["M"] = tr("Modified");
    uploadStatusEnum[" "] = tr("No Value");

    QMap<QString, QString> antPathEnum;
    antPathEnum["G"] = tr("Grayline");
    antPathEnum["O"] = tr("Other");
    antPathEnum["S"] = tr("Short Path");
    antPathEnum["L"] = tr("Long Path");
    antPathEnum[" "] = tr("No Value");

    QMap<QString, QString> boolEnum;
    boolEnum["Y"] = tr("Yes");
    boolEnum["N"] = tr("No");
    boolEnum[" "] = tr("No Value");

    QMap<QString, QString> qsoCompleteEnum;
    qsoCompleteEnum["Y"] = tr("Yes");
    qsoCompleteEnum["N"] = tr("No");
    qsoCompleteEnum["Nil"] = tr("Not Heard");
    qsoCompleteEnum["?"] = tr("uncertain");
    qsoCompleteEnum[" "] = tr("No Value");

    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TIME_ON, new TimestampFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TIME_OFF, new TimestampFormatDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CALL, new CallsignDelegate(ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FREQUENCY, new UnitFormatDelegate("MHz", 6, 0.001, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MODE, new ComboFormatDelegate(new SqlListModel("SELECT name FROM modes", " "), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CONTINENT, new ComboFormatDelegate(QStringList()<<" "<< "AF" << "AN" << "AS" << "EU" << "NA" << "OC" << "SA"));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT, new ComboFormatDelegate(qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT_VIA, new ComboFormatDelegate(qslSentViaEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD, new ComboFormatDelegate(qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD_VIA, new ComboFormatDelegate(qslSentViaEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_SENT_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSL_RCVD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_SENT, new ComboFormatDelegate(qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_RCVD, new ComboFormatDelegate(qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_RCVD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_LOTW_SENT_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TX_POWER, new UnitFormatDelegate("W", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_AGE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_A_INDEX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_AZ, new UnitFormatDelegate("", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_EL, new UnitFormatDelegate("", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_ANT_PATH, new ComboFormatDelegate(antPathEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_CLUBLOG_QSO_UPLOAD_STATUS, new ComboFormatDelegate(uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_DISTANCE, new UnitFormatDelegate("km", 1, 0.1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSLRDATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSLSDATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSL_RCVD, new ComboFormatDelegate(qslRcvdEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_EQSL_QSL_SENT, new ComboFormatDelegate(qslSentEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FISTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FISTS_CC, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FORCE_INIT, new ComboFormatDelegate(boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_FREQ_RX, new UnitFormatDelegate("MHz", 6, 0.001, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_HRDLOG_QSO_UPLOAD_STATUS, new ComboFormatDelegate(uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_IOTA_ISLAND_ID, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_K_INDEX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MAX_BURSTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_CQ_ZONE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_DXCC, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_FISTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_IOTA_ISLAND_ID, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_MY_ITU_ZONE, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NR_BURSTS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_NR_PINGS, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_PROP_MODE, new ComboFormatDelegate(QStringList()<<" "<< Data::instance()->propagationModesIDList(), ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_DATE, new DateFormatDelegate());
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QRZCOM_QSO_UPLOAD_STATUS, new ComboFormatDelegate(uploadStatusEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSO_COMPLETE, new ComboFormatDelegate(qsoCompleteEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_QSO_RANDOM, new ComboFormatDelegate(boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_RX_PWR, new UnitFormatDelegate("W", 1, 0.1, ui->contactTable));
    /*https://www.pe0sat.vgnet.nl/satellite/sat-information/modes/ */
    /* use all possible values, do not use only modern modes in sat_modes.json */
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SAT_MODE, new ComboFormatDelegate(QStringList()<<" "<<"VU"<<"UV"<<"US"<<"LU"<<"LS"<<"LX"<<"VS"<<"K"<<"T"<<"A"<<"J"<<"B"<<"S"<<"L", ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SFI, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SILENT_KEY, new ComboFormatDelegate(boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SRX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_STX, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_SWL, new ComboFormatDelegate(boolEnum, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_TEN_TEN, new UnitFormatDelegate("", 0, 1, ui->contactTable));
    ui->contactTable->setItemDelegateForColumn(LogbookModel::COLUMN_UKSMG, new UnitFormatDelegate("", 0, 1, ui->contactTable));

    QSettings settings;
    QVariant logbookState = settings.value("logbook/state");
    if (!logbookState.isNull()) {
        ui->contactTable->horizontalHeader()->restoreState(logbookState.toByteArray());
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
        ui->contactTable->showColumn(LogbookModel::COLUMN_NAME);
        ui->contactTable->showColumn(LogbookModel::COLUMN_QTH);
        ui->contactTable->showColumn(LogbookModel::COLUMN_COMMENT);
    }

    ui->contactTable->horizontalHeader()->setSectionsMovable(true);

    ui->bandFilter->setModel(new SqlListModel("SELECT name FROM bands ORDER BY start_freq", "Band"));
    ui->modeFilter->setModel(new SqlListModel("SELECT name FROM modes", "Mode"));
    ui->countryFilter->setModel(new SqlListModel("SELECT id, name FROM dxcc_entities WHERE id IN (SELECT DISTINCT dxcc FROM contacts) ORDER BY name;", "Country"));
    ui->countryFilter->setModelColumn(1);

    clublog = new ClubLog(this);

    updateTable();
}

void LogbookWidget::filterSelectedCallsign() {
    FCT_IDENTIFICATION;

    QModelIndexList modeList= ui->contactTable->selectionModel()->selectedRows();
    if ( modeList.count() > 0 )
    {
        QSqlRecord record = model->record(modeList.first().row());
        filterCallsign(record.value("callsign").toString());
    }
}

void LogbookWidget::lookupSelectedCallsign() {
    FCT_IDENTIFICATION;

    QModelIndexList modeList = ui->contactTable->selectionModel()->selectedRows();
    if ( modeList.count() > 0)
    {

        QSqlRecord record = model->record(modeList.first().row());
        QString callsign = record.value("callsign").toString();
        QDesktopServices::openUrl(QString("https://www.qrz.com/lookup/%1").arg(callsign));
    }

}

void LogbookWidget::filterCallsign(QString call) {
    FCT_IDENTIFICATION;

    if ( !call.isEmpty() )
    {
       ui->callsignFilter->setText(call);
    }
    else
    {
       ui->callsignFilter->setText("");
    }
}

void LogbookWidget::callsignFilterChanged() {
    FCT_IDENTIFICATION;

    QString callsign = ui->callsignFilter->text();
    if (!callsign.isEmpty()) {
        model->setFilter(QString("callsign LIKE '%1%'").arg(ui->callsignFilter->text()));
    }
    else {
        model->setFilter(nullptr);
    }
    updateTable();
}

void LogbookWidget::bandFilterChanged() {
    FCT_IDENTIFICATION;

    QString band = ui->bandFilter->currentText();
    if (ui->bandFilter->currentIndex() != 0 && !band.isEmpty()) {
        model->setFilter(QString("band = '%1'").arg(band));
    }
    else {
        model->setFilter(nullptr);
    }
    updateTable();
}

void LogbookWidget::modeFilterChanged() {
    FCT_IDENTIFICATION;

    QString mode = ui->modeFilter->currentText();
    if (ui->modeFilter->currentIndex() != 0 && !mode.isEmpty()) {
        model->setFilter(QString("mode = '%1'").arg(mode));
    }
    else {
        model->setFilter(nullptr);
    }
    updateTable();
}

void LogbookWidget::countryFilterChanged() {
    FCT_IDENTIFICATION;

    QString country = ui->countryFilter->currentText();

    int row = ui->countryFilter->currentIndex();
    QModelIndex idx = ui->countryFilter->model()->index(row,0);
    QVariant data = ui->countryFilter->model()->data(idx);

    if ( ui->countryFilter->currentIndex() != 0 )
    {
        model->setFilter(QString("dxcc = '%1'").arg(data.toInt()));
    }
    else {
        model->setFilter(nullptr);
    }
    updateTable();
}

void LogbookWidget::uploadClublog() {
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);

    foreach (QModelIndex index, ui->contactTable->selectionModel()->selectedRows()) {
        QSqlRecord record = model->record(index.row());
        adi.exportContact(record);
    }

    stream.flush();

    clublog->uploadAdif(data);
}

void LogbookWidget::deleteContact() {
    FCT_IDENTIFICATION;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Delete"), tr("Delete the selected contacts?"),
                                  QMessageBox::Yes|QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    foreach (QModelIndex index, ui->contactTable->selectionModel()->selectedRows()) {
        model->removeRow(index.row());
    }
    ui->contactTable->clearSelection();
    updateTable();
}

void LogbookWidget::editContact()
{
    FCT_IDENTIFICATION;
    ui->contactTable->edit(ui->contactTable->selectionModel()->currentIndex());
}

void LogbookWidget::displayedColumns()
{
    FCT_IDENTIFICATION;

    ColumnSettingDialog dialog(ui->contactTable);

    dialog.exec();

    saveTableHeaderState();
}

void LogbookWidget::updateTable() {
    FCT_IDENTIFICATION;

    model->select();
    ui->contactTable->resizeColumnsToContents();
}

void LogbookWidget::saveTableHeaderState() {
    FCT_IDENTIFICATION;

    QSettings settings;
    QByteArray logbookState = ui->contactTable->horizontalHeader()->saveState();
    settings.setValue("logbook/state", logbookState);
}

void LogbookWidget::showTableHeaderContextMenu(const QPoint& point) {
    FCT_IDENTIFICATION;

    QMenu* contextMenu = new QMenu(this);
    for (int i = 0; i < model->columnCount(); i++) {
        QString name = model->headerData(i, Qt::Horizontal).toString();
        QAction* action = new QAction(name, contextMenu);
        action->setCheckable(true);
        action->setChecked(!ui->contactTable->isColumnHidden(i));

        connect(action, &QAction::triggered, [this, i]() {
            ui->contactTable->setColumnHidden(i, !ui->contactTable->isColumnHidden(i));
            saveTableHeaderState();
        });

        contextMenu->addAction(action);
    }

    contextMenu->exec(point);
}

LogbookWidget::~LogbookWidget() {
    FCT_IDENTIFICATION;

    saveTableHeaderState();
    delete ui;
}
