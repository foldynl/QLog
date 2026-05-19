#ifndef QLOG_AWARDS_AWARDUSCOUNTY_H
#define QLOG_AWARDS_AWARDUSCOUNTY_H

#include "SecondarySubdivisionAward.h"

class AwardUSCounty : public SecondarySubdivisionAward
{
public:
    AwardUSCounty();
    QString displayName() const override;
};

#endif // QLOG_AWARDS_AWARDUSCOUNTY_H
