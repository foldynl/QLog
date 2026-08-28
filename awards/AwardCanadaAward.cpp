#include "AwardCanadaAward.h"
#include <QCoreApplication>

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
    return " FROM adif_enum_primary_subdivision d"
           "   LEFT OUTER JOIN source_contacts c ON d.dxcc = c.dxcc AND d.code = c.state AND c.my_dxcc = '" + entity + "' AND d.dxcc = 1 "
                      "   LEFT OUTER JOIN modes m on c.mode = m.name";
}

QString AwardCanadaAward::additionalWhere(const QString &) const
{
    return " AND d.dxcc = 1 ";
}

QString AwardCanadaAward::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("state = '%1' and dxcc = 1 ").arg(col2Value);
}
