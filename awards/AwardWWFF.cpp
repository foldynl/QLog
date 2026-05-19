#include <QCoreApplication>
#include "AwardWWFF.h"

QString AwardWWFF::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WWFF");
}

QString AwardWWFF::headersColumns(const QString &) const
{
    return QStringLiteral("w.reference col1, w.name col2 ");
}

QString AwardWWFF::sqlDetailTable(const QString &) const
{
    return " FROM wwff_directory w "
           "     INNER JOIN source_contacts c ON c.wwff_ref = w.reference "
           "     INNER JOIN modes m on c.mode = m.name ";
}

QString AwardWWFF::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardWWFF::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("wwff_ref = '%1'").arg(col1Value);
}
