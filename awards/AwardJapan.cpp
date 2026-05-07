#include <QCoreApplication>
#include "AwardJapan.h"

AwardJapan::AwardJapan()
    : SecondarySubdivisionAward(QStringLiteral("japan"), QStringLiteral("339"))
{
}

QString AwardJapan::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Japanese Cities/Ku/Guns");
}
