#include <QCoreApplication>
#include "AwardWAC.h"


/* https://www.arrl.org/wac */

QString AwardWAC::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WAC");
}

QString AwardWAC::headersColumns(const QString &) const
{
    return QStringLiteral("d.column2 col1, d.column1 col2 ");
}

QString AwardWAC::sqlDetailTable(const QString &entity) const
{
    return " FROM continents d "
           "   LEFT OUTER JOIN source_contacts c ON d.column1 = c.cont AND c.my_dxcc = '" + entity + "'"
           "   LEFT OUTER JOIN modes m on c.mode = m.name ";
}

QString AwardWAC::additionalWhere(const QString &) const
{
    return QString();
}

QStringList AwardWAC::additionalCTEs(const QString &, const QString &) const
{
    return { generateValuesCTE("continents",
                               { { QStringLiteral("NA"), QCoreApplication::translate("AwardsDialog", "North America") },
                                 { QStringLiteral("SA"), QCoreApplication::translate("AwardsDialog", "South America") },
                                 { QStringLiteral("EU"), QCoreApplication::translate("AwardsDialog", "Europe") },
                                 { QStringLiteral("AF"), QCoreApplication::translate("AwardsDialog", "Africa") },
                                 { QStringLiteral("OC"), QCoreApplication::translate("AwardsDialog", "Oceania") },
                                 { QStringLiteral("AS"), QCoreApplication::translate("AwardsDialog", "Asia") }
                               }) };
}

QString AwardWAC::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("cont = '%1'").arg(col2Value);
}
