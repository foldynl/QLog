#include <QColor>
#include <QStringList>
#include "WsjtxTableModel.h"
#include "data/Data.h"

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
    const Row &row = wsjtxData.at(index.row());
    const WsjtxEntry &entry = row.entry;

    if (role == Qt::DisplayRole)
    {
        switch ( index.column() )
        {
        case COLUMN_CALLSIGN: return entry.callsign;
        case COLUMN_GRID: return entry.grid;
        case COLUMN_DISTANCE: if ( entry.distance > 0.0 ) return entry.distance; else return QVariant();
        case COLUMN_SNR: return QString::number(entry.decode.snr);
        case COLUMN_ACTIVITY: return QVariant();
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
    else if (index.column() == COLUMN_CALLSIGN && role == Qt::ToolTipRole)
    {
        return  QCoreApplication::translate("DBStrings", entry.dxcc.country.toUtf8().constData()) + " [" + Data::statusToText(entry.status) + "]";
    }
    else if (index.column() == COLUMN_FREQUENCY && role == Qt::ToolTipRole)
    {
        return entry.decode.id;
    }
    else if ( index.column() == COLUMN_ACTIVITY && role == Qt::ToolTipRole )
    {
        QStringList lines;
        const int period = periodMilliseconds(entry);
        const QDateTime sourcePeriod = referencePeriod(row);

        for ( const WsjtxActivity::Sample &sample : row.activity.samples() )
        {
            const qint64 elapsed = sample.decodedAt.msecsTo(sourcePeriod);
            const qint64 periodOffset = (elapsed + period / 2) / period;
            if ( elapsed < 0 || periodOffset >= WsjtxActivity::HISTORY_LENGTH )
            {
                continue;
            }

            lines.append(tr("%1 UTC: %2 dB")
                         .arg(sample.decodedAt.time().toString("HH:mm:ss.zzz"))
                         .arg(sample.snr));
        }

        return lines.join('\n');
    }
    else if ( index.column() == COLUMN_ACTIVITY && role == ACTIVITY_HISTORY_ROLE )
    {
        return row.activity.history(referencePeriod(row),
                                    periodMilliseconds(entry));
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
        case COLUMN_ACTIVITY:
            return row.activity.latestActivity();
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
    case COLUMN_ACTIVITY: return tr("Activity");
    case COLUMN_LAST_MESSAGE: return tr("Last Message");
    case COLUMN_MEMBER: return tr("Member");
    case COLUMN_FREQUENCY: return tr("Frequency");
    default: return QVariant();
    }
}

void WsjtxTableModel::addOrReplaceEntry(const WsjtxEntry &entry)
{
    const int idx = findRow(entry);

    if ( idx >= 0 )
    {
        Row &row = wsjtxData[idx];
        const QDateTime decodedAt = row.activity.update(entry.receivedTime,
                                                        entry.decode.time,
                                                        entry.decode.snr,
                                                        periodMilliseconds(entry));
        const bool sourceAdvanced = updateSourceActivity(entry, decodedAt);

        if ( ! entry.grid.isEmpty() )
        {
            row.entry.grid = entry.grid;
        }

        row.entry.status = entry.status;
        row.entry.dxccStatusSatellite = entry.dxccStatusSatellite;
        row.entry.decode = entry.decode;
        if ( !entry.replyDecode.id.isEmpty() )
            row.entry.replyDecode = entry.replyDecode;
        row.entry.receivedTime = entry.receivedTime;
        row.entry.dupeCount = entry.dupeCount;
        row.entry.freq = entry.freq;
        row.entry.band = entry.band;
        row.entry.decodedMode = entry.decodedMode;
        row.entry.bandPlanMode = entry.bandPlanMode;
        row.entry.modeGroupString = entry.modeGroupString;
        row.entry.comment = entry.comment;
        row.entry.spotPeriod = entry.spotPeriod;
        // does not update club info

        emit dataChanged(createIndex(idx, 0), createIndex(idx, COLUMN_FREQUENCY));
        if ( sourceAdvanced )
            emitSourceActivityChanged(entry.decode.id);
    }
    else
    {
        beginInsertRows(QModelIndex(), wsjtxData.count(), wsjtxData.count());
        Row row;
        row.entry = entry;
        const QDateTime decodedAt = row.activity.update(entry.receivedTime,
                                                        entry.decode.time,
                                                        entry.decode.snr,
                                                        periodMilliseconds(entry));
        const bool sourceAdvanced = updateSourceActivity(entry, decodedAt);
        wsjtxData.append(row);
        endInsertRows();
        if ( sourceAdvanced )
            emitSourceActivityChanged(entry.decode.id);
    }
}

void WsjtxTableModel::spotAging()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for ( int row = wsjtxData.size() - 1; row >= 0; )
    {
        if ( wsjtxData.at(row).entry.receivedTime.secsTo(now) <= 3 * 60 )
        {
            --row;
            continue;
        }

        const int lastRow = row;
        while ( row >= 0
                && wsjtxData.at(row).entry.receivedTime.secsTo(now) > 3 * 60 )
        {
            --row;
        }
        const int firstRow = row + 1;

        // keep the entry longer than the spotPeriod, because it is used for querying from the Map.
        beginRemoveRows(QModelIndex(), firstRow, lastRow);
        for ( int removeRow = lastRow; removeRow >= firstRow; --removeRow )
            wsjtxData.removeAt(removeRow);
        endRemoveRows();
    }
}

