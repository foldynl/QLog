#ifndef QLOG_AWARDS_SECONDARYSUBDIVISIONAWARD_H
#define QLOG_AWARDS_SECONDARYSUBDIVISIONAWARD_H

#include "BandTableAward.h"

/* Base class for awards based on adif_enum_secondary_subdivision table.
 *
 * These awards share identical SQL structure — only the DXCC entity filter,
 * display name, and key differ. Subclasses only need to provide identity
 * (key, displayName) and the DXCC filter via constructor. */
class SecondarySubdivisionAward : public BandTableAward
{
public:
    SecondarySubdivisionAward(const QString &key, const QString &dxccFilter);

    QString key() const override { return m_key; }
    bool entityInputEnabled() const override { return false; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;

private:
    QString m_key;
    QString m_dxccFilter;
};

#endif // QLOG_AWARDS_SECONDARYSUBDIVISIONAWARD_H
