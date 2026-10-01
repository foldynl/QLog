#include "data/Data.h"
#include "data/BandPlan.h"
#include "data/RigProfile.h"

// Spot styling and physical port selection are outside these report tests.
QColor Data::statusToColor(const DxccStatus &, bool, const QColor &defaultColor)
{
    return defaultColor;
}

const QString BandPlan::bandPlanMode2ExpectedMode(const BandPlanMode &, QString &)
{
    return QString();
}

bool BandPlan::isFTxBandMode(BandPlanMode)
{
    return false;
}

RigProfile::rigPortType RigProfile::getPortType() const
{
    return SERIAL_ATTACHED;
}
