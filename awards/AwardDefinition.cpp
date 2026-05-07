#include "AwardDefinition.h"

bool AwardDefinition::entityInputEnabled() const
{
    return true;
}

bool AwardDefinition::notWorkedEnabled() const
{
    return true;
}

AwardDefinition::ConditionResult AwardDefinition::getConditionSelected(const QModelIndex &) const
{
    return ConditionResult();
}
