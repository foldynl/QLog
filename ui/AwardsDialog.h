#ifndef QLOG_UI_AWARDSDIALOG_H
#define QLOG_UI_AWARDSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include "awards/AwardDefinition.h"
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

signals:
    void awardConditionSelected(QString, QString, QString);

private:
    Ui::AwardsDialog *ui;
    QList<AwardDefinition*> m_awards;
    SqlListModel* entityCallsignModel;

    AwardDefinition* currentAward() const;
    AwardFilterParams buildFilterParams() const;
    QString getSelectedEntity() const;
    void setEntityInputEnabled(bool);
    void setNotWorkedEnabled(bool);

    static QList<AwardDefinition*> createAwards();
};

#endif // QLOG_UI_AWARDSDIALOG_H
