#ifndef QSOFILTERMANAGER_H
#define QSOFILTERMANAGER_H

#include <QObject>
#include <QSqlQuery>
#include <QDateTime>

#include "models/LogbookModel.h"

struct QSOFilterRule
{
    QSOFilterRule() {};
    QSOFilterRule(int in_idx, int in_operatorID, const QString &in_value)
        : tableFieldIndex(in_idx),
        operatorID(in_operatorID),
        value(in_value){ };
    int tableFieldIndex;
    int operatorID;
    QString value;
};

class QSOFilter
{
public:
    QString filterName;
    int machingType;
    QList<QSOFilterRule> rules;

    QSOFilter() : machingType(0){};

    void addRule(const QSOFilterRule &rule)
    {
        rules.append(rule);
    }

    static QSOFilterRule createFromDateRule(const QDateTime &date)
    {
        return QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 4, date.toString("yyyy-MM-ddTHH:mm:ss"));  // 4 - should be enum - later '4' >
    }

    static QSOFilterRule createNonEmptyContestRule(const QString &contestID)
    {
        return QSOFilterRule(LogbookModel::COLUMN_CONTEST_ID, 2, contestID);// '2' is like
    }

    static QSOFilterRule createToDateRule(const QDateTime &date)
    {
        return QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 5, date.toString("yyyy-MM-ddTHH:mm:ss"));  // 5 - should be enum - later '5' <
                                                  // ON of OFF???
    }

    static QSOFilter createFromDateContestFilter(const QString &contestID, const QDateTime &date)
    {
        QSOFilter ret;

        ret.filterName = QString("%1-%2").arg(contestID, date.toString("yyyy/MM/dd hh:mm"));
        ret.machingType = 0; // should be enum - later
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

    static QString getWhereClause(const QString &filterName);
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
