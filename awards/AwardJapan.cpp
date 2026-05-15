#include <QCoreApplication>
#include "AwardJapan.h"

AwardJapan::AwardJapan()
    : SecondarySubdivisionAward(QStringLiteral("japan"),
                                QStringLiteral("339"),
                                QStringLiteral("https://www.jarl.org/English/4_Library/A-4-2_Awards/Award_Main.htm"))
{
}

QString AwardJapan::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Japanese Cities/Ku/Guns");
}
