#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QProcess>

#include "AlertEvaluator.h"
#include "debug.h"
#include "data/DxSpot.h"
#include "data/WsjtxEntry.h"
#include "data/SpotAlert.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.ui.alertevaluator");

AlertEvaluator::AlertEvaluator(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
    loadRules();
}

void AlertEvaluator::clearRules()
{
    FCT_IDENTIFICATION;

    qDeleteAll(ruleList);
    ruleList.clear();
}

void AlertEvaluator::dxSpot(const DxSpot & spot)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "DX Spot";

    QStringList matchedRules;
    QList<const AlertRule *> matchedRulePtrs;

    for ( const AlertRule *rule : static_cast<const QList<AlertRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << *rule;

        if ( rule->match(spot, logStatusResolver) )
        {
            matchedRules << rule->ruleName;
            matchedRulePtrs << rule;
        }
    }

    if ( matchedRules.size() > 0 )
    {
        SpotAlert alert(matchedRules, spot);
        emit spotAlert(alert);
        runAlarms(matchedRulePtrs, spot);
    }
}

void AlertEvaluator::WSJTXCQSpot(const WsjtxEntry &wsjtx)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "WSJTX CQ Spot";

    QStringList matchedRules;
    QList<const AlertRule *> matchedRulePtrs;

    for ( const AlertRule *rule : static_cast<const QList<AlertRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << *rule;
        if ( rule->match(wsjtx, logStatusResolver) )
        {
            matchedRules << rule->ruleName;
            matchedRulePtrs << rule;
        }
    }

    if ( matchedRules.size() > 0 )
    {
        SpotAlert alert(matchedRules, wsjtx);
        emit spotAlert(alert);
        runAlarms(matchedRulePtrs, wsjtx);
    }
}

void AlertEvaluator::setLogStatusResolver(const AlertRule::LogStatusResolver &resolver)
{
    FCT_IDENTIFICATION;

    logStatusResolver = resolver;
}

void AlertEvaluator::setAlarmsMuted(bool muted)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << muted;

    alarmsMutedState = muted;
}

bool AlertEvaluator::alarmDue(QHash<QString, QDateTime> &history,
                              const QString &ruleName,
                              int backoffSeconds,
                              const QString &callsign,
                              const QDateTime &now)
{
    FCT_IDENTIFICATION;

    const QString key = ruleName + QLatin1Char('|') + callsign;
    const QDateTime last = history.value(key);

    if ( last.isValid() && backoffSeconds > 0 && last.secsTo(now) < backoffSeconds )
    {
        qCDebug(runtime) << "Alarm suppressed by backoff" << key;
        return false;
    }

    history.insert(key, now);

    // keep the history small - entries older than the longest reasonable backoff are useless
    if ( history.size() > 1000 )
    {
        for ( auto it = history.begin(); it != history.end(); )
        {
            if ( it.value().secsTo(now) > 86400 )
                it = history.erase(it);
            else
                ++it;
        }
    }

    return true;
}

bool AlertEvaluator::startAlarm(const QStringList &commandLine)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << commandLine;

    if ( commandLine.isEmpty() )
        return false;

    const bool started = QProcess::startDetached(commandLine.first(), commandLine.mid(1));

    if ( !started )
        qWarning() << "Cannot start the alarm command" << commandLine;

    return started;
}

void AlertEvaluator::runAlarms(const QList<const AlertRule *> &matchedRules, const DxSpot &spot)
{
    FCT_IDENTIFICATION;

    if ( alarmsMutedState )
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();

    // one alarm per spot - the first matching rule with an alarm wins
    for ( const AlertRule *rule : matchedRules )
    {
        if ( rule->alarm == AlertRule::AlarmType::None )
            continue;

        if ( rule->alarm == AlertRule::AlarmType::Command && rule->alarmCommand.trimmed().isEmpty() )
            continue;

        if ( !alarmDue(alarmHistory, rule->ruleName, rule->alarmBackoff, spot.callsign, now) )
            continue;

        if ( rule->alarm == AlertRule::AlarmType::Bell )
            emit alarmBell();
        else
            startAlarm(AlertRule::alarmCommandLine(rule->alarmCommand, spot, rule->ruleName));

        return;
    }
}

