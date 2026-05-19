#ifndef QLOG_AWARDS_AWARDWWFF_H
#define QLOG_AWARDS_AWARDWWFF_H

#include "BandTableAward.h"

class AwardWWFF : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("wwff"); }
    QString displayName() const override;
    bool entityInputEnabled() const override { return false; }
    bool notWorkedEnabled() const override { return false; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDWWFF_H
