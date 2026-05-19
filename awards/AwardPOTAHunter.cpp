#include <QCoreApplication>
#include "AwardPOTAHunter.h"

QString AwardPOTAHunter::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "POTA Hunter");
}

QString AwardPOTAHunter::headersColumns(const QString &) const
{
    return QStringLiteral("p.reference col1, p.name col2 ");
}

QString AwardPOTAHunter::sqlDetailTable(const QString &) const
{
    return " FROM pota_directory p "
           "      INNER JOIN source_contacts c ON SUBSTR(c.pota, 1, COALESCE(NULLIF(INSTR(c.pota, '@'), 0) - 1, LENGTH(c.pota))) = p.reference"
           "      INNER JOIN modes m on c.mode = m.name ";
}

QString AwardPOTAHunter::additionalWhere(const QString &) const
{
    return " AND c.pota is not NULL ";
}

QStringList AwardPOTAHunter::additionalCTEs(const QString &, const QString &contactFilter) const
{
    return { generateSplitCTE("pota_ref", "pota", contactFilter) };
}

QString AwardPOTAHunter::sourceContactsOverride(const QString &) const
{
    return generateSplitSourceContacts("pota");
}

QString AwardPOTAHunter::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("pota_ref LIKE '%%1%'").arg(col1Value);
}