void AlertEvaluator::loadRules()
{
    FCT_IDENTIFICATION;

    if ( ruleList.size() > 0 )
    {
        clearRules();
    }

    QSqlQuery ruleStmt;

    if ( ! ruleStmt.prepare("SELECT rule_name FROM alert_rules") )
    {
        qWarning() << "Cannot prepare select statement";
    }
    else
    {
        if ( ruleStmt.exec() )
        {
            while (ruleStmt.next())
            {
                AlertRule *rule;
                rule = new AlertRule();
                if ( rule )
                {
                    rule->load(ruleStmt.value(0).toString());
                    ruleList.append(rule);
                }
            }
        }
        else
        {
            qInfo()<< "Cannot get filters names from DB" << ruleStmt.lastError();
        }
    }
}

AlertRule::AlertRule(QObject *parent) :
    QObject(parent),
    enabled(false),
    sourceMap(SpotAlert::UKNOWN),
    dxCountry(-1),
    dxLogStatusMap(0),
    dxLogStatusScope(DxccStatusScope::BandMode),
    spotterCountry(-1),
    ituz(0),
    cqz(0),
    pota(false),
    sota(false),
    iota(false),
    wwff(false),
    alarm(AlarmType::None),
    alarmBackoff(300),
    ruleValid(false)
{
    FCT_IDENTIFICATION;
}

const QStringList AlertRule::ALARM_PLACEHOLDERS =
{
    QStringLiteral("{callsign}"),
    QStringLiteral("{band}"),
    QStringLiteral("{mode}"),
    QStringLiteral("{freq}"),
    QStringLiteral("{country}"),
    QStringLiteral("{rule}")
};

QStringList AlertRule::alarmCommandLine(const QString &command,
                                        const DxSpot &spot,
                                        const QString &ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << command << ruleName;

    if ( command.trimmed().isEmpty() )
        return QStringList();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    QStringList parts = QProcess::splitCommand(command);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
    QStringList parts = command.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
#endif

    // the values are substituted after the split, therefore a value with
    // a space (e.g. a country name) stays one argument
    for ( QString &part : parts )
    {
        part.replace(QLatin1String("{callsign}"), spot.callsign);
        part.replace(QLatin1String("{band}"), spot.band);
        part.replace(QLatin1String("{mode}"), spot.modeGroupString);
        part.replace(QLatin1String("{freq}"), QString::number(spot.freq, 'f', 3));
        part.replace(QLatin1String("{country}"), spot.dxcc.country);
        part.replace(QLatin1String("{rule}"), ruleName);
    }

    return parts;
}

