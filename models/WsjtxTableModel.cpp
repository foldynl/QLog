#include <QColor>
#include "WsjtxTableModel.h"
#include "data/Data.h"

bool operator==(const WsjtxEntry& a, const WsjtxEntry& b)
{
    return a.decode.id == b.decode.id
           && a.callsign == b.callsign;
}

int WsjtxTableModel::rowCount(const QModelIndex&) const
{
    return wsjtxData.count();
}

int WsjtxTableModel::columnCount(const QModelIndex&) const
{
    return COLUMN_FREQUENCY + 1;
}

QVariant WsjtxTableModel::data(const QModelIndex& index, int role) const
{
    WsjtxEntry entry = wsjtxData.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch ( index.column() )
        {
        case COLUMN_CALLSIGN: return entry.callsign;
        case COLUMN_GRID: return entry.grid;
        case COLUMN_DISTANCE: if ( entry.distance > 0.0 ) return entry.distance; else return QVariant();
        case COLUMN_SNR: return QString::number(entry.decode.snr);
        case COLUMN_LAST_ACTIVITY: return entry.decode.time.toString();
        case COLUMN_LAST_MESSAGE: return entry.comment;
        case COLUMN_MEMBER: return entry.memberList2StringList().join(", ");
        case COLUMN_FREQUENCY: return entry.freq;
        default: return QVariant();
        }
    }
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::BackgroundRole)
    {
        return Data::statusToColor(entry.status, entry.dupeCount, QColor(Qt::transparent));
    }
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::ForegroundRole)
    {
        return Data::textColorForBackground(Data::statusToColor(entry.status,
                                                                entry.dupeCount,
                                                                QColor(Qt::transparent)));
    }
    else if (index.column() > COLUMN_CALLSIGN && role == Qt::BackgroundRole)
    {
        if ( entry.receivedTime.secsTo(QDateTime::currentDateTimeUtc()) >= entry.spotPeriod * 0.8)
            /* -20% time of period because WSTX sends messages in waves and not exactly in time period */
        {
            return QColor(Qt::darkGray);
        }
    }
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::ToolTipRole)
    {
        return  QCoreApplication::translate("DBStrings", entry.dxcc.country.toUtf8().constData()) + " [" + Data::statusToText(entry.status) + "]";
    }
    else if (index.column() == COLUMN_FREQUENCY && role == Qt::ToolTipRole)
    {
        return entry.decode.id;
    }
    else if ( role == Qt::UserRole )
    {
        switch ( index.column() )
        {
        case COLUMN_DISTANCE:
            return data(index, Qt::DisplayRole).toDouble();
            break;
        case COLUMN_SNR:
            return data(index, Qt::DisplayRole).toInt();
            break;
        case COLUMN_LAST_ACTIVITY:
            return data(index, Qt::DisplayRole).toTime();
            break;
        case COLUMN_FREQUENCY:
            return entry.freq;
            break;
        default:
            return data(index, Qt::DisplayRole);
        }
    }

    return QVariant();
}

QVariant WsjtxTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch (section)
    {
    case COLUMN_CALLSIGN: return tr("Callsign");
    case COLUMN_GRID: return tr("Gridsquare");
    case COLUMN_DISTANCE: return tr("Distance");
    case COLUMN_SNR: return tr("SNR");
    case COLUMN_LAST_ACTIVITY: return tr("Last Activity");
    case COLUMN_LAST_MESSAGE: return tr("Last Message");
    case COLUMN_MEMBER: return tr("Member");
    case COLUMN_FREQUENCY: return tr("Frequency");
    default: return QVariant();
    }
}

void WsjtxTableModel::addOrReplaceEntry(const WsjtxEntry &entry)
{
    int idx = wsjtxData.indexOf(entry);

    if ( idx >= 0 )
    {
        if ( ! entry.grid.isEmpty() )
        {
            wsjtxData[idx].grid = entry.grid;
        }

        wsjtxData[idx].status = entry.status;
        wsjtxData[idx].dxccStatusSatellite = entry.dxccStatusSatellite;
        wsjtxData[idx].decode = entry.decode;
        if ( !entry.replyDecode.id.isEmpty() )
            wsjtxData[idx].replyDecode = entry.replyDecode;
        wsjtxData[idx].receivedTime = entry.receivedTime;
        wsjtxData[idx].dupeCount = entry.dupeCount;
        wsjtxData[idx].freq = entry.freq;
        wsjtxData[idx].band = entry.band;
        wsjtxData[idx].decodedMode = entry.decodedMode;
        wsjtxData[idx].bandPlanMode = entry.bandPlanMode;
        wsjtxData[idx].modeGroupString = entry.modeGroupString;
        wsjtxData[idx].comment = entry.comment;
        wsjtxData[idx].spotPeriod = entry.spotPeriod;
        // does not update club info

        emit dataChanged(createIndex(idx, 0), createIndex(idx, COLUMN_FREQUENCY));
    }
    else
    {
        beginInsertRows(QModelIndex(), wsjtxData.count(), wsjtxData.count());
        wsjtxData.append(entry);
        endInsertRows();
    }
}

