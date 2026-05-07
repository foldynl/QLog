#include <QCoreApplication>
#include "AwardSOTA.h"

QString AwardSOTA::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "SOTA");
}

QString AwardSOTA::headersColumns(const QString &) const
{
    return QStringLiteral("s.summit_code col1, NULL col2 ");
}

QString AwardSOTA::sqlDetailTable(const QString &) const
{
    return " FROM sota_summits s "
           "     INNER JOIN source_contacts c ON c.sota_ref = s.summit_code "
           "     INNER JOIN modes m on c.mode = m.name ";
}

QString AwardSOTA::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardSOTA::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("sota_ref = '%1'").arg(col1Value);
}
