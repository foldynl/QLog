#ifndef QLOG_AWARDS_AWARDWANA_H
#define QLOG_AWARDS_AWARDWANA_H

#include "BandTableAward.h"

class AwardWANA : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("wana"); }
    QString displayName() const override;
    QString rulesUrl() const override;
    bool requiresEqslAuthenticityGuaranteed() const override { return true; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QStringList additionalCTEs(const QString &entity,
                               const QString &contactFilter) const override;
    QString additionalClickFilter() const override;
    bool clickUsesCountryName() const override;

private:
    QString eligibleContactCondition(const QString &columnPrefix) const;
};

#endif // QLOG_AWARDS_AWARDWANA_H
