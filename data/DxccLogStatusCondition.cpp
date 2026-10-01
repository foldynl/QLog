#include "data/DxccLogStatusCondition.h"
#include "data/DxSpot.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.dxcclogstatuscondition");

DxccLogStatusCondition::DxccLogStatusCondition(int mask) : mask(mask)
{
    FCT_IDENTIFICATION;
}

bool DxccLogStatusCondition::match(const DxSpot &spot,
                                  const DxccConfirmationResolver &resolver) const
{
    FCT_IDENTIFICATION;

    if ( !goalSet && !contextSet )
        return spot.status & mask;

    if ( !hasCondition() )
        return false;

    if ( spot.status == DxccStatus::UnknownStatus )
        return false;

    const AlertDxccGoal goal = static_cast<AlertDxccGoal>(this->goal);
    const AlertDxccContext context = static_cast<AlertDxccContext>(this->context);

    if ( spot.dxccStatusSatellite )
    {
        return goal == AlertDxccGoal::Worked
                   ? spot.status == DxccStatus::NewEntity
                   : spot.status != DxccStatus::Confirmed;
    }

    if ( goal == AlertDxccGoal::Worked )
    {
        switch ( context )
        {
            case AlertDxccContext::Anywhere:
                return spot.status == DxccStatus::NewEntity;
            case AlertDxccContext::Band:
                return spot.status == DxccStatus::NewEntity
                    || spot.status == DxccStatus::NewBand
                    || spot.status == DxccStatus::NewBandMode;
            case AlertDxccContext::Mode:
                return spot.status == DxccStatus::NewEntity
                    || spot.status == DxccStatus::NewMode
                    || spot.status == DxccStatus::NewBandMode;
            case AlertDxccContext::BandMode:
                return spot.status == DxccStatus::NewEntity
                    || spot.status == DxccStatus::NewBand
                    || spot.status == DxccStatus::NewMode
                    || spot.status == DxccStatus::NewBandMode
                    || spot.status == DxccStatus::NewSlot;
        }
    }

    if ( context == AlertDxccContext::BandMode )
        return spot.status != DxccStatus::Confirmed;

    if ( spot.status == DxccStatus::Confirmed )
        return false;

    if ( context == AlertDxccContext::Anywhere
         && spot.status == DxccStatus::NewEntity )
        return true;

    if ( context == AlertDxccContext::Band
         && (spot.status == DxccStatus::NewEntity
             || spot.status == DxccStatus::NewBand
             || spot.status == DxccStatus::NewBandMode) )
        return true;

    if ( context == AlertDxccContext::Mode
         && (spot.status == DxccStatus::NewEntity
             || spot.status == DxccStatus::NewMode
             || spot.status == DxccStatus::NewBandMode) )
        return true;

    if ( !resolver )
        return false;

    const DxccConfirmationResult result = resolver(spot.dxcc.dxcc,
                                                   spot.band,
                                                   spot.modeGroupString,
                                                   context);

    return result == DxccConfirmationResult::NotConfirmed;
}

bool DxccLogStatusCondition::hasCondition() const
{
    FCT_IDENTIFICATION;

    return goalSet && contextSet
        && (goal == static_cast<int>(AlertDxccGoal::Worked)
            || goal == static_cast<int>(AlertDxccGoal::Confirmed))
        && context >= static_cast<int>(AlertDxccContext::Anywhere)
        && context <= static_cast<int>(AlertDxccContext::BandMode);
}

bool DxccLogStatusCondition::isInvalid() const
{
    FCT_IDENTIFICATION;

    return (goalSet || contextSet) && !hasCondition();
}

bool DxccLogStatusCondition::isFiltered() const
{
    FCT_IDENTIFICATION;

    return goalSet || contextSet
        || (mask != DxccStatus::All && mask != (DxccStatus::All & ~DxccStatus::UnknownStatus));
}

QString DxccLogStatusCondition::toJson() const
{
    FCT_IDENTIFICATION;

    QJsonObject object;
    object.insert(QStringLiteral("mask"), mask);
    if ( goalSet )
        object.insert(QStringLiteral("goal"), goal);
    if ( contextSet )
        object.insert(QStringLiteral("context"), context);
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

DxccLogStatusCondition DxccLogStatusCondition::fromJson(const QString &value)
{
    FCT_IDENTIFICATION;

    const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8());
    const QJsonObject object = document.object();
    DxccLogStatusCondition condition;
    const QJsonValue maskValue = object.value(QStringLiteral("mask"));
    if ( !document.isObject() || !maskValue.isDouble()
         || maskValue.toDouble() != maskValue.toInt(-1) )
    {
        condition.goal = -1;
        condition.goalSet = true;
        return condition;
    }

    condition.mask = maskValue.toInt();
    condition.goalSet = object.contains(QStringLiteral("goal"));
    condition.contextSet = object.contains(QStringLiteral("context"));
    condition.goal = object.value(QStringLiteral("goal")).toInt(-1);
    condition.context = object.value(QStringLiteral("context")).toInt(-1);
    return condition;
}
