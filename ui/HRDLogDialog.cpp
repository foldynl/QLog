#include <QSqlQuery>
#include <QSqlRecord>
#include <QProgressDialog>
#include <QMessageBox>
#include <QSqlError>

#include "HRDLogDialog.h"
#include "ui_HRDLogDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "ui/ShowUploadDialog.h"
#include "core/HRDLog.h"
#include "data/StationProfile.h"

MODULE_IDENTIFICATION("qlog.ui.hrdlogdialog");

HRDLogDialog::HRDLogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HRDLogDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    ui->myCallsignCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                + ui->myCallsignCombo->currentText()
                                                + "' ORDER BY my_gridsquare", ""));
    loadDialogState();
}

HRDLogDialog::~HRDLogDialog()
{
    delete ui;
}

void HRDLogDialog::upload()
{
    FCT_IDENTIFICATION;

    QString QSOList;
    int count = 0;
    QStringList qslUploadStatuses = {"'M'"};

    if ( ui->addlUploadStatusN->isChecked() )
    {
        qslUploadStatuses << "'N'";
    }

    // http://www.iw1qlh.net/projects/hrdlog/HRDLognet_4.pdf
    // It is not clear what QLog should send to HRDLog. Therefore it will
    // send all ADIF-fields
    QString query_string = "SELECT * ";
    QString query_from   = "FROM contacts ";
    QString query_where =  QString("WHERE (upper(hrdlog_qso_upload_status) in (%1) OR hrdlog_qso_upload_status is NULL) ").arg(qslUploadStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( !ui->myCallsignCombo->currentText().isEmpty() )
    {
        query_where.append(" AND station_callsign = '" + ui->myCallsignCombo->currentText() + "'");
    }

    if ( !ui->myGridCombo->currentText().isEmpty() )
    {
        query_where.append(" AND my_gridsquare = '" + ui->myGridCombo->currentText() + "'");
    }

    query_string = query_string + query_from + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);
    QList<QSqlRecord> qsos;

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        qsos.append(record);
    }

    count = qsos.count();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {
            QProgressDialog* dialog = new QProgressDialog(tr("Uploading to HRDLOG"),
                                                          tr("Cancel"),
                                                          0, count, this);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setValue(0);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->show();

            HRDLog *hrdlog = new HRDLog(dialog);

            connect(hrdlog, &HRDLog::uploadedQSO, this, [hrdlog, dialog](int qsoID)
            {
                QString query_string = "UPDATE contacts "
                                       "SET hrdlog_qso_upload_status='Y', hrdlog_qso_upload_date = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                                       "WHERE id = :qsoID";

                qCDebug(runtime) << query_string;

                QSqlQuery query_update;

                query_update.prepare(query_string);
                query_update.bindValue(":qsoID", qsoID);

                if ( ! query_update.exec() )
                {
                    qInfo() << "Cannot Update HRDLog status for QSO number " << qsoID << " " << query_update.lastError().text();
                    hrdlog->abortRequest();
                    hrdlog->deleteLater();
                }
                dialog->setValue(dialog->value() + 1);
            });

            connect(hrdlog, &HRDLog::uploadFinished, this, [this, hrdlog, dialog, count](bool)
            {
                dialog->done(QDialog::Accepted);
                QMessageBox::information(this, tr("QLog Information"),
                                         tr("%n QSO(s) uploaded.", "", count));
                hrdlog->deleteLater();
            });

            connect(hrdlog, &HRDLog::uploadError, this, [this, hrdlog, dialog](const QString &msg)
            {
                dialog->done(QDialog::Accepted);
                qCInfo(runtime) << "HRDLog.com Upload Error: " << msg;
                QMessageBox::warning(this, tr("QLog Warning"),
                                     tr("Cannot upload the QSO(s): ") + msg);
                hrdlog->deleteLater();
            });

            connect(dialog, &QProgressDialog::canceled, hrdlog, &HRDLog::abortRequest);

            hrdlog->uploadContacts(qsos);
        }
    }
    else
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No QSOs found to upload."));
    }
}

void HRDLogDialog::uploadCallsignChanged(const QString &my_callsign)
{
    FCT_IDENTIFICATION;

    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='" + my_callsign + "' ORDER BY my_gridsquare", ""));
}

void HRDLogDialog::saveDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue("hrdlog/last_mycallsign", ui->myCallsignCombo->currentText());
    settings.setValue("hrdlog/last_mygrid", ui->myGridCombo->currentText());
}

void HRDLogDialog::loadDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    int index = ui->myCallsignCombo->findText(profile.callsign);

    if ( index >= 0 )
        ui->myCallsignCombo->setCurrentIndex(index);
    else
        ui->myCallsignCombo->setCurrentText(settings.value("hrdlog/last_mycallsign").toString());

    index = ui->myGridCombo->findText(profile.locator);

    if ( index >= 0 )
        ui->myGridCombo->setCurrentIndex(index);
    else
        ui->myGridCombo->setCurrentText(settings.value("hrdlog/last_mygrid").toString());
}
