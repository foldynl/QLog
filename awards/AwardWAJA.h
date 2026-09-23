#ifndef QLOG_AWARDS_AWARDWAJA_H
#define QLOG_AWARDS_AWARDWAJA_H

#include "BandTableAward.h"

class AwardWAJA : public BandTableAward
{
public:
    QString key() const override;
    QString displayName() const override;
    QString rulesUrl() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
    QString additionalClickFilter() const override;

private:
    QString eligibleContactCondition(const QString &columnPrefix) const;
    QString prefectureCondition(const QString &columnPrefix, const QString &code) const;
};

#endif // QLOG_AWARDS_AWARDWAJA_H
