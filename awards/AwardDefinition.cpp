#include "AwardDefinition.h"

bool AwardDefinition::entityInputEnabled() const
{
    return true;
}

bool AwardDefinition::notWorkedEnabled() const
{
    return true;
}

bool AwardDefinition::acceptsEqslConfirmation() const
{
    return true;
}

bool AwardDefinition::requiresEqslAuthenticityGuaranteed() const
{
    return false;
}

QString AwardDefinition::rulesUrl() const
{
    return QString();
}

AwardDefinition::ConditionResult AwardDefinition::getConditionSelected(const QModelIndex &) const
{
    return ConditionResult();
}
