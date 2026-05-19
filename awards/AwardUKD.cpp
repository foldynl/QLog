#include <QCoreApplication>
#include "AwardUKD.h"

AwardUKD::AwardUKD()
    : SecondarySubdivisionAward(QStringLiteral("ukd"), QStringLiteral("288"))
{
}

QString AwardUKD::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Ukrainian Districts");
}
