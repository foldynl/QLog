#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

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
    QHash<int, DxccConfirmationResult> confirmationResults;
    const DxccConfirmationResolver spotConfirmationResolver =
        confirmationResolverForSpot(confirmationResults);

    for ( const AlertRule *rule : static_cast<const QList<AlertRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << *rule;

        if ( rule->match(spot, spotConfirmationResolver) )
        {
            matchedRules << rule->ruleName;
        }
    }

    if ( matchedRules.size() > 0 )
    {
        SpotAlert alert(matchedRules, spot);
        emit spotAlert(alert);
    }
}

void AlertEvaluator::WSJTXCQSpot(const WsjtxEntry &wsjtx)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "WSJTX CQ Spot";

    QStringList matchedRules;
    QHash<int, DxccConfirmationResult> confirmationResults;
    const DxccConfirmationResolver spotConfirmationResolver =
        confirmationResolverForSpot(confirmationResults);

    for ( const AlertRule *rule : static_cast<const QList<AlertRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << *rule;
        if ( rule->match(wsjtx, spotConfirmationResolver) )
        {
            matchedRules << rule->ruleName;
        }
    }

    if ( matchedRules.size() > 0 )
    {
        SpotAlert alert(matchedRules, wsjtx);
        emit spotAlert(alert);
    }
}

void AlertEvaluator::setDxccConfirmationResolver(const DxccConfirmationResolver &resolver)
{
    FCT_IDENTIFICATION;

    confirmationResolver = resolver;
}