void WsjtxTableModel::spotAging()
{
    beginResetModel();

    QMutableListIterator<WsjtxEntry> entry(wsjtxData);

    while ( entry.hasNext() )
    {
        const WsjtxEntry &current = entry.next();

        // keep the entry longer than the spotPeriod, because it is used for querying from the Map.
        if ( current.receivedTime.secsTo(QDateTime::currentDateTimeUtc()) > 3 * 60 )
            entry.remove();
    }

    endResetModel();
}

bool WsjtxTableModel::callsignExists(const WsjtxEntry &call)
{
    return wsjtxData.contains(call);
}

const WsjtxEntry WsjtxTableModel::getEntry(QModelIndex idx) const
{
    return wsjtxData.at(idx.row());
}

const WsjtxEntry WsjtxTableModel::getEntry(const QString &id, const QString &callsign) const
{
    WsjtxEntry entry;
    entry.decode.id = id;
    entry.callsign = callsign;
    int index = wsjtxData.indexOf(entry);

    return (index < 0) ? WsjtxEntry() : wsjtxData.at(index);
}

void WsjtxTableModel::clear()
{
    beginResetModel();
    wsjtxData.clear();
    endResetModel();
}

void WsjtxTableModel::removeSource(const QString &id)
{
    for ( int row = wsjtxData.size() - 1; row >= 0; )
    {
        if ( wsjtxData.at(row).decode.id != id )
        {
            --row;
            continue;
        }

        const int lastRow = row;
        while ( row >= 0 && wsjtxData.at(row).decode.id == id )
            --row;
        const int firstRow = row + 1;

        beginRemoveRows(QModelIndex(), firstRow, lastRow);
        for ( int removeRow = lastRow; removeRow >= firstRow; --removeRow )
            wsjtxData.removeAt(removeRow);
        endRemoveRows();
    }
}

void WsjtxTableModel::removeSpot(const QString &callsign)
{
    for ( int row = wsjtxData.size() - 1; row >= 0; )
    {
        if ( wsjtxData.at(row).callsign != callsign )
        {
            --row;
            continue;
        }

        const int lastRow = row;
        while ( row >= 0 && wsjtxData.at(row).callsign == callsign )
            --row;
        const int firstRow = row + 1;

        beginRemoveRows(QModelIndex(), firstRow, lastRow);
        for ( int removeRow = lastRow; removeRow >= firstRow; --removeRow )
            wsjtxData.removeAt(removeRow);
        endRemoveRows();
    }
}

void WsjtxTableModel::refreshStatusColors()
{
    if ( wsjtxData.isEmpty() )
        return;

    emit dataChanged(createIndex(0, COLUMN_CALLSIGN),
                     createIndex(wsjtxData.size() - 1, COLUMN_CALLSIGN),
                     {Qt::BackgroundRole, Qt::ForegroundRole});
}

void WsjtxTableModel::recalculateDxccStatus()
{
    if ( wsjtxData.isEmpty() )
        return;

    const bool satellite = Data::instance()->isSatelliteDxccContext();

    for ( WsjtxEntry &entry : wsjtxData )
    {
        entry.status = Data::instance()->currentDxccStatus(entry.dxcc.dxcc,
                                                           entry.band,
                                                           entry.modeGroupString);
        entry.dxccStatusSatellite = satellite;
    }

    emit dataChanged(createIndex(0, COLUMN_CALLSIGN),
                     createIndex(wsjtxData.size() - 1, COLUMN_CALLSIGN),
                     {Qt::BackgroundRole, Qt::ForegroundRole, Qt::ToolTipRole});
}

bool WsjtxTableModel::updateSpotsDxccStatus(const QSet<uint> &entities)
{
    if ( entities.isEmpty() )
        return false;

    const bool satellite = Data::instance()->isSatelliteDxccContext();
    int firstChangedRow = -1;
    int lastChangedRow = -1;

    for ( int row = 0; row < wsjtxData.size(); ++row )
    {
        WsjtxEntry &entry = wsjtxData[row];

        if ( !entities.contains(entry.dxcc.dxcc) )
            continue;

        entry.status = Data::instance()->currentDxccStatus(entry.dxcc.dxcc,
                                                           entry.band,
                                                           entry.modeGroupString);
        entry.dxccStatusSatellite = satellite;

        if ( firstChangedRow < 0 )
            firstChangedRow = row;
        lastChangedRow = row;
    }

    if ( firstChangedRow < 0 )
        return false;

    emit dataChanged(createIndex(firstChangedRow, COLUMN_CALLSIGN),
                     createIndex(lastChangedRow, COLUMN_CALLSIGN),
                     {Qt::BackgroundRole, Qt::ForegroundRole, Qt::ToolTipRole});
    return true;
}

QList<WsjtxEntry> WsjtxTableModel::entries() const
{
    return wsjtxData;
}
