#ifndef QLOG_AWARDS_AWARDCANADAAWARD_H
#define QLOG_AWARDS_AWARDCANADAAWARD_H

#include "BandTableAward.h"

class AwardCanadaAward: public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("CANADAAWARD"); }
    QString displayName() const override;
    QString rulesUrl() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDWAIP_H
