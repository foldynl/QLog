#ifndef QLOG_AWARDS_AWARDDEFINITION_H
#define QLOG_AWARDS_AWARDDEFINITION_H

#include <QString>
#include <QStringList>
#include <QWidget>
#include <QModelIndex>

/* Filter parameters collected from the AwardsDialog UI controls.
 * Passed to AwardDefinition::updateData() on every filter change. */
struct AwardFilterParams
{
    QString entitySelected;            /* DXCC entity ID from the "My DXCC Entity" combo */
    QStringList modes;                 /* SQL IN-list values, e.g. {"'NONE'", "'CW'", "'PHONE'"} */
    QStringList confirmedConditions;   /* SQL OR-conditions for confirmed status */
    bool notWorkedOnly;                /* Show only entities with no QSOs on any band */
    bool notConfirmedOnly;             /* Show only entities worked but not confirmed */
    QString userFilterWhereClause;     /* Extra WHERE clause from QSOFilterManager (includes leading "AND") */
};

/* Abstract base class for all award definitions.
 *
 * To add a completely custom award (non-table display), subclass this directly
 * and implement createWidget() / updateData().
 *
 * For the common band-column table display, subclass BandTableAward instead.
 *
 * Registration: add one line to AwardsDialog::createAwards(). */
class AwardDefinition
{
public:
    virtual ~AwardDefinition() = default;

    /* Unique internal key (e.g. "dxcc", "sota", "grid4"). Used as combo item data. */
    virtual QString key() const = 0;

    /*Translatable display name shown in the Award combo box. */
    virtual QString displayName() const = 0;

    /*Whether the "My DXCC Entity" combo is shown. Default: true. */
    virtual bool entityInputEnabled() const;

    /*Whether the Not-Worked / Not-Confirmed checkboxes are shown. Default: true. */
    virtual bool notWorkedEnabled() const;

    /*Create the display widget. Called once on first selection. Store in m_widget. */
    virtual QWidget* createWidget(QWidget *parent) = 0;

    /*Refresh data using the current filter parameters. Called on every filter change. */
    virtual void updateData(const AwardFilterParams &params) = 0;

    /* Return filter data for the logbook based on a double-clicked cell.
     * The dialog uses ConditionResult to emit AwardConditionSelected(country, band, filter)
     * which opens the logbook filtered to matching QSOs.
     * Default implementation returns an invalid (no-op) result. */
    struct ConditionResult
    {
        QString country;   /* Country name for the logbook filter (empty = not used) */
        QString band;      /* Band name from the column header (empty = all bands) */
        QString filter;    /* SQL WHERE clause fragment, e.g. "(sota_ref = 'OE/TI-001')" */
        bool valid = false;
    };
    virtual ConditionResult getConditionSelected(const QModelIndex &clickedIndex) const;

    /*Returns the cached widget (nullptr before createWidget()). */
    QWidget* widget() const { return m_widget; }

protected:
    QWidget *m_widget = nullptr;
};

#endif // QLOG_AWARDS_AWARDDEFINITION_H
