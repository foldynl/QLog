#ifndef QLOG_AWARDS_BANDTABLEAWARD_H
#define QLOG_AWARDS_BANDTABLEAWARD_H

#include <QTableView>
#include <QPair>
#include "AwardDefinition.h"
#include "models/AwardsTableModel.h"

/* Standard band-column table award — the default display for most awards.
 *
 * This class builds and executes a SQL query that produces a table where:
 *   - Rows  = award entities (countries, zones, parks, summits, ...)
 *   - Cols  = amateur bands + SAT + EME
 *   - Cells = worked / confirmed / not-worked status (color-coded)
 *
 * Summary rows (TOTAL Worked, TOTAL Confirmed, per-band Worked/Confirmed counts)
 * are prepended automatically.
 *
 * === How to add a new band-table award ===
 *
 * 1. Create a subclass of BandTableAward.
 * 2. Implement key() and displayName() (identity).
 * 3. Override entityInputEnabled() / notWorkedEnabled() if needed.
 * 4. Implement the three REQUIRED SQL fragment methods:
 *      - headersColumns()   — what goes into SELECT as col1, col2
 *      - sqlDetailTable()   — FROM + JOINs
 *      - additionalWhere()  — extra WHERE conditions
 * 5. Override clickFilter() so double-click navigates to the right QSOs.
 * 6. Register in AwardsDialog::createAwards().
 *
 * That's it for simple awards. For awards that need a CTE (e.g. a number range
 * or a comma-separated field split), override additionalCTEs() and optionally
 * sourceContactsOverride(). Use the static helper methods to build CTEs without
 * writing raw SQL.
 *
 * === The generated SQL structure ===
 *
 *   WITH
 *     <additionalCTEs>,                        -- optional, from additionalCTEs()
 *     source_contacts AS (SELECT ... FROM contacts WHERE ...),  -- or sourceContactsOverride()
 *     detail_table AS (
 *       SELECT <headersColumns> col1, col2,
 *              MAX(CASE WHEN band='160m' ... END) as '160m',
 *              ...                              -- one column per band + SAT + EME
 *       <sqlDetailTable>                        -- FROM ... JOIN ...
 *       <additionalWhere>                       -- AND ...
 *       GROUP BY 1,2
 *     ),
 *     unique_worked AS (...),
 *     unique_confirmed AS (...)
 *   SELECT ...                                  -- summary rows + detail rows
 *   ORDER BY 1,2 COLLATE LOCALEAWARE ASC
 *
 * === Double-click behavior ===
 *
 * When a user double-clicks a detail row (row > 3), getConditionSelected()
 * builds a ConditionResult by calling clickFilter(col1, col2). The dialog
 * then emits AwardConditionSelected() to filter the logbook.
 *
 * Override clickUsesCountryName() to return true if col1 is a country name
 * that should be passed as the "country" parameter instead of a filter clause
 * (used by DXCC). */
class BandTableAward : public AwardDefinition
{
public:
    QWidget* createWidget(QWidget *parent) override;
    void updateData(const AwardFilterParams &params) override;
    ConditionResult getConditionSelected(const QModelIndex &clickedIndex) const override;

protected:
    // ===================================================================
    //  REQUIRED — every subclass must implement these three methods
    // ===================================================================

    /* SQL fragment for the SELECT clause that defines the two label columns.
     *
     * Must return exactly two expressions aliased as "col1" and "col2".
     * col1 is the primary identifier (displayed in the first column).
     * col2 is a secondary label (displayed in the second column), or NULL.
     *
     * The expressions can reference:
     *   - "d" — the directory/reference table (if using a CTE or lookup table)
     *   - "c" — the source_contacts table
     *   - "s", "p", "w" — any alias used in sqlDetailTable()
     *
     * entity - the selected DXCC entity ID (may be unused if entityInputEnabled() == false)
     *
     * Examples:
     *   "d.n col1, null col2"                              — WAZ (zone number, no secondary)
     *   "translate_to_locale(d.name) col1, d.prefix col2"  — DXCC (country name + prefix)
     *   "p.reference col1, p.name col2"                    — POTA (park reference + park name) */
    virtual QString headersColumns(const QString &entity) const = 0;

    /* SQL fragment starting with FROM that defines the source tables and joins.
     *
     * This is inserted into the detail_table CTE after the SELECT clause.
     * Must join to "source_contacts c" and "modes m" (m.dxcc is used for mode filtering).
     *
     * IMPORTANT: Must NOT contain a WHERE clause at the outer level.
     * The SQL template adds "WHERE 1=1" automatically after this fragment,
     * followed by additionalWhere(). Place entity filtering in the JOIN ON
     * conditions, and any other WHERE-level filters in additionalWhere().
     *
     * The join to source_contacts can be:
     *   - LEFT OUTER JOIN -- when all possible entities should appear (even unworked ones).
     *     Use this for awards with a fixed set of targets (DXCC, WAZ, WAC, WAS, ITU).
     *   - INNER JOIN -- when only worked entities should appear.
     *     Use this for open-ended awards (WPX, IOTA, POTA, SOTA, WWFF, Gridsquare).
     *
     * entity - the selected DXCC entity ID
     *
     * Examples:
     *   " FROM cqzCTE d"
     *   "   LEFT OUTER JOIN source_contacts c ON d.n = c.cqz AND c.my_dxcc = '<entity>'"
     *   "   LEFT OUTER JOIN modes m ON c.mode = m.name"
     *
     *   " FROM sota_summits s"
     *   "     INNER JOIN source_contacts c ON c.sota_ref = s.summit_code"
     *   "     INNER JOIN modes m ON c.mode = m.name" */
    virtual QString sqlDetailTable(const QString &entity) const = 0;

