#ifndef QLOG_AWARDS_AWARDPGAHUNTER_H
#define QLOG_AWARDS_AWARDPGAHUNTER_H

#include <QSqlQueryModel>
#include "AwardDefinition.h"

class AwardPGAHunter : public AwardDefinition
{
public:
    QString key() const override { return QStringLiteral("pga_hunter"); }
    QString displayName() const override;
    QString rulesUrl() const override;
    bool entityInputEnabled() const override { return false; }
    // PGA credits are confirmed from activator logs submitted to SPGA, not
    // from QSL cards, LoTW or eQSL. QLog only shows local worked/own progress,
    // so a QSL-based "Confirmed" column would misrepresent official PGA status.
    bool confirmationInputEnabled() const override { return false; }
    QWidget* createWidget(QWidget *parent) override;
    void updateData(const AwardFilterParams &params) override;
    ConditionResult getConditionSelected(const QModelIndex &index) const override;

private:
    friend class PGAHunterTest;

    class TableModel : public QSqlQueryModel
    {
    public:
        using QSqlQueryModel::QSqlQueryModel;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    };

    QSqlQueryModel *model = nullptr;
    QString currentContactFilter;

    static QString eligibleContactCondition(const QString &referenceColumn);
    QString sqlQuery(bool notWorkedOnly) const;
};

#endif
