#include "AwardWorkedAllRAC.h"
#include <QCoreApplication>

QString AwardWorkedAllRAC::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Worked All RAC");
}

QString AwardWorkedAllRAC::rulesUrl() const
{
    return QStringLiteral("https://www.rac.ca/worked-all-rac/");
}

QString AwardWorkedAllRAC::headersColumns(const QString &) const
{
    return QStringLiteral("c.callsign col1, null col2 ");
}

QString AwardWorkedAllRAC::sqlDetailTable(const QString &entity) const
{
    return " FROM source_contacts c "
                      "   LEFT OUTER JOIN modes m on c.mode = m.name AND c.my_dxcc = '" + entity + "' ";
}

QString AwardWorkedAllRAC::additionalWhere(const QString &) const
{
    return " AND c.callsign in ('VA1RAC','VA2RAC','VA3RAC','VA4RAC','VA5RAC','VA6RAC','VA7RAC','VE1RAC','VE3RAC','VE4RAC','VE5RAC','VE6RAC',' VE7RAC','VE8RAC','VE9RAC','VO1RAC','VO2RAC','VY0RAC','VY1RAC','VY2RAC','VA3RHQ','VE3RHQ') ";
}

QString AwardWorkedAllRAC::clickFilter(const QString &, const QString &col1Value) const
{
    return QString("callsign = '%1' ").arg(col1Value);
}



