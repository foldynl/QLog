#ifndef QLOG_AWARDS_AWARDSPANISHDME_H
#define QLOG_AWARDS_AWARDSPANISHDME_H

#include "SecondarySubdivisionAward.h"

class AwardSpanishDME : public SecondarySubdivisionAward
{
public:
    AwardSpanishDME();
    QString displayName() const override;
};

#endif // QLOG_AWARDS_AWARDSPANISHDME_H
