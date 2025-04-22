#include <QProgressDialog>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <ui/QSLImportStatDialog.h>
#include <QNetworkReply>

#include "Eqsldialog.h"
#include "ui_Eqsldialog.h"
#include "core/debug.h"
#include "service/eqsl/Eqsl.h"
#include "models/SqlListModel.h"
#include "logformat/AdiFormat.h"
#include "ui/ShowUploadDialog.h"
#include "data/StationProfile.h"

MODULE_IDENTIFICATION("qlog.ui.eqsldialog");

EqslDialog::EqslDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EqslDialog)
{
    FCT_IDENTIFICATION;
    ui->setupUi(this);

    /* Upload */
    ui->myCallsignCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                + ui->myCallsignCombo->currentText()
                                                + "' ORDER BY my_gridsquare", ""));

    ui->dateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->qslRadioButton->setChecked(true);
    ui->qsoRadioButton->setChecked(false);

    loadDialogState();
}

EqslDialog::~EqslDialog()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void EqslDialog::download()
{
    FCT_IDENTIFICATION;

    QProgressDialog* dialog = new QProgressDialog(tr("Downloading from eQSL"), tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setRange(0, 0);
    dialog->show();

    bool qsl = ui->qslRadioButton->isChecked();

    EQSL *eQSL = new EQSL(dialog);

    connect(eQSL, &EQSL::updateProgress, dialog, &QProgressDialog::setValue);

    connect(eQSL, &EQSL::updateStarted, this, [dialog]
    {
        dialog->setLabelText(tr("Processing eQSL QSLs"));
        dialog->setRange(0, 100);
    });

    connect(eQSL, &EQSL::updateComplete, this, [eQSL, dialog, qsl](QSLMergeStat stats)
    {
        if ( qsl )
        {
            QSettings settings;
            settings.setValue("eqsl/last_update", QDateTime::currentDateTimeUtc().date());
        }
        dialog->done(0);

        QSLImportStatDialog statDialog(stats);
        statDialog.exec();

        qCDebug(runtime) << "New QSLs: " << stats.newQSLs;
        qCDebug(runtime) << "Unmatched QSLs: " << stats.unmatchedQSLs;
        eQSL->deleteLater();
    });

    connect(eQSL, &EQSL::updateFailed, this, [this, eQSL, dialog](const QString &error)
    {
        dialog->done(1);
        QMessageBox::critical(this, tr("QLog Error"), tr("eQSL update failed: ") + error);
        eQSL->deleteLater();
    });

    connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
    {
        qCDebug(runtime)<< "Operation canceled";

        eQSL->abortRequest();
        eQSL->deleteLater();
    });

    /* do not call saveDialogState() here */
    /* we want to save only profile from download part */
    QSettings settings;
    settings.setValue("eqsl/last_QTHProfile", ui->qthProfileEdit->text());
    settings.setValue("eqsl/last_qsoqsl", ui->qslRadioButton->isChecked());

    eQSL->update(ui->dateEdit->date(),
                 ui->qsoRadioButton->isChecked(),
                 ui->qthProfileEdit->text());
}

