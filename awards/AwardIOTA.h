#ifndef QLOG_AWARDS_AWARDIOTA_H
#define QLOG_AWARDS_AWARDIOTA_H

#include "BandTableAward.h"

class AwardIOTA : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("iota"); }
    QString displayName() const override;
    bool notWorkedEnabled() const override { return false; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDIOTA_H
