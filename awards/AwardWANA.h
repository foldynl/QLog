#ifndef QLOG_AWARDS_AWARDWANA_H
#define QLOG_AWARDS_AWARDWANA_H

#include "BandTableAward.h"

class AwardWANA : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("WANA"); }
    QString displayName() const override;
    QString rulesUrl() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QStringList additionalCTEs(const QString &entity,
                               const QString &contactFilter) const override;
    bool clickUsesCountryName() const override;
};

#endif // QLOG_AWARDS_AWARDWANA_H
