#include <QCoreApplication>
#include "AwardDXCC.h"

QString AwardDXCC::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "DXCC");
}

QString AwardDXCC::headersColumns(const QString &) const
{
    return QStringLiteral("translate_to_locale(d.name) col1, d.prefix col2 ");
}

QString AwardDXCC::sqlDetailTable(const QString &entity) const
{
    return " FROM (SELECT id, name, prefix FROM dxcc_entities_clublog WHERE deleted = 0 "
           "       UNION "
           "       SELECT DISTINCT dxcc, b.name, b.prefix || ' ("
           + QCoreApplication::translate("AwardsDialog", "DELETED") + ")' "
           "             FROM source_contacts a INNER JOIN dxcc_entities_clublog b ON a.dxcc = b.id AND b.deleted = 1 where a.my_dxcc = '" + entity + "') d "
           "   LEFT OUTER JOIN source_contacts c ON (d.id = c.dxcc AND (c.id IS NULL OR c.my_dxcc = '" + entity + "'))"
           "   LEFT OUTER JOIN modes m on c.mode = m.name ";
}

QString AwardDXCC::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardDXCC::clickFilter(const QString &, const QString &) const
{
    return QString();
}

bool AwardDXCC::clickUsesCountryName() const
{
    return true;
}
