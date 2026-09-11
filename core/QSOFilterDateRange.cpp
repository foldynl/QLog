#include "QSOFilterDateRange.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.qsofilterdaterange");

namespace
{
using Anchor = QSOFilterDateRange::Boundary::Anchor;

// Persistent tokens, not UI labels. Never translate or rename these.
const struct { Anchor anchor; const char *token; } relativeAnchors[] = {
    {Anchor::Today, "TODAY"},
    {Anchor::WeekStart, "WEEK_START"},
    {Anchor::MonthStart, "MONTH_START"},
    {Anchor::YearStart, "YEAR_START"},
    {Anchor::YearEnd, "YEAR_END"}
};
const QLatin1String fromKey("from"), toKey("to");

QDateTime utcDateTime(const QDate &date)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    return QDateTime(date, QTime(0, 0), QTimeZone::UTC);
#else
    return QDateTime(date, QTime(0, 0), Qt::UTC);
#endif
}
}

QString QSOFilterDateRange::toString() const
{
    return QString::fromUtf8(QJsonDocument(QJsonObject{{fromKey, from}, {toKey, to}})
                             .toJson(QJsonDocument::Compact));
}

bool QSOFilterDateRange::fromString(const QString &value, QSOFilterDateRange &range)
{
    const QJsonObject object = QJsonDocument::fromJson(value.toUtf8()).object();
    if ( !object.value(fromKey).isString() || !object.value(toKey).isString() )
        return false;

    range.from = object.value(fromKey).toString();
    range.to = object.value(toKey).toString();
    return true;
}

QSOFilterDateRange::Boundary QSOFilterDateRange::Boundary::fromString(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral("^([A-Z_]+)([+-][0-9]+)?$"),
                                               QRegularExpression::CaseInsensitiveOption);
    const auto match = expression.match(value);
    if ( match.hasMatch() )
    {
        bool ok = true;
        const int offset = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt(&ok);
        if ( ok )
            for ( const auto &entry : relativeAnchors )
                if ( match.captured(1).compare(QLatin1String(entry.token), Qt::CaseInsensitive) == 0 )
                    return Boundary(entry.anchor, offset);
        return Boundary(Anchor::Invalid);
    }

    Boundary boundary(value.size() == 10 ? Anchor::Date : Anchor::DateTime);
    boundary.fixedDateTime = boundary.anchor == Anchor::Date
                              ? utcDateTime(QDate::fromString(value, Qt::ISODate))
                              : QDateTime::fromString(value, Qt::ISODate);
    if ( !boundary.fixedDateTime.isValid() ) return Boundary(Anchor::Invalid);

    // A timestamp without a zone is UTC, just like existing filters.
    if ( boundary.fixedDateTime.timeSpec() == Qt::LocalTime )
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        boundary.fixedDateTime.setTimeZone(QTimeZone::UTC);
#else
        boundary.fixedDateTime.setTimeSpec(Qt::UTC);
#endif
    }
    boundary.fixedDateTime = boundary.fixedDateTime.toUTC();
    return boundary;
}

QString QSOFilterDateRange::Boundary::toString() const
{
    if ( anchor == Anchor::Date ) return fixedDateTime.date().toString(Qt::ISODate);
    if ( anchor == Anchor::DateTime ) return fixedDateTime.toString("yyyy-MM-ddTHH:mm:ss");

    for ( const auto &entry : relativeAnchors )
        if ( entry.anchor == anchor )
            return QLatin1String(entry.token) + (offsetDays == 0 ? QString()
                    : (offsetDays > 0 ? QString("+%1").arg(offsetDays) : QString::number(offsetDays)));
    return {};
}

bool QSOFilterDateRange::Boundary::isRelative() const
{
    return anchor != Anchor::Invalid && anchor != Anchor::Date && anchor != Anchor::DateTime;
}

QDateTime QSOFilterDateRange::Boundary::resolve(const QDate &today, bool end) const
{
    QDate date;
    switch ( anchor )
    {
    case Anchor::Date:       date = fixedDateTime.date(); break;
    case Anchor::DateTime:   return end ? fixedDateTime.addSecs(1) : fixedDateTime;
    case Anchor::Today:      date = today; break;
    case Anchor::WeekStart:  date = today.addDays(1 - today.dayOfWeek()); break;
    case Anchor::MonthStart: date = QDate(today.year(), today.month(), 1); break;
    case Anchor::YearStart:  date = QDate(today.year(), 1, 1); break;
    case Anchor::YearEnd:    date = QDate(today.year(), 12, 31); break;
    case Anchor::Invalid:    return {};
    }
    date = date.addDays(offsetDays);
    return utcDateTime(end ? date.addDays(1) : date);
}

bool QSOFilterDateRange::resolve(const QDate &today, QDateTime &start, QDateTime &end) const
{
    FCT_IDENTIFICATION;

    start = Boundary::fromString(from).resolve(today, false);
    end = Boundary::fromString(to).resolve(today, true);
    return start.isValid() && end.isValid() && start < end;
}

bool QSOFilterDateRange::isRelative() const
{
    FCT_IDENTIFICATION;

    return Boundary::fromString(from).isRelative() || Boundary::fromString(to).isRelative();
}
