#include <QCoreApplication>
#include "AwardPOTAActivator.h"

QString AwardPOTAActivator::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "POTA Activator");
}

QString AwardPOTAActivator::headersColumns(const QString &) const
{
    return QStringLiteral("p.reference col1, p.name col2 ");
}

QString AwardPOTAActivator::sqlDetailTable(const QString &) const
{
    return " FROM pota_directory p "
           "      INNER JOIN source_contacts c ON SUBSTR(c.my_pota_ref_str, 1, COALESCE(NULLIF(INSTR(c.my_pota_ref_str, '@'), 0) - 1, LENGTH(c.my_pota_ref_str))) = p.reference"
           "      INNER JOIN modes m on c.mode = m.name ";
}

QString AwardPOTAActivator::additionalWhere(const QString &) const
{
    return " AND c.my_pota_ref_str is not NULL ";
}

QStringList AwardPOTAActivator::additionalCTEs(const QString &, const QString &contactFilter) const
{
    return { generateSplitCTE("my_pota_ref", "my_pota_ref_str", contactFilter) };
}

QString AwardPOTAActivator::sourceContactsOverride(const QString &) const
{
    return generateSplitSourceContacts("my_pota_ref_str");
}

QString AwardPOTAActivator::clickFilter(const QString &col1Value, const QString &) const
{
    // Bug fix: original had "my_pota_ref LIKE = '%%1%'" (extra '=')
    return QString("my_pota_ref LIKE '%%1%'").arg(col1Value);
}
