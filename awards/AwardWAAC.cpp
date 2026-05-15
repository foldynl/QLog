#include <QCoreApplication>
#include "AwardWAAC.h"

QString AwardWAAC::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WAAC");
}

QString AwardWAAC::headersColumns(const QString &) const
{
    return QStringLiteral("d.name col1, d.id col2 ");
}

QString AwardWAAC::sqlDetailTable(const QString &entity) const
{
    return " FROM dxcc_entities_ad1c d"
           "   LEFT OUTER JOIN source_contacts c ON d.id = c.dxcc AND c.my_dxcc = '" + entity + "' AND d.cont = 'AF' "
           "   LEFT OUTER JOIN modes m on c.mode = m.name";
}

QString AwardWAAC::additionalWhere(const QString &) const
{
    return " AND d.cont = 'AF' ";
}

QString AwardWAAC::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("dxcc = '%1' ").arg(col2Value);
}