bool WsjtxTableModel::callsignExists(const WsjtxEntry &call)
{
    return findRow(call) >= 0;
}

const WsjtxEntry WsjtxTableModel::getEntry(QModelIndex idx) const
{
    return wsjtxData.at(idx.row()).entry;
}

const WsjtxEntry WsjtxTableModel::getEntry(const QString &id, const QString &callsign) const
{
    WsjtxEntry entry;
    entry.decode.id = id;
    entry.callsign = callsign;
    const int index = findRow(entry);

    return (index < 0) ? WsjtxEntry() : wsjtxData.at(index).entry;
}

void WsjtxTableModel::clear()
{
    beginResetModel();
    wsjtxData.clear();
    sourceActivity.clear();
    endResetModel();
}

void WsjtxTableModel::removeSource(const QString &id)
{
    for ( int row = wsjtxData.size() - 1; row >= 0; )
    {
        if ( wsjtxData.at(row).entry.decode.id != id )
        {
            --row;
            continue;
        }

        const int lastRow = row;
        while ( row >= 0 && wsjtxData.at(row).entry.decode.id == id )
            --row;
        const int firstRow = row + 1;

        beginRemoveRows(QModelIndex(), firstRow, lastRow);
        for ( int removeRow = lastRow; removeRow >= firstRow; --removeRow )
            wsjtxData.removeAt(removeRow);
        endRemoveRows();
    }

    sourceActivity.remove(id);
}

void WsjtxTableModel::removeSpot(const QString &callsign, const QString &band)
{
    for ( int row = wsjtxData.size() - 1; row >= 0; )
    {
        if ( wsjtxData.at(row).entry.callsign != callsign
             || wsjtxData.at(row).entry.band != band )
        {
            --row;
            continue;
        }

        const int lastRow = row;
        while ( row >= 0 && wsjtxData.at(row).entry.callsign == callsign
                && wsjtxData.at(row).entry.band == band )
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

    for ( Row &row : wsjtxData )
    {
        WsjtxEntry &entry = row.entry;
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
        WsjtxEntry &entry = wsjtxData[row].entry;

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
    QList<WsjtxEntry> entries;
    entries.reserve(wsjtxData.size());
    for ( const Row &row : wsjtxData )
        entries.append(row.entry);
    return entries;
}

int WsjtxTableModel::findRow(const WsjtxEntry &entry) const
{
    for ( int row = 0; row < wsjtxData.size(); ++row )
    {
        if ( wsjtxData.at(row).entry.decode.id == entry.decode.id
             && wsjtxData.at(row).entry.callsign == entry.callsign )
        {
            return row;
        }
    }

    return -1;
}

int WsjtxTableModel::periodMilliseconds(const WsjtxEntry &entry)
{
    return qMax(1, qRound(entry.spotPeriod * 1000.0f));
}

QDateTime WsjtxTableModel::referencePeriod(const Row &row) const
{
    const auto source = sourceActivity.constFind(row.entry.decode.id);
    if ( source != sourceActivity.constEnd()
         && source->periodMilliseconds == periodMilliseconds(row.entry) )
    {
        return source->referencePeriod;
    }

    return row.activity.samples().isEmpty()
           ? QDateTime()
           : row.activity.samples().constLast().decodedAt;
}

bool WsjtxTableModel::updateSourceActivity(const WsjtxEntry &entry,
                                           const QDateTime &decodedAt)
{
    if ( entry.decode.id.isEmpty() || !decodedAt.isValid() )
        return false;

    const int period = periodMilliseconds(entry);
    SourceActivity &source = sourceActivity[entry.decode.id];
    if ( source.periodMilliseconds != period )
    {
        source.periodMilliseconds = period;
        source.referencePeriod = decodedAt;
        return true;
    }

    if ( !source.referencePeriod.isValid()
         || source.referencePeriod < decodedAt )
    {
        source.referencePeriod = decodedAt;
        return true;
    }

    return false;
}

void WsjtxTableModel::emitSourceActivityChanged(const QString &id)
{
    for ( int row = 0; row < wsjtxData.size(); )
    {
        if ( wsjtxData.at(row).entry.decode.id != id )
        {
            ++row;
            continue;
        }

        const int firstRow = row;
        while ( row < wsjtxData.size()
                && wsjtxData.at(row).entry.decode.id == id )
        {
            ++row;
        }

        emit dataChanged(createIndex(firstRow, COLUMN_ACTIVITY),
                         createIndex(row - 1, COLUMN_ACTIVITY),
                         {ACTIVITY_HISTORY_ROLE, Qt::ToolTipRole});
    }
}