DxccConfirmationResolver AlertEvaluator::confirmationResolverForSpot(
    QHash<int, DxccConfirmationResult> &results) const
{
    FCT_IDENTIFICATION;

    if ( !confirmationResolver ) return DxccConfirmationResolver();

    return [this, &results](int dxcc,
                            const QString &band,
                            const QString &mode,
                            AlertDxccContext context)
    {
        const int key = static_cast<int>(context);
        const auto cachedResult = results.constFind(key);
        if ( cachedResult != results.constEnd() ) return cachedResult.value();

        const DxccConfirmationResult result = confirmationResolver(dxcc, band, mode, context);
        results.insert(key, result);
        return result;
    };
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
    notificationEnabled(true),
    notificationSoundEnabled(true),
    notificationRepeatMinutes(DEFAULT_NOTIFICATION_REPEAT_MINUTES),
    sourceMap(SpotAlert::UKNOWN),
    dxCountry(-1),
    dxLogStatusMap(0),
    dxAlertGoal(static_cast<int>(AlertDxccGoal::Confirmed)),
    dxAlertContext(static_cast<int>(AlertDxccContext::Anywhere)),
    dxAlertGoalSet(false),
    dxAlertContextSet(false),
    spotterCountry(-1),
    ituz(0),
    cqz(0),
    pota(false),
    sota(false),
    iota(false),
    wwff(false),
    ruleValid(false)
{
    FCT_IDENTIFICATION;
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

    if ( ! insertUpdateStmt.prepare("INSERT INTO alert_rules(rule_name, enabled, notification_enabled, notification_sound_enabled, notification_repeat_minutes, source, dx_callsign, dx_country, "
                                    "dx_logstatus, dx_alert_goal, dx_alert_context, dx_continent, spot_comment, mode, band, spotter_country, spotter_continent, dx_member, ituz, cqz, pota, sota, iota, wwff) "
                                    " VALUES (:ruleName, :enabled, :notificationEnabled, :notificationSoundEnabled, :notificationRepeatMinutes, :source, :dxCallsign, :dxCountry, "
                                    ":dxLogstatus, :dxAlertGoal, :dxAlertContext, :dxContinent, :spotComment, :mode, :band, :spotterCountry, :spotterContinent, :dxMember, :ituz, :cqz, :pota, :sota, :iota, :wwff) "
                                    " ON CONFLICT(rule_name) DO UPDATE SET enabled = :enabled, notification_enabled = :notificationEnabled, notification_sound_enabled = :notificationSoundEnabled, notification_repeat_minutes = :notificationRepeatMinutes, source = :source, dx_callsign =:dxCallsign, "
                                    "dx_country = :dxCountry, dx_logstatus = :dxLogstatus, dx_alert_goal = :dxAlertGoal, dx_alert_context = :dxAlertContext, dx_continent = :dxContinent, spot_comment = :spotComment, "
                                    "mode = :mode, band = :band, spotter_country = :spotterCountry, spotter_continent = :spotterContinent, dx_member = :dxMember, ituz = :ituz, cqz = :cqz, pota = :pota, sota = :sota, iota = :iota, wwff = :wwff "
                                    " WHERE rule_name = :ruleName"))
    {
        qWarning() << "Cannot prepare insert/update Alert Rule statement" << insertUpdateStmt.lastError();
        return false;
    }

    insertUpdateStmt.bindValue(":ruleName", ruleName);
    insertUpdateStmt.bindValue(":enabled", enabled);
    insertUpdateStmt.bindValue(":notificationEnabled", notificationEnabled);
    insertUpdateStmt.bindValue(":notificationSoundEnabled", notificationSoundEnabled);
    insertUpdateStmt.bindValue(":notificationRepeatMinutes", qBound<int>(MIN_NOTIFICATION_REPEAT_MINUTES,
                                                                         notificationRepeatMinutes,
                                                                         MAX_NOTIFICATION_REPEAT_MINUTES));
    insertUpdateStmt.bindValue(":source", sourceMap);
    insertUpdateStmt.bindValue(":dxCallsign", dxCallsign);
    insertUpdateStmt.bindValue(":dxCountry", dxCountry);
    insertUpdateStmt.bindValue(":dxLogstatus", dxLogStatusMap);
    insertUpdateStmt.bindValue(":dxAlertGoal", dxAlertGoalSet ? QVariant(dxAlertGoal)
                                                              : QVariant());
    insertUpdateStmt.bindValue(":dxAlertContext", dxAlertContextSet ? QVariant(dxAlertContext)
                                                                    : QVariant());
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

    if ( ! insertUpdateStmt.exec() )
    {
        qCDebug(runtime) << "Cannot Update Alert Rules - "
                         << insertUpdateStmt.lastError().text();
        return false;
    }
    return true;
}

bool AlertRule::load(const QString &in_ruleName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_ruleName;

    QSqlQuery query;

    if ( ! query.prepare("SELECT rule_name, enabled, notification_enabled, notification_sound_enabled, notification_repeat_minutes, source, dx_callsign, dx_country, dx_logstatus, dx_alert_goal, dx_alert_context, "
                         "dx_continent, spot_comment, mode, band, spotter_country, spotter_continent, dx_member, ituz, cqz, pota, sota, iota, wwff "
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
        notificationEnabled = record.value("notification_enabled").toBool();
        notificationSoundEnabled = record.value("notification_sound_enabled").toBool();
        notificationRepeatMinutes = qBound<int>(MIN_NOTIFICATION_REPEAT_MINUTES,
                                                record.value("notification_repeat_minutes").toInt(),
                                                MAX_NOTIFICATION_REPEAT_MINUTES);
        sourceMap        = record.value("source").toInt();
        dxCallsign       = record.value("dx_callsign").toString();
        dxCountry        = record.value("dx_country").toInt();
        dxLogStatusMap   = record.value("dx_logstatus").toInt();
        dxAlertGoalSet   = !record.value("dx_alert_goal").isNull();
        dxAlertContextSet = !record.value("dx_alert_context").isNull();
        if ( dxAlertGoalSet )
            dxAlertGoal = record.value("dx_alert_goal").toInt();
        if ( dxAlertContextSet )
            dxAlertContext = record.value("dx_alert_context").toInt();
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

    if ( hasInvalidDxAlertCondition() )
        qCWarning(runtime) << "Invalid DXCC alert condition in rule" << ruleName;

    qCDebug(runtime) << "Rule: " << ruleName << " was loaded";

    ruleValid = true;
    return true;
}

bool AlertRule::match(const WsjtxEntry &wsjtx,
                      const DxccConfirmationResolver &confirmationResolver) const
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

    if ( mode != "*" )
    {
        const QString &group = BandPlan::isFTxMode(wsjtx.decodedMode)
            ? BandPlan::MODE_GROUP_STRING_FTx
            : BandPlan::MODE_GROUP_STRING_DIGITAL;
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
                     && commentRE.match(wsjtx.decode.message).hasMatch()
                     && logStatusCondition().match(wsjtx, confirmationResolver);

    qCDebug(runtime) << "Rule name: " << ruleName << " - result " << ret;
    return ret;
}

bool AlertRule::match(const DxSpot &spot,
                      const DxccConfirmationResolver &confirmationResolver) const
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
        commentRE.match(spot.comment).hasMatch() &&
        logStatusCondition().match(spot, confirmationResolver);

    qCDebug(runtime) << "Rule name:" << ruleName << "- result" << ret;
    return ret;
}

DxccLogStatusCondition AlertRule::logStatusCondition() const
{
    FCT_IDENTIFICATION;

    DxccLogStatusCondition condition(dxLogStatusMap);
    condition.goal = dxAlertGoal;
    condition.context = dxAlertContext;
    condition.goalSet = dxAlertGoalSet;
    condition.contextSet = dxAlertContextSet;
    return condition;
}

void AlertRule::setLogStatusCondition(const DxccLogStatusCondition &condition)
{
    FCT_IDENTIFICATION;

    dxLogStatusMap = condition.mask;
    dxAlertGoal = condition.goal;
    dxAlertContext = condition.context;
    dxAlertGoalSet = condition.goalSet;
    dxAlertContextSet = condition.contextSet;
}

bool AlertRule::hasDxAlertCondition() const
{
    FCT_IDENTIFICATION;

    return logStatusCondition().hasCondition();
}

bool AlertRule::hasInvalidDxAlertCondition() const
{
    FCT_IDENTIFICATION;

    return logStatusCondition().isInvalid();
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
            + "dxComment: "        + dxComment + "; "
            + "mode: "             + mode + "; "
            + "band: "             + band + "; "
            + "spotterCountry: "   + QString::number(spotterCountry) + "; "
            + "spotterContinent: " + spotterContinent + "; "
            + ")";
}
