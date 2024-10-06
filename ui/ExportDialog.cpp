#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QSqlError>
#include "ui/ExportDialog.h"
#include "ui_ExportDialog.h"

#include "core/debug.h"
#include "models/SqlListModel.h"
#include "data/StationProfile.h"
#include "ui/ColumnSettingDialog.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.ui.exportdialog");

ExportDialog::ExportDialog(QWidget *parent) :
   ExportDialog(QList<QSqlRecord>(), parent)
{
    FCT_IDENTIFICATION;

    this->setWindowTitle(tr("Export QSOs"));

    ui->myCallsignComboBox->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));
    ui->myCallsignComboBox->setCurrentText(StationProfilesManager::instance()->getCurProfile1().callsign.toUpper());
    ui->myGridComboBox->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                + ui->myCallsignComboBox->currentText()
                                                + "' ORDER BY my_gridsquare", ""));
    ui->startDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->startDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->endDateEdit->setDate(QDate::currentDate().addDays(1));
}

ExportDialog::ExportDialog(const QList<QSqlRecord>& qsos, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportDialog),
    qsos4export(qsos)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Export"));

    if ( qsos4export.size() > 0 )
    {
        ui->filterGroup->setVisible(false);
        ui->addlSentStatusCheckbox->setVisible(false);
        ui->addlSentStatusICheckBox->setVisible(false);
        ui->addlSentStatusNCheckBox->setVisible(false);
        ui->addlSentStatusAlreadySentCheckBox->setVisible(false);
        adjustSize();
    }

    fillQSLSendViaCombo();
    fillExportTypeCombo();
    fillExportedColumnsCombo();
    // not posibility to define Exported Columns here
}

void ExportDialog::browse()
{
    FCT_IDENTIFICATION;

    const QString &lastPath = ( ui->fileEdit->text().isEmpty() ) ? settings.value("export/last_path", QDir::homePath()).toString()
                                                                 : ui->fileEdit->text();

    QString filename = QFileDialog::getSaveFileName(this, nullptr, lastPath);
    if ( !filename.isEmpty() )
    {
        settings.setValue("export/last_path", QFileInfo(filename).path());
        ui->fileEdit->setText(filename);
    }
}

void ExportDialog::toggleDateRange()
{
    FCT_IDENTIFICATION;

    ui->startDateEdit->setEnabled(ui->dateRangeCheckBox->isChecked());
    ui->endDateEdit->setEnabled(ui->dateRangeCheckBox->isChecked());
}

void ExportDialog::toggleMyCallsign()
{
    FCT_IDENTIFICATION;
    ui->myCallsignComboBox->setEnabled(ui->myCallsignCheckBox->isChecked());
}

void ExportDialog::toggleMyGridsquare()
{
    FCT_IDENTIFICATION;
    ui->myGridComboBox->setEnabled(ui->myGridCheckBox->isChecked());
}

void ExportDialog::toggleQslSendVia()
{
    FCT_IDENTIFICATION;
    ui->qslSendViaComboBox->setEnabled(ui->qslSendViaCheckbox->isChecked());
}

void ExportDialog::toggleSentStatus()
{
    FCT_IDENTIFICATION;
    ui->addlSentStatusICheckBox->setEnabled(ui->addlSentStatusCheckbox->isChecked());
    ui->addlSentStatusNCheckBox->setEnabled(ui->addlSentStatusCheckbox->isChecked());
    ui->addlSentStatusAlreadySentCheckBox->setEnabled(ui->addlSentStatusCheckbox->isChecked());
}

