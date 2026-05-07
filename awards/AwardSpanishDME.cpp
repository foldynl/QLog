#include <QCoreApplication>
#include "AwardSpanishDME.h"

AwardSpanishDME::AwardSpanishDME()
    : SecondarySubdivisionAward(QStringLiteral("spanishdme"), QStringLiteral("21, 29, 32, 281"))
{
}

QString AwardSpanishDME::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Spanish DMEs");
}
