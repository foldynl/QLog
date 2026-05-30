#include <QColor>
#include <QSqlRecord>

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
    return 8;
}

QVariant AlertTableModel::data(const QModelIndex& index, int role) const
{
    const AlertTableRecord &selectedRecord = alertList.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch ( index.column() )
        {
        case COLUMN_RULENAME: return selectedRecord.ruleName.join(",");
        case COLUMN_CALLSIGN: return selectedRecord.alert.spot.callsign;
        case COLUMN_FREQ: return QSTRING_FREQ(selectedRecord.alert.spot.freq);
        case COLUMN_MODE: return selectedRecord.alert.spot.modeGroupString;
        case COLUMN_UPDATED: return selectedRecord.counter;
        case COLUMN_LAST_UPDATE: return locale.toString(selectedRecord.alert.spot.dateTime,locale.formatTimeLongWithoutTZ());
        case COLUMN_LAST_COMMENT: return selectedRecord.alert.spot.comment;
        case COLUMN_MEMBER: return selectedRecord.alert.spot.memberList2StringList().join(",");
        default: return QVariant();
        }
    }
    else if ( index.column() == COLUMN_CALLSIGN && role == Qt::BackgroundRole )
    {
        return Data::statusToColor(selectedRecord.alert.spot.status, selectedRecord.alert.spot.dupeCount, QColor(Qt::transparent));
    }
    else if ( index.column() == COLUMN_CALLSIGN && role == Qt::ForegroundRole )
    {
        return Data::textColorForBackground(Data::statusToColor(selectedRecord.alert.spot.status,
                                                                selectedRecord.alert.spot.dupeCount,
                                                                QColor(Qt::transparent)));
    }
    else if ( role == Qt::UserRole )
    {
        switch ( index.column() )
        {
        case COLUMN_FREQ: return data(index, Qt::DisplayRole).toDouble(); break;
        case COLUMN_UPDATED: return data(index, Qt::DisplayRole).toULongLong(); break;
        case COLUMN_LAST_UPDATE: return selectedRecord.alert.spot.dateTime; break;
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
    case COLUMN_MEMBER: return tr("Member");
    default: return QVariant();
    }
}

void AlertTableModel::addAlert(const SpotAlert &entry)
{
    AlertTableRecord newRecord(entry);

    QMutexLocker locker(&alertListMutex);

    int spotIndex = alertList.indexOf(newRecord);

    if ( spotIndex >= 0)
    {
        /* QLog already contains the spot, update it */
        alertList[spotIndex].counter++;
        alertList[spotIndex].ruleName << entry.ruleNameList;
        alertList[spotIndex].ruleName.removeDuplicates();
        alertList[spotIndex].ruleName.sort();

        // QLog have WSJTX record and Spot from DXC is processed.
        // only change a limited number of fields to preserve the WSJTX Decode for an WSJTX application tuning
        if ( entry.source == SpotAlert::DXSPOT
             && alertList[spotIndex].alert.source == SpotAlert::WSJTXCQSPOT )
        {
            alertList[spotIndex].alert.spot.comment = entry.spot.comment;
            alertList[spotIndex].alert.spot.spotter = entry.spot.spotter;
            alertList[spotIndex].alert.spot.dxcc_spotter = entry.spot.dxcc_spotter;
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
        if ( alertIterator.value().alert.spot.dateTime.addSecs(clear_interval_sec) <= QDateTime::currentDateTimeUtc() )
        {
            alertIterator.remove();
        }
    }
    endResetModel();
}

void AlertTableModel::resetDupe()
{
    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alert : alertList )
        alert.alert.spot.dupeCount = 0;
    endResetModel();
}

void AlertTableModel::recalculateDupe()
{
    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alertRecord : alertList )
    {
        SpotAlert &alert = alertRecord.alert;
        alert.spot.dupeCount = Data::countDupe(alert.spot.callsign,
                                               alert.spot.band,
                                               alert.spot.modeGroupString);
    }
    endResetModel();
}

