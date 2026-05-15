#ifndef QLOG_AWARDS_AWARDWAIP_H
#define QLOG_AWARDS_AWARDWAIP_H

#include "BandTableAward.h"

class AwardWAIP : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("WAIP"); }
    QString displayName() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDWAIP_H