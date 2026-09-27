#ifndef QLOG_CORE_ALERTEVALUATOR_H
#define QLOG_CORE_ALERTEVALUATOR_H

#include <QObject>
#include "data/DxSpot.h"
#include "data/WsjtxEntry.h"
#include "data/SpotAlert.h"
#include <QRegularExpression>
#include <functional>

class AlertRule : public QObject
{
    Q_OBJECT

public:
    /* Provides the DX Log Status for a scope other than Band & Mode.
     * The spot itself carries only the Band & Mode status; the coarser
     * statuses (per Band, per Entity) are resolved on demand via this callback */
    using LogStatusResolver = std::function<DxccStatus(int dxcc,
                                                       const QString &band,
                                                       const QString &modeGroup,
                                                       DxccStatusScope scope)>;

    /* The Log Status of a rule is expressed as "alert while I still need
     * <Need> until it is <Until>". The pair is stored as the status bitmap
     * plus the scope at which the status is evaluated */
    enum class LogStatusNeed
    {
        NewEntity   = 0,  // the entity, regardless of band and mode
        NewBand     = 1,  // the entity on the spot's band, regardless of mode
        NewBandMode = 2,  // the entity on the spot's band in the spot's mode
        Any         = 3,  // regardless of the log status
        NewMode     = 4   // the entity in the spot's mode, regardless of band
    };

    enum class LogStatusUntil
    {
        Worked    = 0,
        Confirmed = 1
    };

    /* What happens when the rule matches a spot */
    enum class AlarmType
    {
        None    = 0,
        Bell    = 1,   // the system bell
        Command = 2    // the alarm command is started
    };

    explicit AlertRule(QObject *parent = nullptr);
    ~AlertRule() {};

    bool save();
    bool load(const QString &);
    bool match(const WsjtxEntry &wsjtx,
               const LogStatusResolver &resolver = LogStatusResolver()) const;
    bool match(const DxSpot & spot,
               const LogStatusResolver &resolver = LogStatusResolver()) const;
    bool isValid() const;
    operator QString() const;

    static DxccStatusScope toLogStatusScope(int value);

    void setLogStatus(LogStatusNeed need, LogStatusUntil until);
    LogStatusNeed logStatusNeed() const;
    LogStatusUntil logStatusUntil() const;

    /* The alarm command line with the placeholders replaced by the spot
     * values. The first element is the program, the rest are its arguments.
     * An empty list means that there is nothing to run. */
    static QStringList alarmCommandLine(const QString &command,
                                        const DxSpot &spot,
                                        const QString &ruleName);
    static const QStringList ALARM_PLACEHOLDERS;
    static AlarmType toAlarmType(int value);

public:
    QString ruleName;
    bool enabled;
    int sourceMap;
    QString dxCallsign;
    int dxCountry;
    int dxLogStatusMap;
    DxccStatusScope dxLogStatusScope;
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
    AlarmType alarm;
    QString alarmCommand;
    int alarmBackoff;   // seconds; the same callsign does not alarm again within this time

private:
    DxccStatus logStatus(const DxSpot &spot,
                         const QString &modeGroup,
                         const LogStatusResolver &resolver) const;

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
    void setLogStatusResolver(const AlertRule::LogStatusResolver &resolver);
    void setAlarmsMuted(bool muted);
    bool alarmsMuted() const { return alarmsMutedState; }

    /* Decides whether the alarm of a rule is due for a callsign and records
     * the time when it is. The history is passed in to keep the function testable */
    static bool alarmDue(QHash<QString, QDateTime> &history,
                         const QString &ruleName,
                         int backoffSeconds,
                         const QString &callsign,
                         const QDateTime &now);
    static bool startAlarm(const QStringList &commandLine);

public slots:
    void dxSpot(const DxSpot&);
    void WSJTXCQSpot(const WsjtxEntry&);
    void loadRules();

signals:
    void spotAlert(SpotAlert alert);
    void alarmBell();

private:
    void runAlarms(const QList<const AlertRule *> &matchedRules, const DxSpot &spot);

    QList<AlertRule *>ruleList;
    AlertRule::LogStatusResolver logStatusResolver;
    QHash<QString, QDateTime> alarmHistory;
    bool alarmsMutedState = false;
};

#endif // QLOG_CORE_ALERTEVALUATOR_H
