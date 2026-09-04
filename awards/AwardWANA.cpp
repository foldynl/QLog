#include <QCoreApplication>

#include "AwardWANA.h"

QString AwardWANA::eligibleContactCondition(const QString &columnPrefix) const
{
    return QStringLiteral("%1start_time >= '1946-01-01'"
                          " AND UPPER(%1callsign) NOT LIKE '%/MM%'"
                          " AND UPPER(COALESCE(%1prop_mode, '')) <> 'RPT'").arg(columnPrefix);
}

QString AwardWANA::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Worked All North America (WANA)");
}

QString AwardWANA::rulesUrl() const
{
    return QStringLiteral("https://www.rac.ca/worked-all-north-america/");
}

QString AwardWANA::headersColumns(const QString &) const
{
    return QStringLiteral("translate_to_locale(d.name) col1, d.prefix col2 ");
}

QString AwardWANA::sqlDetailTable(const QString &entity) const
{
    return QStringLiteral(" FROM wana_entities w"
                          "   INNER JOIN dxcc_entities_clublog d ON d.id = w.id"
                          "   LEFT OUTER JOIN source_contacts c"
                          "     ON d.id = c.dxcc"
                          "    AND c.my_dxcc = '%1'"
                          "    AND %2"
                          "   LEFT OUTER JOIN modes m ON c.mode = m.name")
        .arg(entity, eligibleContactCondition(QStringLiteral("c.")));
}

QString AwardWANA::additionalWhere(const QString &) const
{
    return QStringLiteral(" AND d.deleted = 0 ");
}

QStringList AwardWANA::additionalCTEs(const QString &, const QString &) const
{
    // Official RAC WANA list, last updated 2023-06-18. DXCC IDs are stable
    // across prefix changes and use the same identifiers as stored contacts.
    return { QStringLiteral(
        " wana_entities(id) AS (VALUES "
        "(289),(82),(62),(60),(70),(211),(252),(79),(516),(84),"
        "(36),(277),(213),(78),(72),(216),(88),(80),(77),(97),"
        "(95),(98),(291),(105),(6),(182),(285),(202),(43),(237),"
        "(519),(518),(76),(308),(37),(94),(66),(249),(1),(12),"
        "(96),(65),(89),(64),(50),(204),(86),(74),(17),(69))") };
}

QString AwardWANA::additionalClickFilter() const
{
    return eligibleContactCondition(QString());
}

bool AwardWANA::clickUsesCountryName() const
{
    return true;
}
