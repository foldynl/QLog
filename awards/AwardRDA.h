#ifndef QLOG_AWARDS_AWARDRDA_H
#define QLOG_AWARDS_AWARDRDA_H

#include "SecondarySubdivisionAward.h"

class AwardRDA : public SecondarySubdivisionAward
{
public:
    AwardRDA();
    QString displayName() const override;
};

#endif // QLOG_AWARDS_AWARDRDA_H
