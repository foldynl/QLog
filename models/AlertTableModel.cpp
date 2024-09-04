#include <QColor>
#include "AlertTableModel.h"
#include "data/Data.h"
#include "rig/macros.h"

//-+ FREQ_MATCH_TOLERANCE MHz is OK when QLog evaluates the same spot freq
#define FREQ_MATCH_TOLERANCE 0.005

int AlertTableModel::rowCount(const QModelIndex&) const
{
    return alertList.count();
}

int AlertTableModel::columnCount(const QModelIndex&) const
{
    return 7;
}

QVariant AlertTableModel::data(const QModelIndex& index, int role) const
{
    AlertTableRecord selectedRecord = alertList.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch ( index.column() )
        {
        case COLUMN_RULENAME: return selectedRecord.ruleName.join(",");
        case COLUMN_CALLSIGN: return selectedRecord.alert.callsign;
        case COLUMN_FREQ: return QSTRING_FREQ(selectedRecord.alert.freq);
        case COLUMN_MODE: return selectedRecord.alert.modeGroupString;
        case COLUMN_UPDATED: return selectedRecord.counter;
        case COLUMN_LAST_UPDATE: return selectedRecord.alert.dateTime.toString(locale.formatTimeLongWithoutTZ());
        case COLUMN_LAST_COMMENT: return selectedRecord.alert.comment;
        default: return QVariant();
        }
    }
    else if ( index.column() == COLUMN_CALLSIGN && role == Qt::BackgroundRole )
    {
        return Data::statusToColor(selectedRecord.alert.status, QColor(Qt::transparent));
    }
    else if ( role == Qt::UserRole )
    {
        switch ( index.column() )
        {
        case COLUMN_FREQ: return data(index, Qt::DisplayRole).toDouble(); break;
        case COLUMN_UPDATED: return data(index, Qt::DisplayRole).toULongLong(); break;
        case COLUMN_LAST_UPDATE: return selectedRecord.alert.dateTime; break;
        default: return data(index, Qt::DisplayRole);
        }
    }

    return QVariant();
}

QVariant AlertTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch (section)
    {
    case COLUMN_RULENAME: return tr("Rule Name");
    case COLUMN_CALLSIGN: return tr("Callsign");
    case COLUMN_FREQ: return tr("Frequency");
    case COLUMN_MODE: return tr("Mode");
    case COLUMN_UPDATED: return tr("Updated");
    case COLUMN_LAST_UPDATE: return tr("Last Update");
    case COLUMN_LAST_COMMENT: return tr("Last Comment");
    default: return QVariant();
    }
}

void AlertTableModel::addAlert(SpotAlert entry)
{
    AlertTableRecord newRecord(entry);

    QMutexLocker locker(&alertListMutex);

    int spotIndex = alertList.indexOf(newRecord);

    if ( spotIndex >= 0)
    {
        /* QLog already contains the spot, update it */
        alertList[spotIndex].counter++;
        alertList[spotIndex].ruleName << entry.ruleName;
        alertList[spotIndex].ruleName.removeDuplicates();
        alertList[spotIndex].ruleName.sort();

        // QLog have WSJTX record and Spot from DXC is processed.
        // only change a limited number of fields to preserve the WSJTX Decode for an WSJTX application tuning
        if ( entry.source == SpotAlert::DXSPOT
             && alertList[spotIndex].alert.source == SpotAlert::WSJTXCQSPOT )
        {
            alertList[spotIndex].alert.comment = entry.comment;
            alertList[spotIndex].alert.spotter = entry.spotter;
            alertList[spotIndex].alert.dxcc_spotter = entry.dxcc_spotter;
        }
        else
        {
            alertList[spotIndex].alert = entry;
        }
        emit dataChanged(createIndex(spotIndex,0), createIndex(spotIndex,5));
    }
    else
    {
        /* New spot, insert it */
        beginInsertRows(QModelIndex(), 0, 0);
        alertList.prepend(newRecord);
        endInsertRows();
    }
}

void AlertTableModel::clear()
{
    QMutexLocker locker(&alertListMutex);
    beginResetModel();
    alertList.clear();
    endResetModel();
}

const AlertTableModel::AlertTableRecord AlertTableModel::getTableRecord(const QModelIndex &index)
{
    QMutexLocker locker(&alertListMutex);
    return alertList.at(index.row());
}

void AlertTableModel::aging(const int clear_interval_sec)
{
    if ( clear_interval_sec <= 0 ) return;

    QMutexLocker locker(&alertListMutex);

    QMutableListIterator<AlertTableRecord> alertIterator(alertList);

    beginResetModel();
    while ( alertIterator.hasNext() )
    {
        alertIterator.next();
        if ( alertIterator.value().alert.dateTime.addSecs(clear_interval_sec) <= QDateTime::currentDateTimeUtc() )
        {
            alertIterator.remove();
        }
    }
    endResetModel();
}


bool AlertTableModel::AlertTableRecord::operator==(const AlertTableRecord &spot) const
{
   return ( (spot.alert.callsign == this->alert.callsign)
            && (spot.alert.modeGroupString == this->alert.modeGroupString)
            && (qAbs(this->alert.freq - spot.alert.freq) <= FREQ_MATCH_TOLERANCE)
            );
}

AlertTableModel::AlertTableRecord::AlertTableRecord(const SpotAlert &spotAlert) :

    ruleName(spotAlert.ruleName),
    counter(0),
    alert(spotAlert)
{
}
