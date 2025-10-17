#include <QSqlQuery>
#include <QSqlError>


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

    for ( const AlertRule *rule : static_cast<const QList<AlertRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << *rule;

        if ( rule->match(spot) )
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

    for ( const AlertRule *rule : static_cast<const QList<AlertRule *>&>(ruleList) )
    {
        qCDebug(runtime) << "Processing " << *rule;
        if ( rule->match(wsjtx) )
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
    sourceMap(0),
    dxCountry(-1),
    dxLogStatusMap(0),
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

    if ( ! insertUpdateStmt.prepare("INSERT INTO alert_rules(rule_name, enabled, source, dx_callsign, dx_country, "
                                    "dx_logstatus, dx_continent, spot_comment, mode, band, spotter_country, spotter_continent, dx_member, ituz, cqz, pota, sota, iota, wwff) "
                                    " VALUES (:ruleName, :enabled, :source, :dxCallsign, :dxCountry, "
                                    ":dxLogstatus, :dxContinent, :spotComment, :mode, :band, :spotterCountry, :spotterContinent, :dxMember, :ituz, :cqz, :pota, :sota, :iota, :wwff) "
                                    " ON CONFLICT(rule_name) DO UPDATE SET enabled = :enabled, source = :source, dx_callsign =:dxCallsign, "
                                    "dx_country = :dxCountry, dx_logstatus = :dxLogstatus, dx_continent = :dxContinent, spot_comment = :spotComment, "
                                    "mode = :mode, band = :band, spotter_country = :spotterCountry, spotter_continent = :spotterContinent, dx_member = :dxMember, ituz = :ituz, cqz = :cqz, pota = :pota, sota = :sota, iota = :iota, wwff = :wwff "
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

    if ( ! query.prepare("SELECT rule_name, enabled, source, dx_callsign, dx_country, dx_logstatus, "
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
        sourceMap        = record.value("source").toInt();
        dxCallsign       = record.value("dx_callsign").toString();
        dxCountry        = record.value("dx_country").toInt();
        dxLogStatusMap   = record.value("dx_logstatus").toInt();
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

    qCDebug(runtime) << "Rule: " << ruleName << " was loaded";

    ruleValid = true;
    return true;
}

bool AlertRule::match(const WsjtxEntry &wsjtx) const
{
    FCT_IDENTIFICATION;

    bool ret = false;

    qCDebug(function_parameters) << wsjtx;

    /* the first part validates a primitive types */
    if ( isValid()
         && enabled
         && (sourceMap & SpotAlert::WSJTXCQSPOT)
         && (dxCountry == 0 || dxCountry == wsjtx.dxcc.dxcc)
         && (ituz == 0 || ituz == wsjtx.dxcc.ituz)
         && (cqz == 0 || cqz == wsjtx.dxcc.cqz)
         && (!pota || wsjtx.containsPOTA)
         && (!sota || wsjtx.containsSOTA)
         && (!iota || wsjtx.containsIOTA)
         && (!wwff || wsjtx.containsWWFF)
         && (wsjtx.status & dxLogStatusMap)
         && (mode == "*" || mode.contains("|" + ((wsjtx.decodedMode == "FT8") ? BandPlan::MODE_GROUP_STRING_FT8
                                                                              : BandPlan::MODE_GROUP_STRING_DIGITAL )))
         && (band == "*" || band.contains("|" + wsjtx.band))
         && (spotterCountry == 0 || spotterCountry == wsjtx.dxcc_spotter.dxcc )
         && (dxContinent == "*" || dxContinent.contains("|" + wsjtx.dxcc.cont))
         && (spotterContinent == "*" || spotterContinent.contains("|" + wsjtx.dxcc_spotter.cont))
         && (dxMember == QStringList("*") || wsjtx.memberList2Set().intersects(dxMemberSet))
       )
    {
        qCDebug(runtime) << "Rule match - phase 1 - OK";

        qCDebug(runtime) << "Callsign RE" << callsignRE.pattern();
        qCDebug(runtime) << "Comment RE" << commentRE.pattern();

        /* primitive types are OK, lets go to validate RE */
        QRegularExpressionMatch callsignMatch = callsignRE.match(wsjtx.callsign);
        QRegularExpressionMatch commentMatch = commentRE.match(wsjtx.decode.message);

        ret = callsignMatch.hasMatch()
              && commentMatch.hasMatch();
    }

    qCDebug(runtime) << "Rule name: " << ruleName << " - result " << ret;

    return ret;
}

bool AlertRule::match(const DxSpot &spot) const
{
    FCT_IDENTIFICATION;

    bool ret = false;

    qCDebug(function_parameters) << spot;

    /* the first part validates a primitive types */
    if ( isValid()
         && enabled
         && (sourceMap & SpotAlert::DXSPOT)
         && (dxCountry == 0 || dxCountry == spot.dxcc.dxcc)
         && (ituz == 0 || ituz == spot.dxcc.ituz)
         && (cqz == 0 || cqz == spot.dxcc.cqz)
         && (!pota || spot.containsPOTA)
         && (!sota || spot.containsSOTA)
         && (!iota || spot.containsIOTA)
         && (!wwff || spot.containsWWFF)
         && (spot.status & dxLogStatusMap)
         && (mode == "*" || (!spot.modeGroupString.isEmpty() && mode.contains("|" + spot.modeGroupString)))
         && (band == "*" || (!spot.band.isEmpty() && band.contains("|" + spot.band)))
         && (spotterCountry == 0 || spotterCountry == spot.dxcc_spotter.dxcc )
         && (dxContinent == "*" || (!spot.dxcc.cont.isEmpty() && dxContinent.contains("|" + spot.dxcc.cont)))
         && (spotterContinent == "*" || (!spot.dxcc_spotter.cont.isEmpty() && spotterContinent.contains("|" + spot.dxcc_spotter.cont)))
         && (dxMember == QStringList("*") || spot.memberList2Set().intersects(dxMemberSet))
       )
    {
        qCDebug(runtime) << "Rule match - phase 1 - OK";

        qCDebug(runtime) << "Callsign RE" << callsignRE.pattern();
        qCDebug(runtime) << "Comment RE" << commentRE.pattern();

        /* primitive types are OK, lets go to validate RE */
        QRegularExpressionMatch callsignMatch = callsignRE.match(spot.callsign);
        QRegularExpressionMatch commentMatch = commentRE.match(spot.comment);

        ret = callsignMatch.hasMatch()
              && commentMatch.hasMatch();
    }

    qCDebug(runtime) << "Rule name: " << ruleName << " - result " << ret;

    return ret;
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
