#include <QCoreApplication>
#include "AwardRDA.h"

AwardRDA::AwardRDA()
    : SecondarySubdivisionAward(QStringLiteral("rda"), QStringLiteral("15, 54, 61, 126, 151"))
{
}

QString AwardRDA::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Russian Districts");
}
