#ifndef QLOG_AWARDS_AWARDDXCC_H
#define QLOG_AWARDS_AWARDDXCC_H

#include "BandTableAward.h"

class AwardDXCC : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("dxcc"); }
    QString displayName() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
    bool clickUsesCountryName() const override;
};

#endif // QLOG_AWARDS_AWARDDXCC_H
