#ifndef QLOG_UI_IMPORTDIALOG_H
#define QLOG_UI_IMPORTDIALOG_H

#include <QDialog>
#include <QSqlRecord>
#include <logformat/LogFormat.h>
#include "data/StationProfile.h"
#include "core/LogLocale.h"

class QComboBox;

namespace Ui {
class ImportDialog;
}

class ImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportDialog(QWidget *parent = 0);
    ~ImportDialog();

private slots:
    void browse();
    void toggleAll();
    void toggleComment();
    void runImport();
    void computeProgress(qint64 position);
    void stationProfileTextChanged(const QString&);
    void rigProfileTextChanged(const QString&);
    void toggleMyProfile();
    void toggleMyRig();
    void commentChanged(const QString&);
    void qslSentStatusDefaultChanged(int);

private:
    Ui::ImportDialog *ui;
    qint64 size;
    StationProfile selectedStationProfile;
    LogLocale locale;

    static LogFormat::duplicateQSOBehaviour showDuplicateDialog(QSqlRecord *, QSqlRecord *);

    static void setQslSentStatusValue(QComboBox *, const QString &);

    void setupQslSentStatusComboData();
    void updateQslSentStatusDefaultWidgets();
    void loadQslSentStatusCustomDefaults();
    void saveQslSentStatusCustomDefaults() const;
    void updateWidgetSize();
    void saveImportDetails(const QString &importDetail,
                           const QString &filename,
                           const int count,
                           const unsigned long warnings,
                           const unsigned long errors);
};

#endif // QLOG_UI_IMPORTDIALOG_H
