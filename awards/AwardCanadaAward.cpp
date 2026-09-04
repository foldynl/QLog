#include <QCoreApplication>

#include "AwardCanadaAward.h"

QString AwardCanadaAward::eligibleContactCondition(const QString &columnPrefix) const
{
    return QStringLiteral("%1start_time >= '1977-07-02'"
                          " AND UPPER(%1callsign) NOT LIKE 'VE0%'"
                          " AND UPPER(COALESCE(%1prop_mode, '')) <> 'RPT'").arg(columnPrefix);
}

QString AwardCanadaAward::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Canada Award");
}

QString AwardCanadaAward::rulesUrl() const
{
    return QStringLiteral("https://www.rac.ca/canadaward/");
}

QString AwardCanadaAward::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardCanadaAward::sqlDetailTable(const QString &entity) const
{
    return QStringLiteral(" FROM adif_enum_primary_subdivision d"
                          "   LEFT OUTER JOIN source_contacts c"
                          "     ON d.dxcc = c.dxcc"
                          "    AND d.code = c.state"
                          "    AND c.my_dxcc = '%1'"
                          "    AND %2"
                          "   LEFT OUTER JOIN modes m ON c.mode = m.name")
        .arg(entity, eligibleContactCondition(QStringLiteral("c.")));
}

QString AwardCanadaAward::additionalWhere(const QString &) const
{
    return QStringLiteral(" AND d.dxcc = 1 ");
}

QString AwardCanadaAward::clickFilter(const QString &, const QString &col2Value) const
{
    return QStringLiteral("state = '%1' AND dxcc = 1").arg(col2Value);
}

QString AwardCanadaAward::additionalClickFilter() const
{
    return eligibleContactCondition(QString());
}
