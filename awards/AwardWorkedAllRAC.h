#ifndef AWARDWORKEDALLRAC_H
#define AWARDWORKEDALLRAC_H

#include "BandTableAward.h"

class AwardWorkedAllRAC: public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("WorkedAllRAC"); }
    QString displayName() const override;
    QString rulesUrl() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // AWARDWORKEDALLRAC_H

