#include <QCoreApplication>
#include "AwardGridsquare.h"

AwardGridsquare::AwardGridsquare(int chars)
    : m_chars(chars)
{
}

QString AwardGridsquare::key() const
{
    return QString("grid%1").arg(m_chars);
}

QString AwardGridsquare::displayName() const
{
    switch ( m_chars )
    {
    case 2: return QCoreApplication::translate("AwardsDialog", "Gridsquare 2-Chars");
    case 4: return QCoreApplication::translate("AwardsDialog", "Gridsquare 4-Chars");
    case 6: return QCoreApplication::translate("AwardsDialog", "Gridsquare 6-Chars");
    default: return QCoreApplication::translate("AwardsDialog", "Gridsquare %1-Chars").arg(m_chars);
    }
}

QString AwardGridsquare::headersColumns(const QString &) const
{
    return QString("substr(c.gridsquare, 1, %1) col1, NULL col2 ").arg(m_chars);
}

QString AwardGridsquare::sqlDetailTable(const QString &entity) const
{
    return " FROM source_contacts c"
           "      INNER JOIN modes m ON c.mode = m.name AND c.my_dxcc = '" + entity + "' ";
}

QString AwardGridsquare::additionalWhere(const QString &) const
{
    return QString(" AND length(c.gridsquare) >= %1 ").arg(m_chars);
}

QString AwardGridsquare::clickFilter(const QString &col1Value, const QString &) const
{
    return QString("gridsquare LIKE '%1%%'").arg(col1Value);
}
