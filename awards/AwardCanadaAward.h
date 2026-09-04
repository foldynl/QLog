#ifndef QLOG_AWARDS_AWARDCANADAAWARD_H
#define QLOG_AWARDS_AWARDCANADAAWARD_H

#include "BandTableAward.h"

class AwardCanadaAward : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("canadaward"); }
    QString displayName() const override;
    QString rulesUrl() const override;
    bool requiresEqslAuthenticityGuaranteed() const override { return true; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value,
                        const QString &col2Value) const override;
    QString additionalClickFilter() const override;

private:
    QString eligibleContactCondition(const QString &columnPrefix) const;
};

#endif // QLOG_AWARDS_AWARDCANADAAWARD_H
