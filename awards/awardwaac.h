#ifndef QLOG_AWARDS_AWARDWAAC_H
#define QLOG_AWARDS_AWARDWAAC_H

#include "BandTableAward.h"

class AwardWAAC : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("WAAC"); }
    QString displayName() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDWAAC_H