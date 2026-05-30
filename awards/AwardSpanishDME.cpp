#include <QCoreApplication>
#include "AwardSpanishDME.h"

AwardSpanishDME::AwardSpanishDME()
    : SecondarySubdivisionAward(QStringLiteral("spanishdme"),
                                QStringLiteral("21, 29, 32, 281"),
                                QStringLiteral("https://www.ure.es/dme-award-english-version/"))
{
}

QString AwardSpanishDME::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Spanish DMEs");
}
