#include "multiqsoupload.h"
#include "ui_multiqsoupload.h"

#include <QTextStream>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QNetworkReply>

#include "LotwDialog.h"
#include "logformat/AdiFormat.h"
#include "core/Lotw.h"
#include "core/debug.h"
#include "ui/QSLImportStatDialog.h"
#include "models/SqlListModel.h"
#include "ui/ShowUploadDialog.h"
#include "data/StationProfile.h"

MultiQSOUpload::MultiQSOUpload(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MultiQSOUpload)
{
    ui->setupUi(this);

    /* Upload */

    ui->myCallsignCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign", ""));
    ui->myGridCombo->setModel(new SqlListModel("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts WHERE station_callsign ='"
                                                   + ui->myCallsignCombo->currentText()
                                                   + "' ORDER BY my_gridsquare", ""));
    loadDialogState();
}

MultiQSOUpload::~MultiQSOUpload()
{
    delete ui;
}

void MultiQSOUpload::on_uploadButton_clicked()
{

}


void MultiQSOUpload::on_cancelButton_clicked()
{

}

void MultiQSOUpload::saveDialogState()
{
    FCT_IDENTIFICATION;
    QSettings settings;
    settings.setValue("MultiQSOUpload/last_mycallsign", ui->myCallsignCombo->currentText());
    settings.setValue("MultiQSOUpload/last_mygrid", ui->myGridCombo->currentText());
   // settings.setValue("lotw/last_station", ui->stationCombo->currentText());
   // settings.setValue("lotw/last_qsoqsl", ui->qslRadioButton->isChecked());
}

void MultiQSOUpload::loadDialogState()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    int index = ui->myCallsignCombo->findText(profile.callsign);

    if ( index >= 0 )
        ui->myCallsignCombo->setCurrentIndex(index);
    else
        ui->myCallsignCombo->setCurrentText(settings.value("MultiQSOUpload/last_mycallsign").toString());

    index = ui->myGridCombo->findText(profile.locator);

    if ( index >= 0 )
        ui->myGridCombo->setCurrentIndex(index);
    else
        ui->myGridCombo->setCurrentText(settings.value("MultiQSOUpload/last_mygrid").toString());

   // ui->dateEdit->setDate(settings.value("lotw/last_update", QDate(1900, 1, 1)).toDate());
   // ui->qslRadioButton->setChecked(settings.value("lotw/last_qsoqsl",0).toBool());
   // ui->qsoRadioButton->setChecked(!settings.value("lotw/last_qsoqsl",0).toBool());
}
