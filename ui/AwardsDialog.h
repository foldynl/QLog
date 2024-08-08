#ifndef QLOG_UI_AWARDSDIALOG_H
#define QLOG_UI_AWARDSDIALOG_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QComboBox>
#include "models/AwardsTableModel.h"
#include "models/SqlListModel.h"

namespace Ui {
class AwardsDialog;
}

class AwardsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AwardsDialog(QWidget *parent = nullptr);
    ~AwardsDialog();
public slots:
    void refreshTable(int);
    void awardTableDoubleClicked(QModelIndex);

signals:
    void AwardConditionSelected(QString, QString, QString);

private slots:
    void on_stationComboBox_currentIndexChanged(int index);

private:
    Ui::AwardsDialog *ui;
    AwardsTableModel *detailedViewModel;
    SqlListModel* entityCallsignModel;
    SqlListModel* stationCallsignModel;

    QString getSelectedEntity();
    QString getSelectedAward();
    QString getSelectedCallsign();
};

#endif // QLOG_UI_AWARDSDIALOG_H
