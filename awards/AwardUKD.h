#ifndef QLOG_AWARDS_AWARDUKD_H
#define QLOG_AWARDS_AWARDUKD_H

#include "SecondarySubdivisionAward.h"

class AwardUKD : public SecondarySubdivisionAward
{
public:
    AwardUKD();
    QString displayName() const override;
};

#endif // QLOG_AWARDS_AWARDUKD_H
