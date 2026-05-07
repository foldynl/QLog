#include <QCoreApplication>
#include "AwardIOTA.h"

QString AwardIOTA::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "IOTA");
}

QString AwardIOTA::headersColumns(const QString &) const
{
    return QStringLiteral("c.iota col1, NULL col2 ");
}

QString AwardIOTA::sqlDetailTable(const QString &entity) const
{
    return " FROM source_contacts c"
           "      INNER JOIN modes m ON c.mode = m.name AND c.my_dxcc = '" + entity + "'";
}

QString AwardIOTA::additionalWhere(const QString &) const
{
    return " AND c.iota is not NULL ";
}

QString AwardIOTA::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("iota = '%1'").arg(col1Value);
}