void AlertTableModel::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    qint32 dxcc = record.value("dxcc").toInt();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());
    const QString &callsign = record.value("callsign").toString();

    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alertRecord : alertList )
    {
        SpotAlert &alert = alertRecord.alert;

        alert.spot.status = Data::dxccNewStatusWhenQSOAdded(alert.spot.status,
                                                            alert.spot.dxcc.dxcc,
                                                            alert.spot.band,
                                                            ( ( alert.spot.modeGroupString == BandPlan::MODE_GROUP_STRING_FTx ) ? BandPlan::MODE_GROUP_STRING_DIGITAL
                                                                                                                                : dxccModeGroup ),
                                                            dxcc,
                                                            band,
                                                            dxccModeGroup);
        if ( alert.spot.callsign == callsign )
            alert.spot.dupeCount = Data::dupeNewCountWhenQSOAdded(alert.spot.dupeCount,
                                                                  alert.spot.band,
                                                                  alert.spot.modeGroupString,
                                                                  band,
                                                                  dxccModeGroup);
    }
    endResetModel();
}

void AlertTableModel::updateSpotsStatusWhenQSOUpdated(const QSqlRecord &)
{
    QMutexLocker locker(&alertListMutex);

    // at this point, we don't know if callsign has been changed or other field.
    // TODO: DXCC status
    // TODO: Dupe status update

    // beginResetModel();
    // for ( AlertTableRecord &alert : alertList )
    // {
    //     SpotAlert &spot = alert.alert;
    //     spot.dupeCount = Data::countDupe(spot.callsign, spot.band, spot.modeGroupString);
    // }
    // endResetModel();
}

void AlertTableModel::updateSpotsStatusWhenQSODeleted(const QSqlRecord &record)
{
    // Pay attention: this method is called before the QSO is added to contacts
    const QString &callsign = record.value("callsign").toString();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());

    QMutexLocker locker(&alertListMutex);

    for ( AlertTableRecord &alertRecord : alertList )
    {
        SpotAlert &alert = alertRecord.alert;

        if ( alert.spot.dupeCount && alert.spot.callsign == callsign )
            alert.spot.dupeCount = Data::dupeNewCountWhenQSODelected(alert.spot.dupeCount,
                                                                     alert.spot.band,
                                                                     alert.spot.modeGroupString,
                                                                     band,
                                                                     dxccModeGroup);
    }
}

void AlertTableModel::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    if ( entities.isEmpty() )
        return;

    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alertRecord : alertList  )
    {
        SpotAlert &alert = alertRecord.alert;

        if ( !entities.contains(alert.spot.dxcc.dxcc) )
            continue;

        alert.spot.status = Data::instance()->dxccStatus(alert.spot.dxcc.dxcc, alert.spot.band, alert.spot.modeGroupString);
    }
    endResetModel();
}

void AlertTableModel::recalculateDxccStatus()
{
    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alertRecord : alertList  )
    {
        SpotAlert &alert = alertRecord.alert;

        alert.spot.status = Data::instance()->dxccStatus(alert.spot.dxcc.dxcc, alert.spot.band, alert.spot.modeGroupString);
    }
    endResetModel();
}

bool AlertTableModel::AlertTableRecord::operator==(const AlertTableRecord &spot) const
{
   return ( (spot.alert.spot.callsign == this->alert.spot.callsign)
            && (spot.alert.spot.modeGroupString == this->alert.spot.modeGroupString)
            && (qAbs(this->alert.spot.freq - spot.alert.spot.freq) <= FREQ_MATCH_TOLERANCE)
            );
}

AlertTableModel::AlertTableRecord::AlertTableRecord(const SpotAlert &spotAlert) :

    ruleName(spotAlert.ruleNameList),
    counter(0),
    alert(spotAlert)
{
}