bool AlertRule::save()
{
    FCT_IDENTIFICATION;

    if ( ruleName.isEmpty() )
    {
        qCDebug(runtime) << "rule name is empty - do not save";
        return false;
    }

    QSqlQuery insertUpdateStmt;

    if ( ! insertUpdateStmt.prepare("INSERT INTO alert_rules(rule_name, enabled, source, dx_callsign, dx_country, "
                                    "dx_logstatus, dx_logstatus_scope, dx_continent, spot_comment, mode, band, spotter_country, spotter_continent, dx_member, ituz, cqz, pota, sota, iota, wwff, "
                                    "alarm, alarm_command, alarm_backoff) "
                                    " VALUES (:ruleName, :enabled, :source, :dxCallsign, :dxCountry, "
                                    ":dxLogstatus, :dxLogstatusScope, :dxContinent, :spotComment, :mode, :band, :spotterCountry, :spotterContinent, :dxMember, :ituz, :cqz, :pota, :sota, :iota, :wwff, "
                                    ":alarm, :alarmCommand, :alarmBackoff) "
                                    " ON CONFLICT(rule_name) DO UPDATE SET enabled = :enabled, source = :source, dx_callsign =:dxCallsign, "
                                    "dx_country = :dxCountry, dx_logstatus = :dxLogstatus, dx_logstatus_scope = :dxLogstatusScope, dx_continent = :dxContinent, spot_comment = :spotComment, "
                                    "mode = :mode, band = :band, spotter_country = :spotterCountry, spotter_continent = :spotterContinent, dx_member = :dxMember, ituz = :ituz, cqz = :cqz, pota = :pota, sota = :sota, iota = :iota, wwff = :wwff, "
                                    "alarm = :alarm, alarm_command = :alarmCommand, alarm_backoff = :alarmBackoff "
                                    " WHERE rule_name = :ruleName"))
    {
        qWarning() << "Cannot prepare insert/update Alert Rule statement" << insertUpdateStmt.lastError();
        return false;
    }

    insertUpdateStmt.bindValue(":ruleName", ruleName);
    insertUpdateStmt.bindValue(":enabled", enabled);
    insertUpdateStmt.bindValue(":source", sourceMap);
    insertUpdateStmt.bindValue(":dxCallsign", dxCallsign);
    insertUpdateStmt.bindValue(":dxCountry", dxCountry);
    insertUpdateStmt.bindValue(":dxLogstatus", dxLogStatusMap);
    insertUpdateStmt.bindValue(":dxLogstatusScope", static_cast<int>(dxLogStatusScope));
    insertUpdateStmt.bindValue(":dxContinent", dxContinent);
    insertUpdateStmt.bindValue(":dxMember", dxMember.join(","));
    insertUpdateStmt.bindValue(":spotComment", dxComment);
    insertUpdateStmt.bindValue(":mode", mode);
    insertUpdateStmt.bindValue(":band", band);
    insertUpdateStmt.bindValue(":spotterCountry", spotterCountry);
    insertUpdateStmt.bindValue(":spotterContinent", spotterContinent);
    insertUpdateStmt.bindValue(":cqz", cqz);
    insertUpdateStmt.bindValue(":ituz", ituz);
    insertUpdateStmt.bindValue(":pota", pota);
    insertUpdateStmt.bindValue(":sota", sota);
    insertUpdateStmt.bindValue(":iota", iota);
    insertUpdateStmt.bindValue(":wwff", wwff);
    insertUpdateStmt.bindValue(":alarm", static_cast<int>(alarm));
    insertUpdateStmt.bindValue(":alarmCommand", alarmCommand);
    insertUpdateStmt.bindValue(":alarmBackoff", alarmBackoff);

    if ( ! insertUpdateStmt.exec() )
    {
        qCDebug(runtime)<< "Cannot Update Alert Rules - " << insertUpdateStmt.lastError().text();
        return false;
    }
    return true;
}

bool AlertRule::load(const QString &in_ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_ruleName;

    QSqlQuery query;

    if ( ! query.prepare("SELECT rule_name, enabled, source, dx_callsign, dx_country, dx_logstatus, dx_logstatus_scope, "
                         "dx_continent, spot_comment, mode, band, spotter_country, spotter_continent, dx_member, ituz, cqz, pota, sota, iota, wwff, "
                         "alarm, alarm_command, alarm_backoff "
                         "FROM alert_rules "
                         "WHERE rule_name = :rule") )
    {
        qWarning() << "Cannot prepare select statement";
        return false;
    }

    query.bindValue(":rule", in_ruleName);

    if ( query.exec() )
    {
        query.next();

        QSqlRecord record = query.record();

        ruleName         = in_ruleName;
        enabled          = record.value("enabled").toBool();
        sourceMap        = record.value("source").toInt();
        dxCallsign       = record.value("dx_callsign").toString();
        dxCountry        = record.value("dx_country").toInt();
        dxLogStatusMap   = record.value("dx_logstatus").toInt();
        dxLogStatusScope = toLogStatusScope(record.value("dx_logstatus_scope").toInt());
        dxContinent      = record.value("dx_continent").toString();
        dxComment        = record.value("spot_comment").toString();
        dxMember         = record.value("dx_member").toString().split(",");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        dxMemberSet      = QSet<QString>(dxMember.begin(), dxMember.end());
#else /* Due to ubuntu 20.04 where qt5.12 is present */
        dxMemberSet      = QSet<QString>(QSet<QString>::fromList(dxMember));
#endif
        mode             = record.value("mode").toString();
        band             = record.value("band").toString();
        spotterCountry   = record.value("spotter_country").toInt();
        spotterContinent = record.value("spotter_continent").toString();
        ituz             = record.value("ituz").toInt();
        cqz              = record.value("cqz").toInt();
        pota             = record.value("pota").toBool();
        sota             = record.value("sota").toBool();
        iota             = record.value("iota").toBool();
        wwff             = record.value("wwff").toBool();
        alarm            = toAlarmType(record.value("alarm").toInt());
        alarmCommand     = record.value("alarm_command").toString();
        alarmBackoff     = record.value("alarm_backoff").isNull() ? 300 : record.value("alarm_backoff").toInt();

        callsignRE.setPattern(dxCallsign);
        callsignRE.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

        commentRE.setPattern(dxComment);
        commentRE.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }
    else
    {
        qCDebug(runtime) << "SQL execution error: " << query.lastError().text();
        return false;
    }

    qCDebug(runtime) << "Rule: " << ruleName << " was loaded";

    ruleValid = true;
    return true;
}

