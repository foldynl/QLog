#include <QCoreApplication>
#include "AwardJapan.h"

AwardJapan::AwardJapan()
    : SecondarySubdivisionAward(QStringLiteral("japan"),
                                QStringLiteral("339"),
                                QStringLiteral("https://www.jarl.org/Japanese/A_Shiryo/A-2_jcc-jcg/jcc.htm"))
{
}

QString AwardJapan::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Japanese Cities/Ku/Guns");
}
