#ifndef QLOG_CORE_ALERTEVALUATOR_H
#define QLOG_CORE_ALERTEVALUATOR_H

#include <QObject>
#include "data/DxSpot.h"
#include "data/WsjtxEntry.h"
#include "data/SpotAlert.h"
#include <QHash>
#include <QRegularExpression>
#include <functional>

using DxccConfirmationResolver = std::function<DxccConfirmationResult(
    int,
    const QString &,
    const QString &,
    AlertDxccContext)>;

class AlertRule : public QObject
{
    Q_OBJECT

public:
    explicit AlertRule(QObject *parent = nullptr);
    ~AlertRule() {};

    bool save();
    bool load(const QString &);
    bool match(const WsjtxEntry &wsjtx,
               const DxccConfirmationResolver &confirmationResolver = DxccConfirmationResolver()) const;
    bool match(const DxSpot &spot,
               const DxccConfirmationResolver &confirmationResolver = DxccConfirmationResolver()) const;
    bool isValid() const;
    bool hasDxAlertCondition() const;
    bool hasInvalidDxAlertCondition() const;
    operator QString() const;
public:
    QString ruleName;
    bool enabled;
    int sourceMap;
    QString dxCallsign;
    int dxCountry;
    int dxLogStatusMap;
    int dxAlertGoal;
    int dxAlertContext;
    bool dxAlertGoalSet;
    bool dxAlertContextSet;
    QString dxContinent;
    QString dxComment;
    QStringList dxMember;
    QString mode;
    QString band;
    int spotterCountry;
    QString spotterContinent;
    int ituz;
    int cqz;
    bool pota;
    bool sota;
    bool iota;
    bool wwff;

private:
    bool matchDxccStatus(const DxSpot &spot,
                         const DxccConfirmationResolver &confirmationResolver) const;
    bool ruleValid;
    QRegularExpression callsignRE;
    QRegularExpression commentRE;
    QSet<QString> dxMemberSet;

};

class AlertEvaluator : public QObject
{
    Q_OBJECT
public:
    explicit AlertEvaluator(QObject *parent = nullptr);
    ~AlertEvaluator() {clearRules();}

    void clearRules();
    void setDxccConfirmationResolver(const DxccConfirmationResolver &resolver);

public slots:
    void dxSpot(const DxSpot&);
    void WSJTXCQSpot(const WsjtxEntry&);
    void loadRules();

signals:
    void spotAlert(SpotAlert alert);

private:
    DxccConfirmationResolver confirmationResolverForSpot(
        QHash<int, DxccConfirmationResult> &results) const;
    QList<AlertRule *>ruleList;
    DxccConfirmationResolver confirmationResolver;
};

#endif // QLOG_CORE_ALERTEVALUATOR_H
