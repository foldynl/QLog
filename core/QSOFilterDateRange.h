#ifndef QLOG_CORE_QSOFILTERDATERANGE_H
#define QLOG_CORE_QSOFILTERDATERANGE_H

#include <QDateTime>
#include <QString>

// Date boundaries include the entire day; date-time boundaries include the
// selected second. Relative boundaries are calendar dates in UTC.
struct QSOFilterDateRange
{
    struct Boundary
    {
        enum class Anchor
        {
            Invalid,
            Date,
            DateTime,
            Today,
            WeekStart,
            MonthStart,
            YearStart,
            YearEnd
        };

        explicit Boundary(Anchor anchor = Anchor::Today, int offsetDays = 0)
            : anchor(anchor), offsetDays(offsetDays) {}

        Anchor anchor;
        int offsetDays;
        QDateTime fixedDateTime;

        static Boundary fromString(const QString &value);
        QString toString() const;
        QDateTime resolve(const QDate &today, bool end) const;
        bool isRelative() const;
        bool isRelativeTo(Anchor reference, int days = 0) const
        {
            return isRelative() && anchor == reference && offsetDays == days;
        }
    };

    // Keep the stored representation intact, including values from older filters.
    // Interpret it through Boundary; tokens belong only to the serialization layer.
    QString from = Boundary().toString();
    QString to = from;

    QString toString() const;
    static bool fromString(const QString &value, QSOFilterDateRange &range);
    bool resolve(const QDate &today, QDateTime &start, QDateTime &end) const;
    bool isRelative() const;
};

#endif
