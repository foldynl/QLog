#include <QCoreApplication>
#include "AwardRDA.h"

AwardRDA::AwardRDA()
    : SecondarySubdivisionAward(QStringLiteral("rda"),
                                QStringLiteral("15, 54, 61, 126, 151"),
                                QStringLiteral("https://rdaward.org/rda_rules_eng.htm"))
{
}

QString AwardRDA::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Russian Districts");
}
