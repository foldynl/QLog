#ifndef QLOG_AWARDS_AWARDNZ_H
#define QLOG_AWARDS_AWARDNZ_H

#include "SecondarySubdivisionAward.h"

class AwardNZ : public SecondarySubdivisionAward
{
public:
    AwardNZ();
    QString displayName() const override;
};

#endif // QLOG_AWARDS_AWARDNZ_H