bool AlertRule::match(const WsjtxEntry &wsjtx, const LogStatusResolver &resolver) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << wsjtx;

    auto fail = [&]() -> bool
    {
        qCDebug(runtime) << "Rule name:" << ruleName << "- result false";
        return false;
    };

    /* the first part validates a primitive types */
    if ( !isValid() || !enabled )                     return fail();
    if ( !(sourceMap & SpotAlert::WSJTXCQSPOT) )      return fail();

    if ( dxCountry && dxCountry != wsjtx.dxcc.dxcc )  return fail();
    if ( ituz      && ituz      != wsjtx.dxcc.ituz )  return fail();
    if ( cqz       && cqz       != wsjtx.dxcc.cqz )   return fail();
    if ( pota || sota || iota || wwff )
    {
        const bool refMatch =
               (pota && wsjtx.containsPOTA)
            || (sota && wsjtx.containsSOTA)
            || (iota && wsjtx.containsIOTA)
            || (wwff && wsjtx.containsWWFF);

        if ( !refMatch ) return fail();
    }

    const QString &group = BandPlan::isFTxMode(wsjtx.decodedMode)
        ? BandPlan::MODE_GROUP_STRING_FTx
        : BandPlan::MODE_GROUP_STRING_DIGITAL;

    if ( !(logStatus(wsjtx, group, resolver) & dxLogStatusMap) ) return fail();

    if ( mode != "*" )
    {
        if ( !mode.contains(QLatin1Char('|') + group) ) return fail();
    }

    if ( band != "*" && !band.contains(QLatin1Char('|') + wsjtx.band) )  return fail();

    if ( spotterCountry && spotterCountry != wsjtx.dxcc_spotter.dxcc) return fail();
    if ( dxContinent != "*" && !dxContinent.contains(QLatin1Char('|') + wsjtx.dxcc.cont)) return fail();
    if ( spotterContinent != "*" && !spotterContinent.contains(QLatin1Char('|') + wsjtx.dxcc_spotter.cont)) return fail();

    if ( !(dxMember.size() == 1 && dxMember.front() == QLatin1String("*")))
    {
        if ( !wsjtx.memberList2Set().intersects(dxMemberSet) ) return fail();
    }

    qCDebug(runtime) << "Rule match - phase 1 - OK";
    qCDebug(runtime) << "Callsign RE" << callsignRE.pattern();
    qCDebug(runtime) << "Comment RE" << commentRE.pattern();

    const bool ret = callsignRE.match(wsjtx.callsign).hasMatch()
                     && commentRE.match(wsjtx.decode.message).hasMatch();

    qCDebug(runtime) << "Rule name: " << ruleName << " - result " << ret;
    return ret;
}

