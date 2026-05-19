#include <QCoreApplication>
#include "AwardITU.h"

QString AwardITU::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "ITU");
}

QString AwardITU::headersColumns(const QString &) const
{
    return QStringLiteral("d.n col1, null col2 ");
}

QString AwardITU::sqlDetailTable(const QString &entity) const
{
    return " FROM ituzCTE d "
           "   LEFT OUTER JOIN source_contacts c ON d.n = c.ituz AND c.my_dxcc = '" + entity + "'"
           "   LEFT OUTER JOIN modes m on c.mode = m.name";
}

QString AwardITU::additionalWhere(const QString &) const
{
    return QString();
}

QStringList AwardITU::additionalCTEs(const QString &, const QString &) const
{
    return { generateNumberRangeCTE("ituzCTE", 1, 90) };
}

QString AwardITU::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("ituz = '%1'").arg(col1Value);
}
