#include <QCoreApplication>
#include "AwardUSCounty.h"

AwardUSCounty::AwardUSCounty()
    : SecondarySubdivisionAward(QStringLiteral("uscounty"), QStringLiteral("291, 6, 110"))
{
}

QString AwardUSCounty::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "US Counties");
}
