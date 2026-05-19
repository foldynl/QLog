#ifndef QLOG_AWARDS_AWARDJAPAN_H
#define QLOG_AWARDS_AWARDJAPAN_H

#include "SecondarySubdivisionAward.h"

class AwardJapan : public SecondarySubdivisionAward
{
public:
    AwardJapan();
    QString displayName() const override;
};

#endif // QLOG_AWARDS_AWARDJAPAN_H
