#include <QCoreApplication>
#include "awardwaip.h"

QString AwardWAIP::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WAIP");
}

QString AwardWAIP::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardWAIP::sqlDetailTable(const QString &entity) const
{
    return " FROM adif_enum_primary_subdivision d"
           "   LEFT OUTER JOIN source_contacts c ON d.dxcc = c.dxcc AND d.code = c.state AND c.my_dxcc = '" + entity + "' AND d.dxcc in (225, 248)"
                      "   LEFT OUTER JOIN modes m on c.mode = m.name";
}

QString AwardWAIP::additionalWhere(const QString &) const
{
    return " AND d.dxcc in (225, 248) ";
}

QString AwardWAIP::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("state = '%1' and dxcc in (225, 248)").arg(col2Value);
}
