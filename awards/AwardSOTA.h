#ifndef QLOG_AWARDS_AWARDSOTA_H
#define QLOG_AWARDS_AWARDSOTA_H

#include "BandTableAward.h"

class AwardSOTA : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("sota"); }
    QString displayName() const override;
    bool entityInputEnabled() const override { return false; }
    bool notWorkedEnabled() const override { return false; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDSOTA_H
