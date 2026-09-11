#ifndef QSOFILTERMANAGER_H
#define QSOFILTERMANAGER_H

#include <QObject>
#include <QSqlQuery>
#include <QDateTime>

#include "models/LogbookModel.h"
#include "models/SqlListModel.h"

struct QSOFilterRule
{
    // Existing operator IDs are persisted and must never be renumbered.
    // based on DB values
    enum Operator
    {
        Equal = 0,
        NotEqual = 1,
        Contains = 2,
        NotContains = 3,
        GreaterThan = 4,
        LessThan = 5,
        StartsWith = 6,
        RegExp = 7,
        InDateRange = 8,
        OutsideDateRange = 9,
        BeforeDateRange = 10,
        AfterDateRange = 11
    };
    QSOFilterRule() = default;
    QSOFilterRule(int in_idx, int in_operatorID, const QString &in_value)
        : tableFieldIndex(in_idx),
        operatorID(in_operatorID),
        value(in_value){ };
    int tableFieldIndex = -1;
    int operatorID = Equal;
    QString value;

    bool isDateRange() const { return operatorID >= InDateRange && operatorID <= AfterDateRange; }
    bool operator==(const QSOFilterRule &other) const
    {
        return tableFieldIndex == other.tableFieldIndex && operatorID == other.operatorID
               && value == other.value && value.isNull() == other.value.isNull();
    }
};

class QSOFilter
{
public:
    enum Matching
    {
        All = 0,
        Any = 1
    };

    QString filterName;
    int machingType;
    QList<QSOFilterRule> rules;

    QSOFilter() : machingType(All){};

    void addRule(const QSOFilterRule &rule)
    {
        rules.append(rule);
    }

    static QSOFilterRule createFromDateRule(const QDateTime &date)
    {
        return QSOFilterRule(LogbookModel::COLUMN_TIME_ON, QSOFilterRule::GreaterThan, date.toString("yyyy-MM-ddTHH:mm:ss"));
    }

    static QSOFilterRule createNonEmptyContestRule(const QString &contestID)
    {
        return QSOFilterRule(LogbookModel::COLUMN_CONTEST_ID, QSOFilterRule::Contains, contestID);
    }

    static QSOFilterRule createToDateRule(const QDateTime &date)
    {
        return QSOFilterRule(LogbookModel::COLUMN_TIME_ON, QSOFilterRule::LessThan, date.toString("yyyy-MM-ddTHH:mm:ss"));
    }

    static QSOFilter createFromDateContestFilter(const QString &contestID, const QDateTime &date)
    {
        QSOFilter ret;

        ret.filterName = QString("%1-%2").arg(contestID, date.toString("yyyy/MM/dd hh:mm"));
        ret.machingType = All;
        ret.addRule(createFromDateRule(date));
        ret.addRule(createNonEmptyContestRule(contestID));
        return ret;
    }
};

class QSOFilterManager : public QObject
{
    Q_OBJECT
public:

    static QSOFilterManager * instance()
    {
        static QSOFilterManager instance;
        return &instance;
    }

    static QString getWhereClause(const QString &filterName, const QString &columnPrefix = {});
    static QString getWhereClause(const QSOFilter &filter, const QString &columnPrefix = {},
                                  const QDate &today = QDateTime::currentDateTimeUtc().date());
    static SqlListModel* QSOFilterModel(const QString &firstValue, QObject *parent = nullptr);
    bool save(const QSOFilter &filter);
    bool remove(const QString &filterName);
    QStringList getFilterList() const;
    QSOFilter getFilter(const QString &filterName) const;

private:
    QSOFilterManager(QObject *parent = nullptr);

    bool replaceFilter(const QString &filterName, const int matchingType);
    bool insertFilterRule(const QString &filterName, const QSOFilterRule &rule);
    bool deleteFilterRules(const QString &filterName);

    bool stmtsReady;
    QSqlQuery insertRuleStmt;
    QSqlQuery insertFilterStmt;
    QSqlQuery deleteFilterStmt;
};

#endif // QSOFILTERMANAGER_H
