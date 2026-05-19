#include <QCoreApplication>
#include "AwardNZ.h"

AwardNZ::AwardNZ()
    : SecondarySubdivisionAward(QStringLiteral("nz"), QStringLiteral("170"))
{
}

QString AwardNZ::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "NZ Counties");
}
