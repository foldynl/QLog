#include <QTextStream>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QNetworkReply>

#include "LotwDialog.h"
#include "ui_LotwDialog.h"
#include "logformat/AdiFormat.h"
#include "core/Lotw.h"
#include "core/debug.h"
#include "ui/QSLImportStatDialog.h"
#include "models/SqlListModel.h"
#include "ui/ShowUploadDialog.h"
#include "data/StationProfile.h"

MODULE_IDENTIFICATION("qlog.ui.lotwdialog");

LotwDialog::LotwDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LotwDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    /* Upload */

    ui->myCallsignCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                + ui->myCallsignCombo->currentText()
                                                + "' ORDER BY my_gridsquare", ""));
    /* Download */
    ui->qslRadioButton->setChecked(true);
    ui->qsoRadioButton->setChecked(false);

    ui->stationCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));

    ui->dateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    loadDialogState();
}

void LotwDialog::download() {
    FCT_IDENTIFICATION;

    QProgressDialog* dialog = new QProgressDialog(tr("Downloading from LotW"), tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setRange(0, 0);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();

    bool qsl = ui->qslRadioButton->isChecked();

    Lotw* lotw = new Lotw(dialog);
    connect(lotw, &Lotw::updateProgress, dialog, &QProgressDialog::setValue);

    connect(lotw, &Lotw::updateStarted, this, [dialog] {
        dialog->setLabelText(tr("Processing LotW QSLs"));
        dialog->setRange(0, 100);
    });

    connect(lotw, &Lotw::updateComplete, this, [dialog, qsl, lotw](QSLMergeStat stats) {
        if (qsl) {
            QSettings settings;
            settings.setValue("lotw/last_update", QDateTime::currentDateTimeUtc().date());
        }
        dialog->done(QDialog::Accepted);

        QSLImportStatDialog statDialog(stats);
        statDialog.exec();
        lotw->deleteLater();
    });

    connect(lotw, &Lotw::updateFailed, this, [this, dialog, lotw](const QString &error) {
        dialog->done(QDialog::Accepted);
        QMessageBox::critical(this, tr("QLog Error"), tr("LoTW Update failed: ") + error);
        lotw->deleteLater();
    });

    connect(dialog, &QProgressDialog::canceled, this, [lotw]()
    {
        qCDebug(runtime)<< "Operation canceled";
        lotw->abortRequest();
        lotw->deleteLater();
    });

    saveDialogState();

    lotw->update(ui->dateEdit->date(), ui->qsoRadioButton->isChecked(), ui->stationCombo->currentText().toUpper());
}

void LotwDialog::upload() {
    FCT_IDENTIFICATION;

    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);

    AdiFormat adi(stream);
    QString QSOList;
    int count = 0;

    QStringList qslSentStatuses = {"'R'", "'Q'"};

    if ( ui->addlSentStatusI->isChecked() )
    {
        qslSentStatuses << "'I'";
    }

    if ( ui->addlSentStatusN->isChecked() )
    {
        qslSentStatuses << "'N'";
    }

    QString query_string = "SELECT callsign, freq, band, freq_rx, "
                           "       mode, submode, start_time, prop_mode, "
                           "       sat_name, station_callsign, operator, "
                           "       rst_sent, rst_rcvd, my_state, my_cnty, "
                           "       my_vucc_grids "
                           "FROM contacts ";
    QString query_where =  QString("WHERE (upper(lotw_qsl_sent) in (%1) OR lotw_qsl_sent is NULL) "
                           "               AND (upper(prop_mode) NOT IN ('INTERNET', 'RPT', 'ECH', 'IRL') OR prop_mode IS NULL) ").arg(qslSentStatuses.join(","));
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

    query_string = query_string + query_where + query_order;

    qCDebug(runtime) << query_string;

    QSqlQuery query(query_string);

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeSpec(Qt::UTC).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        adi.exportContact(record);
        count++;
    }

    stream.flush();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {

            QProgressDialog* dialog = new QProgressDialog(tr("Processing LoTW"),
                                                          "",
                                                          0, 0, this);
            dialog->setCancelButton(nullptr);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setAttribute(Qt::WA_DeleteOnClose, true);
            dialog->setAutoClose(true);
            dialog->setMinimumDuration(0);
            dialog->show();

            Lotw *lotw = new Lotw(dialog);

            connect(lotw, &Lotw::uploadFinished, this, [this, lotw, dialog, query_where, count]()
            {
                dialog->done(QDialog::Accepted);

                QMessageBox::information(this, tr("QLog Information"), tr("%n QSO(s) uploaded.", "", count));

                QString queryString = "UPDATE contacts "
                               "SET lotw_qsl_sent='Y', lotw_qslsdate = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                               + query_where;

                qCDebug(runtime) << queryString;

                QSqlQuery query_update(queryString);
                if ( ! query_update.exec() )
                {
                    qWarning() << "Cannot execute update query" << query_update.lastError().text();
                    return;
                }
                lotw->deleteLater();
            });

            connect(lotw, &Lotw::uploadError, this, [this, lotw, dialog](const QString &errorString)
            {
                qInfo() << "Error" << errorString;
                dialog->done(QDialog::Accepted);
                qCInfo(runtime) << "LoTW Upload Error: " << errorString;
                QMessageBox::warning(this, tr("QLog Warning"),
                                     tr("Cannot upload the QSO(s): ") + errorString);
                lotw->deleteLater();
            });

            lotw->uploadAdif(data);
        }
    }
    else {
        QMessageBox::information(this, tr("QLog Information"), tr("No QSOs found to upload."));
    }
}

void LotwDialog::uploadCallsignChanged(const QString &my_callsign)
{
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='" + my_callsign + "' ORDER BY my_gridsquare", ""));
}

void LotwDialog::saveDialogState()
{
    FCT_IDENTIFICATION;
    QSettings settings;
    settings.setValue("lotw/last_mycallsign", ui->myCallsignCombo->currentText());
    settings.setValue("lotw/last_mygrid", ui->myGridCombo->currentText());
    settings.setValue("lotw/last_station", ui->stationCombo->currentText());
    settings.setValue("lotw/last_qsoqsl", ui->qslRadioButton->isChecked());
}

void LotwDialog::loadDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    int index = ui->myCallsignCombo->findText(profile.callsign);

    if ( index >= 0 )
        ui->myCallsignCombo->setCurrentIndex(index);
    else
        ui->myCallsignCombo->setCurrentText(settings.value("lotw/last_mycallsign").toString());

    index = ui->myGridCombo->findText(profile.locator);

    if ( index >= 0 )
        ui->myGridCombo->setCurrentIndex(index);
    else
        ui->myGridCombo->setCurrentText(settings.value("lotw/last_mygrid").toString());

    ui->dateEdit->setDate(settings.value("lotw/last_update", QDate(1900, 1, 1)).toDate());
    ui->qslRadioButton->setChecked(settings.value("lotw/last_qsoqsl",0).toBool());
    ui->qsoRadioButton->setChecked(!settings.value("lotw/last_qsoqsl",0).toBool());
}

LotwDialog::~LotwDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}