void EqslDialog::upload()
{
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

    /* http://www.eqsl.cc/qslcard/ADIFContentSpecs.cfm */
    QString query_string = "SELECT start_time, callsign, mode, freq, band, "
                           "       prop_mode, rst_sent, submode, "
                           "       sat_mode, sat_name, "
                           "       my_cnty, my_gridsquare ";
    QString query_from   = "FROM contacts ";
    QString query_where =  QString("WHERE (upper(eqsl_qsl_sent) in (%1) OR eqsl_qsl_sent is NULL) ").arg(qslSentStatuses.join(","));
    QString query_order = " ORDER BY start_time ";

    saveDialogState();

    if ( ui->uploadQSLsMsgCheck->isChecked() )
    {
        query_string.append(", substr(qslmsg, 1, 240) as qslmsg ");
    }
    else if ( ui->uploadcommentCheck->isChecked() )
    {
        query_string.append(", substr(comment,1,240) as qslmsg ");
    }

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

    adi.exportStart();

    QMap<QString, QString> *applTags = nullptr;
    if ( !ui->qthProfileUploadEdit->text().isEmpty() )
    {
        applTags = new QMap<QString, QString>;
        applTags->insert("app_eqsl_qth_nickname", ui->qthProfileUploadEdit->text());
    }

    while (query.next())
    {
        QSqlRecord record = query.record();

        QSOList.append(" "
                       + record.value("start_time").toDateTime().toTimeZone(QTimeZone::utc()).toString(locale.formatDateTimeShortWithYYYY())
                       + "\t" + record.value("callsign").toString()
                       + "\t" + record.value("mode").toString()
                       + "\n");

        adi.exportContact(record, applTags);
        count++;
    }

    if ( applTags )
    {
        delete applTags;
    }

    stream.flush();

    if (count > 0)
    {
        ShowUploadDialog showDialog(QSOList);

        if ( showDialog.exec() == QDialog::Accepted )
        {
            QProgressDialog* dialog = new QProgressDialog(tr("Uploading to eQSL"), tr("Cancel"), 0, 0, this);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->setRange(0, 0);
            dialog->show();

            EQSL *eQSL = new EQSL(dialog);

            connect(eQSL, &EQSL::uploadOK, this, [this, eQSL, dialog, query_where, count](const QString &msg)
            {
                dialog->done(QDialog::Accepted);
                qCDebug(runtime) << "eQSL Upload OK: " << msg;
                QMessageBox::information(this, tr("QLog Information"), tr("%n QSO(s) uploaded.", "", count));
                QString query_string = "UPDATE contacts "
                                       "SET eqsl_qsl_sent='Y', eqsl_qslsdate = strftime('%Y-%m-%d',DATETIME('now', 'utc')) "
                        + query_where;

                qCDebug(runtime) << query_string;

                QSqlQuery query_update(query_string);
                query_update.exec();
                eQSL->deleteLater();
            });

            connect(eQSL, &EQSL::uploadError, this, [this, eQSL, dialog](const QString &msg)
            {
                dialog->done(QDialog::Accepted);
                qCInfo(runtime) << "eQSL Upload Error: " << msg;
                QMessageBox::warning(this, tr("QLog Warning"), tr("Cannot upload the QSO(s): ") + msg);
                eQSL->deleteLater();
            });

            connect(dialog, &QProgressDialog::canceled, this, [eQSL]()
            {
                qCDebug(runtime)<< "Operation canceled";
                eQSL->abortRequest();
                eQSL->deleteLater();
            });

            eQSL->uploadAdif(data);
        }
    }
    else
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No QSOs found to upload."));
    }
}

void EqslDialog::uploadCallsignChanged(const QString &my_callsign)
{
    FCT_IDENTIFICATION;

    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='" + my_callsign + "' ORDER BY my_gridsquare", ""));

}

void EqslDialog::saveDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("eqsl/last_QTHProfile", ui->qthProfileUploadEdit->text());
    settings.setValue("eqsl/last_mycallsign", ui->myCallsignCombo->currentText());
    settings.setValue("eqsl/last_mygrid", ui->myGridCombo->currentText());
    settings.setValue("eqsl/last_checkcomment", ui->uploadcommentCheck->isChecked());
    settings.setValue("eqsl/last_checkqslsmsg", ui->uploadQSLsMsgCheck->isChecked());
    settings.setValue("eqsl/last_qsoqsl", ui->qslRadioButton->isChecked());
}

void EqslDialog::loadDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    int index = ui->myCallsignCombo->findText(profile.callsign);

    if ( index >= 0 )
        ui->myCallsignCombo->setCurrentIndex(index);
    else
        ui->myCallsignCombo->setCurrentText(settings.value("eqsl/last_mycallsign").toString());

    index = ui->myGridCombo->findText(profile.locator);

    if ( index >= 0 )
        ui->myGridCombo->setCurrentIndex(index);
    else
        ui->myGridCombo->setCurrentText(settings.value("eqsl/last_mygrid").toString());

    ui->qthProfileUploadEdit->setText(settings.value("eqsl/last_QTHProfile").toString());
    ui->uploadcommentCheck->setChecked(settings.value("eqsl/last_checkcomment",false).toBool());
    ui->uploadQSLsMsgCheck->setChecked(settings.value("eqsl/last_checkqslsmsg",false).toBool());
    ui->qthProfileEdit->setText(settings.value("eqsl/last_QTHProfile").toString());

    ui->dateEdit->setDate(settings.value("eqsl/last_update", QDate(1900, 1, 1)).toDate());

    bool qsl = settings.value("eqsl/last_qsoqsl",1).toBool();
    ui->qslRadioButton->setChecked(qsl);
    ui->qsoRadioButton->setChecked(!qsl);
}

