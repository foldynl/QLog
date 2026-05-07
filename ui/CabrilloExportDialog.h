#ifndef QLOG_UI_CABRILLOEXPORTDIALOG_H
#define QLOG_UI_CABRILLOEXPORTDIALOG_H

#include <QDialog>
#include "logformat/CabrilloFormat.h"

class QSqlQuery;
class QTimer;

namespace Ui {
class CabrilloExportDialog;
}

class CabrilloExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CabrilloExportDialog(QWidget *parent = nullptr);
    ~CabrilloExportDialog();

private slots:
    void accept() override;
    void templateChanged(int index);
    void manageTemplates();
    void browseFile();
    void callsignChanged(const QString &callsign);
    void refreshCallsigns();
    void updateQsoCount();
    void scheduleQsoCountUpdate();

private:
    void loadTemplates();
    void loadStationProfile(const QString &callsign);
    void resolveBandFilter();
    CabrilloFormat::HeaderData buildHeaderData() const;
    QString buildWhereClause() const;
    void bindWhereClause(QSqlQuery &query) const;

    Ui::CabrilloExportDialog *ui;
    QTimer *countDebounceTimer;
    double bandStartFreq;
    double bandEndFreq;
    bool bandFilterActive;
};

#endif // QLOG_UI_CABRILLOEXPORTDIALOG_H
