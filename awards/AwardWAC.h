#ifndef QLOG_AWARDS_AWARDWAC_H
#define QLOG_AWARDS_AWARDWAC_H

#include "BandTableAward.h"

class AwardWAC : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("wac"); }
    QString displayName() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QStringList additionalCTEs(const QString &entity, const QString &contactFilter) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDWAC_H
