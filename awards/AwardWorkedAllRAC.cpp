#include <QCoreApplication>

#include "AwardWorkedAllRAC.h"


QString AwardWorkedAllRAC::eligibleContactCondition(const QString &columnPrefix) const
{
    return QStringLiteral("%1start_time >= '1998-07-02'"
                          " AND UPPER(COALESCE(%1prop_mode, '')) <> 'RPT'").arg(columnPrefix);
}

QString AwardWorkedAllRAC::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Worked All RAC (WARAC)");
}

QString AwardWorkedAllRAC::rulesUrl() const
{
    return QStringLiteral("https://www.rac.ca/worked-all-rac/");
}

QString AwardWorkedAllRAC::headersColumns(const QString &) const
{
    // col1 identifies one award credit. Alternative callsigns in the same
    // province/territory (or RAC HQ) therefore count only once.
    return QStringLiteral("w.station_group col1, w.area col2 ");
}

QString AwardWorkedAllRAC::sqlDetailTable(const QString &entity) const
{
    return QStringLiteral(" FROM warac_stations w"
                          "   LEFT OUTER JOIN source_contacts c"
                          "     ON c.callsign = w.callsign"
                          "    AND c.my_dxcc = '%1'"
                          "    AND %2"
                          "   LEFT OUTER JOIN modes m ON c.mode = m.name")
        .arg(entity, eligibleContactCondition("c."));
}

QString AwardWorkedAllRAC::additionalWhere(const QString &) const
{
    return QString();
}

QStringList AwardWorkedAllRAC::additionalCTEs(const QString &, const QString &) const
{
    // The 15 credit groups follow the current RAC application form. Each CTE
    // row is an official callsign; GROUP BY col1/col2 merges alternatives.
    return { QStringLiteral(
        " warac_stations(station_group, area, callsign) AS (VALUES "
        "('VA1RAC / VE1RAC','N.S.','VA1RAC'),"
        "('VA1RAC / VE1RAC','N.S.','VE1RAC'),"
        "('VA2RAC','Que.','VA2RAC'),"
        "('VA3RAC / VE3RAC','Ont.','VA3RAC'),"
        "('VA3RAC / VE3RAC','Ont.','VE3RAC'),"
        "('VA4RAC / VE4RAC','Man.','VA4RAC'),"
        "('VA4RAC / VE4RAC','Man.','VE4RAC'),"
        "('VA5RAC / VE5RAC','Sask.','VA5RAC'),"
        "('VA5RAC / VE5RAC','Sask.','VE5RAC'),"
        "('VA6RAC / VE6RAC','Alta.','VA6RAC'),"
        "('VA6RAC / VE6RAC','Alta.','VE6RAC'),"
        "('VA7RAC / VE7RAC','B.C.','VA7RAC'),"
        "('VA7RAC / VE7RAC','B.C.','VE7RAC'),"
        "('VE8RAC','N.W.T.','VE8RAC'),"
        "('VE9RAC','N.B.','VE9RAC'),"
        "('VO1RAC','N.L. (VO1)','VO1RAC'),"
        "('VO2RAC','N.L. (VO2)','VO2RAC'),"
        "('VY0RAC','Nvt.','VY0RAC'),"
        "('VY1RAC','Y.T.','VY1RAC'),"
        "('VY2RAC','P.E.I.','VY2RAC'),"
        "('VA3RHQ / VE3RHQ','RAC HQ','VA3RHQ'),"
        "('VA3RHQ / VE3RHQ','RAC HQ','VE3RHQ'))") };
}

QString AwardWorkedAllRAC::clickFilter(const QString &col1Value, const QString &) const
{
    QStringList quotedCallsigns;
    const QStringList callsigns = col1Value.split(QStringLiteral(" / "));

    for ( QString callsign : callsigns )
    {
        callsign.replace(QLatin1Char('\''), QStringLiteral("''"));
        quotedCallsigns << QStringLiteral("'%1'").arg(callsign);
    }

    return QStringLiteral("callsign IN (%1)").arg(quotedCallsigns.join(QLatin1Char(',')));
}

QString AwardWorkedAllRAC::additionalClickFilter() const
{
    return eligibleContactCondition(QString());
}
