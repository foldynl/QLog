#include "SecondarySubdivisionAward.h"

SecondarySubdivisionAward::SecondarySubdivisionAward(const QString &key, const QString &dxccFilter)
    : m_key(key),
      m_dxccFilter(dxccFilter)
{
}

QString SecondarySubdivisionAward::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString SecondarySubdivisionAward::sqlDetailTable(const QString &) const
{
    return " FROM adif_enum_secondary_subdivision d "
           "   LEFT OUTER JOIN (SELECT * FROM source_contacts "
           "                    WHERE dxcc IN (" + m_dxccFilter + ") "
           "                      AND cnty IS NOT NULL AND cnty != '') c "
           "     ON d.dxcc = c.dxcc AND upper(d.code) = upper(c.cnty) "
           "   LEFT OUTER JOIN modes m ON c.mode = m.name ";
}

QString SecondarySubdivisionAward::additionalWhere(const QString &) const
{
    return " AND d.dxcc IN (" + m_dxccFilter + ") ";
}

QString SecondarySubdivisionAward::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("upper(cnty) = upper('%1') and dxcc in (%2)").arg(col2Value, m_dxccFilter);
}