void ExportDialog::runExport()
{
    FCT_IDENTIFICATION;

    if ( ui->fileEdit->text().isEmpty() )
    {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                             QMessageBox::tr("Filename is empty"));
        return;
    }

    QFile file(ui->fileEdit->text());

    if ( ! file.open(QFile::WriteOnly | QFile::Text) )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                             QMessageBox::tr("Cannot write to the file"));
        return;
    }

    QTextStream out(&file);

    LogFormat* format = LogFormat::open(ui->typeSelect->currentText(), out);

    if (!format)
    {
        qCritical() << "unknown log format";
        return;
    }

    if ( ui->dateRangeCheckBox->isChecked() )
    {
        format->setFilterDateRange(ui->startDateEdit->date(), ui->endDateEdit->date());
    }

    if ( ui->myCallsignCheckBox->isChecked() )
    {
        format->setFilterMyCallsign(ui->myCallsignComboBox->currentText());
    }

    if ( ui->myGridCheckBox->isChecked() )
    {
        format->setFilterMyGridsquare(ui->myGridComboBox->currentText());
    }

    if ( ui->exportTypeCombo->currentData() == "qsl"
         && ui->qslSendViaCheckbox->isChecked() )
    {
        format->setFilterSendVia(ui->qslSendViaComboBox->currentData().toString());
    }

    if ( ui->exportTypeCombo->currentData() == "qsl" )
    {
        format->setFilterSentPaperQSL((ui->addlSentStatusCheckbox->isChecked()) ? ui->addlSentStatusNCheckBox->isChecked() : false,
                                      (ui->addlSentStatusCheckbox->isChecked()) ? ui->addlSentStatusICheckBox->isChecked() : false,
                                      (ui->addlSentStatusCheckbox->isChecked()) ? ui->addlSentStatusAlreadySentCheckBox->isChecked() : false);
    }

    format->setFilterQSOFilter = ui->applyQSOFilterCheckbox->isChecked();

    long count = 0L;

    connect(format, &LogFormat::exportProgress, this, &ExportDialog::setProgress);

    ui->buttonBox->setEnabled(false);
    ui->fileEdit->setEnabled(false);
    ui->typeSelect->setEnabled(false);
    ui->browseButton->setEnabled(false);
    ui->startDateEdit->setEnabled(false);
    ui->endDateEdit->setEnabled(false);
    ui->dateRangeCheckBox->setEnabled(false);
    ui->myCallsignCheckBox->setEnabled(false);
    ui->myCallsignComboBox->setEnabled(false);
    ui->myGridCheckBox->setEnabled(false);
    ui->myGridComboBox->setEnabled(false);
    ui->addlSentStatusICheckBox->setEnabled(false);
    ui->addlSentStatusNCheckBox->setEnabled(false);
    ui->addlSentStatusCheckbox->setEnabled(false);
    ui->addlSentStatusAlreadySentCheckBox->setEnabled(false);
    ui->exportedColumnsButton->setEnabled(false);
    ui->exportTypeCombo->setEnabled(false);
    ui->qslSendViaCheckbox->setEnabled(false);
    ui->qslSendViaComboBox->setEnabled(false);
    ui->markAsSentCheckbox->setEnabled(false);
    ui->exportedColumnsCombo->setEnabled(false);
    ui->applyQSOFilterCheckbox->setEnabled(false);

    if ( exportedColumns.count() > 0 )
    {
        //translate column indexes to SQL column names
        QSetIterator<int> i(exportedColumns);
        QSqlRecord record = logbookmodel.record();
        QStringList fields;
        while ( i.hasNext() )
        {
            fields << record.fieldName(i.next());
        }
        format->setExportedFields(fields);
    }

    if ( qsos4export.size() > 0 )
    {
        count = format->runExport(qsos4export);
    }
    else
    {
        count = format->runExport();

        if ( count > 0
             && ui->exportTypeCombo->currentData() == "qsl"
             && ui->markAsSentCheckbox->isChecked() )
        {
            if ( ! markQSOAsSent(format) )
            {
                QMessageBox::warning(this,
                                    tr("QLog Error"),
                                    tr("Cannot mark exported QSOs as Sent"));
            }
        }
    }

    delete format;

    QMessageBox::information(nullptr, QMessageBox::tr("QLog Information"),
                         QMessageBox::tr("Exported %n contact(s).", "", count));

    accept();
}

void ExportDialog::myCallsignChanged(const QString &myCallsign)
{
    FCT_IDENTIFICATION;

    ui->myGridComboBox->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                  + myCallsign + "' ORDER BY my_gridsquare", ""));
}

void ExportDialog::showColumnsSetting()
{
    FCT_IDENTIFICATION;

    // don't want to export QSO ID because it is not a ADIF field
    QList<LogbookModel::column_id> excludeFilter({LogbookModel::COLUMN_ID});

    ColumnSettingDialog dialog(&logbookmodel,
                               exportedColumns,
                               this,
                               excludeFilter);
    connect(&dialog, &ColumnSettingDialog::columnChanged,
            this, &ExportDialog::exportedColumnStateChanged);
    dialog.exec();
}

void ExportDialog::exportedColumnStateChanged(int index, bool state)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << index << state;

    if ( state )
    {
        exportedColumns.insert(index);
    }
    else
    {
        exportedColumns.remove(index);
    }
    QString comboValue = ui->exportedColumnsCombo->itemData(ui->exportedColumnsCombo->currentIndex()).toString();
    settings.setValue("export/" + comboValue, QVariant::fromValue(exportedColumns));
}
void ExportDialog::fillExportTypeCombo()
{
    FCT_IDENTIFICATION;

    ui->exportTypeCombo->addItem(tr("Generic"), "generic");

    if ( qsos4export.size() == 0 )
    {
        ui->exportTypeCombo->addItem(tr("QSLs"), "qsl");
    }
}

