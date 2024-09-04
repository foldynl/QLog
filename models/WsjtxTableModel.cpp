#include <QColor>
#include "WsjtxTableModel.h"
#include "data/Data.h"

bool operator==(const WsjtxEntry& a, const WsjtxEntry& b)
{
    return a.callsign == b.callsign;
}

int WsjtxTableModel::rowCount(const QModelIndex&) const
{
    return wsjtxData.count();
}

int WsjtxTableModel::columnCount(const QModelIndex&) const
{
    return 7;
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
        case COLUMN_LAST_MESSAGE: return entry.decode.message;
        case COLUMN_MEMBER: return entry.memberList2StringList().join(", ");
        default: return QVariant();
        }
    }
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::BackgroundRole)
    {
        return Data::statusToColor(entry.status, QColor(Qt::transparent));
    }
    else if (index.column() > COLUMN_CALLSIGN && role == Qt::BackgroundRole)
    {
        if ( entry.receivedTime.secsTo(QDateTime::currentDateTimeUtc()) >= spotPeriod * 0.8)
            /* -20% time of period because WSTX sends messages in waves and not exactly in time period */
        {
            return QColor(Qt::darkGray);
        }
    }
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::ForegroundRole)
    {
        //return Data::statusToInverseColor(entry.status, QColor(Qt::black));
    }
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::ToolTipRole)
    {
        return  QCoreApplication::translate("DBStrings", entry.dxcc.country.toUtf8().constData()) + " [" + Data::statusToText(entry.status) + "]";
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
    default: return QVariant();
    }
}

void WsjtxTableModel::addOrReplaceEntry(WsjtxEntry entry)
{
    int idx = wsjtxData.indexOf(entry);

    if ( idx >= 0 )
    {
        if ( ! entry.grid.isEmpty() )
        {
            wsjtxData[idx].grid = entry.grid;
        }

        wsjtxData[idx].status = entry.status;
        wsjtxData[idx].decode = entry.decode;
        wsjtxData[idx].receivedTime = entry.receivedTime;
        // does not update club info

        emit dataChanged(createIndex(idx,0), createIndex(idx,4));
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

const WsjtxEntry WsjtxTableModel::getEntry(const QString &callsign) const
{
    WsjtxEntry entry;
    entry.callsign = callsign;
    int index = wsjtxData.indexOf(entry);

    return (index < 0) ? WsjtxEntry() : wsjtxData.at(index);
}

void WsjtxTableModel::setCurrentSpotPeriod(float period)
{
    spotPeriod = period;
}

void WsjtxTableModel::clear()
{
    beginResetModel();
    wsjtxData.clear();
    endResetModel();
}

