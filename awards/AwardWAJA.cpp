#include <QCoreApplication>

#include "AwardWAJA.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.awards.awardwaja");

QString AwardWAJA::key() const
{
    FCT_IDENTIFICATION;

    return QStringLiteral("WAJA");
}

QString AwardWAJA::displayName() const
{
    FCT_IDENTIFICATION;

    return QCoreApplication::translate("AwardsDialog", "WAJA (Worked All Japan Prefectures)");
}

QString AwardWAJA::rulesUrl() const
{
    FCT_IDENTIFICATION;

    return QStringLiteral("https://www.jarl.org/English/4_Library/A-4-2_Awards/Aw_waja.htm");
}

QString AwardWAJA::eligibleContactCondition(const QString &columnPrefix) const
{
    FCT_IDENTIFICATION;

    return QStringLiteral("%1start_time >= '1952-07-29'").arg(columnPrefix);
}

QString AwardWAJA::prefectureCondition(const QString &columnPrefix, const QString &code) const
{
    FCT_IDENTIFICATION;

    // A recorded STATE takes precedence; CNTY supplies a prefecture only when
    // STATE is empty and the full Japanese subdivision code is recognized.
    return QStringLiteral("COALESCE(NULLIF(TRIM(%1state), ''),"
                          " (SELECT SUBSTR(s.code, 1, 2)"
                          " FROM adif_enum_secondary_subdivision s"
                          " WHERE s.dxcc = 339 AND s.code = %1cnty)) = %2")
        .arg(columnPrefix, code);
}

QString AwardWAJA::headersColumns(const QString &) const
{
    FCT_IDENTIFICATION;

    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardWAJA::sqlDetailTable(const QString &entity) const
{
    FCT_IDENTIFICATION;

    // CROSS JOIN keeps contacts as the outer loop for an indexed prefecture
    // lookup. Empty rows keep unworked prefectures visible in the band table.
    return QStringLiteral(" FROM ("
                          " SELECT p.code, p.subdivision_name, p.dxcc, c.id contact_id"
                          " FROM source_contacts c"
                          " CROSS JOIN adif_enum_primary_subdivision p"
                          " ON p.dxcc = 339 AND %2"
                          " WHERE c.dxcc = 339 AND c.my_dxcc = '%1' AND %3"
                          " UNION ALL"
                          " SELECT p.code, p.subdivision_name, p.dxcc, NULL"
                          " FROM adif_enum_primary_subdivision p WHERE p.dxcc = 339"
                          " ) d"
                          " LEFT OUTER JOIN contacts c ON c.id = d.contact_id"
                          " LEFT OUTER JOIN modes m ON c.mode = m.name")
        .arg(entity,
             prefectureCondition(QStringLiteral("c."), QStringLiteral("p.code")),
             eligibleContactCondition(QStringLiteral("c.")));
}

QString AwardWAJA::additionalWhere(const QString &) const
{
    FCT_IDENTIFICATION;

    return QStringLiteral(" AND d.dxcc = 339 ");
}

QString AwardWAJA::clickFilter(const QString &, const QString &col2Value) const
{
    FCT_IDENTIFICATION;

    return QStringLiteral("dxcc = 339 AND %1")
        .arg(prefectureCondition(QString(), QStringLiteral("'%1'").arg(col2Value)));
}

QString AwardWAJA::additionalClickFilter() const
{
    FCT_IDENTIFICATION;

    return eligibleContactCondition(QString());
}