bool AlertRule::match(const DxSpot &spot, const LogStatusResolver &resolver) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << spot;

    auto fail = [&]() -> bool
    {
        qCDebug(runtime) << "Rule name:" << ruleName << "- result false";
        return false;
    };

    /* the first part validates a primitive types */
    if ( !isValid() || !enabled )         return fail();
    if ( !(sourceMap & SpotAlert::DXSPOT) )  return fail();

    if ( dxCountry != 0 && dxCountry != spot.dxcc.dxcc ) return fail();
    if ( ituz      != 0 && ituz      != spot.dxcc.ituz ) return fail();
    if ( cqz       != 0 && cqz       != spot.dxcc.cqz )  return fail();
    if ( pota || sota || iota || wwff )
    {
        const bool refMatch =
               (pota && spot.containsPOTA)
            || (sota && spot.containsSOTA)
            || (iota && spot.containsIOTA)
            || (wwff && spot.containsWWFF);

        if ( !refMatch ) return fail();
    }

    if ( !(logStatus(spot, spot.modeGroupString, resolver) & dxLogStatusMap) ) return fail();

    if ( mode != "*" )
    {
        if (spot.modeGroupString.isEmpty()) return fail();
        if (!mode.contains(QLatin1Char('|') + spot.modeGroupString)) return fail();
    }

    if ( band != "*" )
    {
        if (spot.band.isEmpty()) return fail();
        if (!band.contains(QLatin1Char('|') + spot.band)) return fail();
    }

    if ( spotterCountry != 0 && spotterCountry != spot.dxcc_spotter.dxcc ) return fail();

    if ( dxContinent != "*" )
    {
        if ( spot.dxcc.cont.isEmpty() )              return fail();
        if ( !dxContinent.contains(QLatin1Char('|') + spot.dxcc.cont) ) return fail();
    }

    if ( spotterContinent != "*" )
    {
        if ( spot.dxcc_spotter.cont.isEmpty() ) return fail();
        if ( !spotterContinent.contains(QLatin1Char('|') + spot.dxcc_spotter.cont) ) return fail();
    }

    if ( !(dxMember.size() == 1 && dxMember.front() == QLatin1String("*")) )
    {
        if (!spot.memberList2Set().intersects(dxMemberSet)) return fail();
    }

    qCDebug(runtime) << "Rule match - phase 1 - OK";
    qCDebug(runtime) << "Callsign RE" << callsignRE.pattern();
    qCDebug(runtime) << "Comment RE" << commentRE.pattern();

    const bool ret =
        callsignRE.match(spot.callsign).hasMatch() &&
        commentRE.match(spot.comment).hasMatch();

    qCDebug(runtime) << "Rule name:" << ruleName << "- result" << ret;
    return ret;
}

DxccStatus AlertRule::logStatus(const DxSpot &spot,
                                const QString &modeGroup,
                                const LogStatusResolver &resolver) const
{
    FCT_IDENTIFICATION;

    // The spot already carries the Band & Mode status computed by its source
    if ( dxLogStatusScope == DxccStatusScope::BandMode )
        return spot.status;

    if ( !resolver )
    {
        qCDebug(runtime) << "No Log Status resolver - using the spot status";
        return spot.status;
    }

    const DxccStatus status = resolver(spot.dxcc.dxcc, spot.band, modeGroup, dxLogStatusScope);

    qCDebug(runtime) << "Scoped Log Status" << static_cast<int>(dxLogStatusScope) << status;

    return status;
}

AlertRule::AlarmType AlertRule::toAlarmType(int value)
{
    FCT_IDENTIFICATION;

    switch ( value )
    {
    case static_cast<int>(AlarmType::Bell):    return AlarmType::Bell;
    case static_cast<int>(AlarmType::Command): return AlarmType::Command;
    default:                                   return AlarmType::None;
    }
}

DxccStatusScope AlertRule::toLogStatusScope(int value)
{
    FCT_IDENTIFICATION;

    switch ( value )
    {
    case static_cast<int>(DxccStatusScope::Band):
        return DxccStatusScope::Band;
    case static_cast<int>(DxccStatusScope::Entity):
        return DxccStatusScope::Entity;
    case static_cast<int>(DxccStatusScope::Mode):
        return DxccStatusScope::Mode;
    default:
        return DxccStatusScope::BandMode;
    }
}

