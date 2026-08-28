#include <QCoreApplication>

#include "AwardWANA.h"

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
    return " FROM wana_entities w"
           "   INNER JOIN dxcc_entities_clublog d ON d.id = w.id"
           "   LEFT OUTER JOIN source_contacts c ON d.id = c.dxcc"
           "       AND c.my_dxcc = '" + entity + "'"
           "       AND c.start_time >= '1946-01-01'"
           "       AND UPPER(c.callsign) NOT LIKE '%/MM%'"
           "   LEFT OUTER JOIN modes m ON c.mode = m.name ";
}

QString AwardWANA::additionalWhere(const QString &) const
{
    // WANA rules exclude deleted DXCC entities.
    return QStringLiteral(" AND d.deleted = 0 ");
}

QStringList AwardWANA::additionalCTEs(const QString &, const QString &) const
{
    // RAC's WANA entity list, last updated 2023-06-18.  Store the DXCC IDs,
    // rather than prefixes, so the result follows QLog's date-aware DXCC data.
    return { QStringLiteral(
        " wana_entities(id) AS (VALUES "
        "(289),(82),(62),(60),(70),(211),(252),(79),(516),(84),"
        "(36),(277),(213),(78),(72),(216),(88),(80),(77),(97),"
        "(95),(98),(291),(105),(6),(182),(285),(202),(43),(237),"
        "(519),(518),(76),(308),(37),(94),(66),(249),(1),(12),"
        "(96),(65),(89),(64),(50),(204),(86),(74),(17),(69))") };
}

bool AwardWANA::clickUsesCountryName() const
{
    return true;
}
