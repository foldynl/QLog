#ifndef QLOG_AWARDS_AWARDGRIDSQUARE_H
#define QLOG_AWARDS_AWARDGRIDSQUARE_H

#include "BandTableAward.h"

class AwardGridsquare : public BandTableAward
{
public:
    explicit AwardGridsquare(int chars);

    QString key() const override;
    QString displayName() const override;
    bool notWorkedEnabled() const override { return false; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;

private:
    int m_chars;
};

#endif // QLOG_AWARDS_AWARDGRIDSQUARE_H