void AlertRule::setLogStatus(LogStatusNeed need, LogStatusUntil until)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << static_cast<int>(need) << static_cast<int>(until);

    switch ( need )
    {
    case LogStatusNeed::NewEntity:
        dxLogStatusMap = DxccStatus::NewEntity;
        dxLogStatusScope = DxccStatusScope::Entity;
        break;
    case LogStatusNeed::NewBand:
        dxLogStatusMap = DxccStatus::NewEntity | DxccStatus::NewBand;
        dxLogStatusScope = DxccStatusScope::Band;
        break;
    case LogStatusNeed::NewMode:
        dxLogStatusMap = DxccStatus::NewEntity | DxccStatus::NewMode;
        dxLogStatusScope = DxccStatusScope::Mode;
        break;
    case LogStatusNeed::NewBandMode:
        dxLogStatusMap = DxccStatus::NewEntity | DxccStatus::NewBand
                         | DxccStatus::NewMode | DxccStatus::NewSlot;
        dxLogStatusScope = DxccStatusScope::BandMode;
        break;
    case LogStatusNeed::Any:
        dxLogStatusMap = DxccStatus::All;
        dxLogStatusScope = DxccStatusScope::BandMode;
        return;
    }

    if ( until == LogStatusUntil::Confirmed )
        dxLogStatusMap |= DxccStatus::Worked;
}

AlertRule::LogStatusNeed AlertRule::logStatusNeed() const
{
    FCT_IDENTIFICATION;

    if ( dxLogStatusMap == DxccStatus::All )
        return LogStatusNeed::Any;

    switch ( dxLogStatusScope )
    {
    case DxccStatusScope::Entity: return LogStatusNeed::NewEntity;
    case DxccStatusScope::Band:   return LogStatusNeed::NewBand;
    case DxccStatusScope::Mode:   return LogStatusNeed::NewMode;
    default:                      return LogStatusNeed::NewBandMode;
    }
}

AlertRule::LogStatusUntil AlertRule::logStatusUntil() const
{
    FCT_IDENTIFICATION;

    return ( dxLogStatusMap & DxccStatus::Worked ) ? LogStatusUntil::Confirmed
                                                   : LogStatusUntil::Worked;
}

bool AlertRule::isValid() const
{
    FCT_IDENTIFICATION;

    return ruleValid;
}

AlertRule::operator QString() const
{
    return QString("AlerRule: ")
            + "("
            + "Rule Name: "        + ruleName + "; "
            + "isValid: "          + QString::number(isValid()) + "; "
            + "Enabled: "          + QString::number(enabled) + "; "
            + "SourceMap: 0b"      + QString::number(sourceMap,2) + "; "
            + "dxCallsign: "       + dxCallsign + "; "
            + "CQZ: "              + QString::number(cqz) + "; "
            + "ITUZ: "             + QString::number(ituz) + "; "
            + "POTA: "             + (pota ? "true" : "false") + "; "
            + "SOTA: "             + (sota ? "true" : "false") + "; "
            + "IOTA: "             + (iota ? "true" : "false") + "; "
            + "WWFF: "             + (wwff ? "true" : "false") + "; "
            + "dxMember: "         + dxMember.join(", ") + "; "
            + "dxCountry: "        + QString::number(dxCountry) + "; "
            + "dxLogStatusMap: 0b" + QString::number(dxLogStatusMap,2) + "; "
            + "dxLogStatusScope: " + QString::number(static_cast<int>(dxLogStatusScope)) + "; "
            + "dxComment: "        + dxComment + "; "
            + "mode: "             + mode + "; "
            + "band: "             + band + "; "
            + "spotterCountry: "   + QString::number(spotterCountry) + "; "
            + "spotterContinent: " + spotterContinent + "; "
            + "alarm: "            + QString::number(static_cast<int>(alarm)) + "; "
            + "alarmCommand: "     + alarmCommand + "; "
            + "alarmBackoff: "     + QString::number(alarmBackoff) + "; "
            + ")";
}
