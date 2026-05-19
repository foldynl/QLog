#include <QCoreApplication>
#include "AwardWPX.h"

QString AwardWPX::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WPX");
}

QString AwardWPX::headersColumns(const QString &) const
{
    return QStringLiteral("c.pfx col1, null col2 ");
}

QString AwardWPX::sqlDetailTable(const QString &entity) const
{
    return " FROM source_contacts c"
           "      INNER JOIN modes m ON c.mode = m.name AND c.my_dxcc = '" + entity + "'";
}

QString AwardWPX::additionalWhere(const QString &) const
{
    return " AND c.pfx is not null ";
}

QString AwardWPX::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("pfx = '%1'").arg(col1Value);
}