    /* Extra WHERE conditions appended to the detail_table query.
     *
     * The SQL template always emits "WHERE 1=1" before this fragment,
     * so sqlDetailTable() must NOT contain its own WHERE clause.
     *
     * Must start with " AND " if non-empty. Return empty QString() if not needed.
     *
     * Use this for:
     *   - filtering the directory/reference table (LEFT OUTER JOIN awards)
     *   - requiring non-null fields (INNER JOIN awards)
     * Do NOT duplicate entity filters that are already in sqlDetailTable's JOIN ON.
     *
     * entity - the selected DXCC entity ID
     *
     * Examples:
     *   " AND d.dxcc IN (6, 110, 291)"   -- filter directory table
     *   " AND c.iota is not NULL"         -- require non-null field
     *   QString()                         -- no extra filter (SOTA, WWFF) */
    virtual QString additionalWhere(const QString &entity) const = 0;

    // ===================================================================
    //  OPTIONAL — override only when the default behavior is not enough
    // ===================================================================

    /* Return additional CTEs to prepend before source_contacts.
     *
     * Override this when the award needs a reference table that doesn't exist
     * in the database (e.g. a numbered range for zones, a values list for continents,
     * or a split CTE for comma-separated fields).
     *
     * Each string in the list is a single CTE definition (without leading comma).
     * Default: empty list.
     *
     * Use the static helper methods to build CTEs:
     *   - generateNumberRangeCTE()  — for ITU (1-90), WAZ (1-40)
     *   - generateValuesCTE()       — for WAC continents
     *   - generateSplitCTE()        — for POTA (comma-separated references)
     *
     * entity        - the selected DXCC entity ID
     * contactFilter - the user filter WHERE clause (needed by split CTEs) */
    virtual QStringList additionalCTEs(const QString &entity,
                                       const QString &contactFilter) const;

    /* Override the default source_contacts CTE.
     *
     * By default, source_contacts is: SELECT * FROM contacts WHERE 1=1 <userFilter>.
     * Override this when the award needs a transformed contact source (e.g. POTA splits
     * comma-separated references into individual rows).
     *
     * Return empty QString() to use the default. Otherwise return a complete CTE definition
     * for "source_contacts AS (...)".
     *
     * contactFilter - the user filter WHERE clause
     *
     * See generateSplitSourceContacts() — helper for the split pattern. */
    virtual QString sourceContactsOverride(const QString &contactFilter) const;

    /* Return a SQL WHERE fragment for logbook filtering on double-click.
     *
     * Called when the user double-clicks a detail row. The returned string is
     * combined with entity filter and band into a complete WHERE clause.
     *
     * col1Value - value from column 1 (the primary identifier)
     * col2Value - value from column 2 (the secondary label)
     *
     * Examples:
     *   "cqz = '15'"                     — WAZ: filter by CQ zone
     *   "sota_ref = 'OE/TI-001'"         — SOTA: filter by summit
     *   "pota_ref LIKE '%K-0001%'"        — POTA: LIKE match for multi-ref field
     *   "state = 'CA' and dxcc in (...)"  — WAS: compound filter */
    virtual QString clickFilter(const QString &col1Value,
                                const QString &col2Value) const;

    /* Return true if col1 is a country name to be passed as the "country"
     * parameter of AwardConditionSelected (instead of a filter clause).
     * Only DXCC uses this. Default: false. */
    virtual bool clickUsesCountryName() const;

    // ===================================================================
    //  STATIC HELPERS — use these in additionalCTEs() / sourceContactsOverride()
    // ===================================================================

    /* Generate a recursive CTE that produces integers from min to max.
     * Output columns: n (the integer), value (same as n).
     * Example: generateNumberRangeCTE("cqzCTE", 1, 40) produces zones 1-40. */
    static QString generateNumberRangeCTE(const QString &name, int min, int max);

    /* Generate a VALUES CTE from a list of (key, label) pairs.
     * Output columns: column1 (key), column2 (label).
     * Example: generateValuesCTE("continents", {{"NA","North America"}, {"EU","Europe"}, ...}) */
    static QString generateValuesCTE(const QString &name,
                                     const QList<QPair<QString,QString>> &values);

    /* Generate a recursive split CTE that expands comma-separated values
     * from sourceColumn into individual rows with column outputColumn.
     *
     * Used for POTA where pota_ref or my_pota_ref may contain "K-0001,K-0002".
     * The CTE is named "split" and should be paired with generateSplitSourceContacts().
     *
     * sourceColumn  - the contacts table column to split (e.g. "pota_ref")
     * outputColumn  - the name for the split output column (e.g. "pota")
     * contactFilter - the user filter WHERE clause */
    static QString generateSplitCTE(const QString &sourceColumn,
                                    const QString &outputColumn,
                                    const QString &contactFilter);

    /* Generate a source_contacts CTE that reads from the "split" CTE.
     * Companion to generateSplitCTE(). Returns a CTE definition for source_contacts
     * that selects non-empty rows from the split result.
     * outputColumn - must match the outputColumn used in generateSplitCTE(). */
    static QString generateSplitSourceContacts(const QString &outputColumn);

private:
    QTableView *m_tableView = nullptr;
    AwardsTableModel *m_model = nullptr;
    QString m_currentEntity;
};

#endif // QLOG_AWARDS_BANDTABLEAWARD_H
