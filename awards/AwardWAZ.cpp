#include <QCoreApplication>
#include "AwardWAZ.h"

QString AwardWAZ::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WAZ");
}

QString AwardWAZ::headersColumns(const QString &) const
{
    return QStringLiteral("d.n col1, null col2 ");
}

QString AwardWAZ::sqlDetailTable(const QString &entity) const
{
    return " FROM cqzCTE d "
           "   LEFT OUTER JOIN source_contacts c ON d.n = c.cqz AND c.my_dxcc = '" + entity + "'"
           "   LEFT OUTER JOIN modes m on c.mode = m.name ";
}

QString AwardWAZ::additionalWhere(const QString &) const
{
    return QString();
}

QStringList AwardWAZ::additionalCTEs(const QString &, const QString &) const
{
    return { generateNumberRangeCTE("cqzCTE", 1, 40) };
}

QString AwardWAZ::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("cqz = '%1'").arg(col1Value);
}