void ExportDialog::exportTypeChanged(int index)
{
    FCT_IDENTIFICATION;

    QString comboValue = ui->exportTypeCombo->itemData(index).toString();

    if ( comboValue != "qsl" )
    {
        ui->addlSentStatusCheckbox->setVisible(false);
        ui->addlSentStatusICheckBox->setVisible(false);
        ui->addlSentStatusNCheckBox->setVisible(false);
        ui->addlSentStatusAlreadySentCheckBox->setVisible(false);
        ui->qslSendViaCheckbox->setVisible(false);
        ui->qslSendViaComboBox->setVisible(false);
        ui->markAsSentCheckbox->setVisible(false);
        ui->exportedColumnsCombo->setCurrentIndex(ui->exportedColumnsCombo->findData("all"));

    }
    else
    {
        ui->addlSentStatusCheckbox->setVisible(true);
        ui->addlSentStatusICheckBox->setVisible(true);
        ui->addlSentStatusNCheckBox->setVisible(true);
        ui->addlSentStatusAlreadySentCheckBox->setVisible(true);
        ui->qslSendViaCheckbox->setVisible(true);
        ui->qslSendViaComboBox->setVisible(true);
        ui->markAsSentCheckbox->setVisible(true);
        ui->exportedColumnsCombo->setCurrentIndex(ui->exportedColumnsCombo->findData("qsl"));
    }

    adjustSize();
}

void ExportDialog::fillExportedColumnsCombo()
{
    FCT_IDENTIFICATION;

    ui->exportedColumnsCombo->addItem(tr("All"), "all");
    ui->exportedColumnsCombo->addItem(tr("Minimal"), "min");
    ui->exportedColumnsCombo->addItem(tr("QSL-specific"), "qsl");
    ui->exportedColumnsCombo->addItem(tr("Custom 1"), "c1");
    ui->exportedColumnsCombo->addItem(tr("Custom 2"), "c2");
    ui->exportedColumnsCombo->addItem(tr("Custom 3"), "c3");
}

bool ExportDialog::markQSOAsSent(LogFormat *format)
{
    FCT_IDENTIFICATION;

    if ( !format )
    {
        return false;
    }

    QString updateQuery = "UPDATE contacts "
                          "SET qsl_sent='Y', qsl_sdate = strftime('%Y-%m-%d',DATETIME('now', 'utc')) WHERE "
                          + format->getWhereClause();

    qCDebug(runtime) << updateQuery ;

    QSqlQuery query_update;

    if ( ! query_update.prepare(updateQuery) )
    {
        qWarning() << "Cannot mark exported QSO as Sent - prepare error " << query_update.lastError().text();
        return false;
    }

    format->bindWhereClause(query_update);

    if ( ! query_update.exec() )
    {
        qWarning() << "Cannot mark exported QSO as Sent - execute error " << query_update.lastError().text();
        return false;
    }

    return true;
}

void ExportDialog::exportedColumnsComboChanged(int index)
{
    FCT_IDENTIFICATION;

    QString comboValue = ui->exportedColumnsCombo->itemData(index).toString();

    //empty set means all values exported
    exportedColumns = QSet<int>();

    if ( comboValue == "all" )
    {
        ui->exportedColumnsButton->setEnabled(false);
    }
    else
    {
        ui->exportedColumnsButton->setEnabled(true);

        if ( comboValue == "min"
             || comboValue == "c1"
             || comboValue == "c2"
             || comboValue == "c3" )
        {
            exportedColumns = settings.value("export/" + comboValue, QVariant::fromValue(minColumns)).value<QSet<int>>();
        }
        else if ( comboValue == "qsl" )
        {
            exportedColumns = settings.value("export/" + comboValue, QVariant::fromValue(qslColumns)).value<QSet<int>>();
        }
    }
}

void ExportDialog::setProgress(float progress)
{
    FCT_IDENTIFICATION;

    ui->progressBar->setValue(progress);
    QCoreApplication::processEvents();
}

void ExportDialog::fillQSLSendViaCombo()
{
    FCT_IDENTIFICATION;

    QMapIterator<QString, QString> iter(Data::instance()->qslSentViaEnum);
    int iter_index = 0;
    while ( iter.hasNext() )
    {
        iter.next();
        ui->qslSendViaComboBox->addItem(iter.value(), iter.key());
        iter_index++;
    }
}

ExportDialog::~ExportDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}
