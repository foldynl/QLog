#ifndef QLOG_AWARDS_AWARDWAS_H
#define QLOG_AWARDS_AWARDWAS_H

#include "BandTableAward.h"

class AwardWAS : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("was"); }
    QString displayName() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDWAS_H
