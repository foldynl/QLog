#include <QDebug>
#include <QApplication>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>

#include "WsjtxWidget.h"
#include "ui_WsjtxWidget.h"
#include "data/Data.h"
#include "core/debug.h"
#include "rig/Rig.h"
#include "rig/macros.h"
#include "data/StationProfile.h"
#include "ui/ColumnSettingDialog.h"
#include "ui/WsjtxFilterDialog.h"
#include "data/Gridsquare.h"
#include "ui/component/StyleItemDelegate.h"
#include "data/BandPlan.h"
#include "core/LogParam.h"
#include "core/PotaQE.h"
#include "core/WsjtxUDPReceiver.h"

MODULE_IDENTIFICATION("qlog.ui.wsjtxswidget");

class WsjtxActivityDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem cellOption(option);
        initStyleOption(&cellOption, index);
        cellOption.text.clear();

        QStyle *style = cellOption.widget ? cellOption.widget->style()
                                          : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,
                           &cellOption,
                           painter,
                           cellOption.widget);

        const QVariantList history = index.data(WsjtxTableModel::ACTIVITY_HISTORY_ROLE).toList();
        if ( history.isEmpty() )
            return;

        const QRect graphRect = option.rect.adjusted(4, 3, -4, -3);
        const int gap = 2;
        const int barWidth = qMax(2,
                                  (graphRect.width() - gap * (history.size() - 1))
                                  / history.size());
        const int graphWidth = barWidth * history.size() + gap * (history.size() - 1);
        const int graphLeft = graphRect.left() + (graphRect.width() - graphWidth) / 2;
        const bool selected = option.state & QStyle::State_Selected;
        const QColor activeColor = cellOption.palette.color(selected ? QPalette::HighlightedText
                                                                     : QPalette::Highlight);
        QColor inactiveColor = cellOption.palette.color(selected ? QPalette::HighlightedText
                                                                 : QPalette::Mid);
        inactiveColor.setAlpha(selected ? 90 : 55);

        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);

        for ( int i = 0; i < history.size(); ++i )
        {
            const int left = graphLeft + i * (barWidth + gap);
            if ( !history.at(i).isValid() )
            {
                painter->setBrush(inactiveColor);
                painter->drawRect(QRect(left, graphRect.bottom(), barWidth, 1));
                continue;
            }

            painter->setBrush(activeColor);
            const int level = WsjtxActivity::signalLevel(history.at(i).toInt());
            const int height = 4 + (level - 1) * 6;
            painter->drawRoundedRect(QRect(left,
                                           graphRect.bottom() - height + 1,
                                           barWidth,
                                           height),
                                     1.0,
                                     1.0);
        }

        painter->restore();
    }
};

WsjtxWidget::WsjtxWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WsjtxWidget),
    cqRE("(^(?:(?P<word1>(?:CQ|DE|QRZ)(?:\\s?DX|\\s(?:[A-Z]{1,4}|\\d{3}))|[A-Z0-9\\/]+|\\.{3})\\s)(?:(?P<word2>[A-Z0-9\\/]+)(?:\\s(?P<word3>[-+A-Z0-9]+)(?:\\s(?P<word4>(?:OOO|(?!RR73)[A-R]{2}[0-9]{2})))?)?)?)")
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    wsjtxTableModel = new WsjtxTableModel(this);

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(wsjtxTableModel);
    proxyModel->setSortRole(Qt::UserRole);

    ui->tableView->setModel(proxyModel);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->sortByColumn(WsjtxTableModel::COLUMN_ACTIVITY, Qt::DescendingOrder);
    ui->tableView->setItemDelegateForColumn(WsjtxTableModel::COLUMN_ACTIVITY,
                                            new WsjtxActivityDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(WsjtxTableModel::COLUMN_DISTANCE,
                                            new DistanceFormatDelegate(1, 0.1, ui->tableView));
    ui->tableView->setItemDelegateForColumn(WsjtxTableModel::COLUMN_FREQUENCY,
                                            new UnitFormatDelegate("", 6, 0.001, ui->tableView));
    ui->tableView->horizontalHeader()->setSectionsMovable(true);
    ui->tableView->addAction(ui->actionFilter);
    ui->tableView->addAction(ui->actionDisplayedColumns);

    ui->tableView->setColumnWidth(WsjtxTableModel::COLUMN_ACTIVITY, 64);
    restoreTableHeaderState();

    reloadSetting();
}

void WsjtxWidget::decodeReceived(WsjtxDecode decode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<decode.message;

    const auto clientIt = clients.constFind(decode.id);
    if ( clientIt == clients.constEnd() || !clientIt->initialized )
    {
        qCDebug(runtime) << "Ignoring WSJT-X decode without status" << decode.id;
        return;
    }

    const ClientState &client = clientIt.value();
    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    if ( decode.message.startsWith("CQ") )
    {
        QRegularExpressionMatch match = cqRE.match((decode.message));

        if (  match.hasMatch() )
        {
            WsjtxEntry entry;

            entry.dateTime = QDateTime::currentDateTime().toTimeZone(QTimeZone::utc());
            entry.decode = decode;
            entry.replyDecode = decode;
            entry.callsign = match.captured(3);
            entry.grid = match.captured(4);
            entry.dxcc = Data::instance()->lookupDxcc(entry.callsign);
            entry.status = Data::instance()->currentDxccStatus(entry.dxcc.dxcc,
                                                               client.band,
                                                               BandPlan::MODE_GROUP_STRING_DIGITAL);
            entry.dxccStatusSatellite = Data::instance()->isSatelliteDxccContext();
            entry.receivedTime = QDateTime::currentDateTimeUtc();
            entry.freq = client.frequency;
            entry.band = client.band;
            entry.decodedMode = client.status.mode;
            entry.bandPlanMode = ( client.status.mode == "FT8" ) ? BandPlan::BAND_MODE_FT8
                   : ( client.status.mode == "FT4" ) ? BandPlan::BAND_MODE_FT4
                   : ( client.status.mode == "FT2" ) ? BandPlan::BAND_MODE_FT2
                   : BandPlan::BAND_MODE_DIGITAL;
            entry.modeGroupString = BandPlan::bandMode2BandModeGroupString(entry.bandPlanMode);
            entry.spotPeriod = client.spotPeriod;
            entry.spotter = profile.callsign.toUpper();
            entry.comment = decode.message;
            entry.dxcc_spotter = Data::instance()->lookupDxcc(entry.spotter);
            // fix dxcc_spotter based on station prifile setting - cont is not calculated
            entry.dxcc_spotter.country = profile.country;
            entry.dxcc_spotter.cqz = profile.cqz;
            entry.dxcc_spotter.ituz = profile.ituz;
            entry.dxcc_spotter.dxcc = profile.dxcc;
            entry.distance = 0.0;
            entry.potaRef = PotaQE::instance()->findReferenceId(Callsign(entry.callsign), entry.freq).reference;
            if ( !entry.potaRef.isEmpty() )
            {
                entry.containsPOTA = true;
                entry.comment.append(" [+] POTA " + entry.potaRef);
            }
            entry.callsign_member = MembershipQE::instance()->query(entry.callsign);
            entry.dupeCount = Data::countDupe(entry.callsign, entry.band, BandPlan::MODE_GROUP_STRING_DIGITAL);
            if ( !profile.locator.isEmpty() )
            {
                Gridsquare myGrid(profile.locator);
                double distance;

                if ( myGrid.distanceTo(Gridsquare(entry.grid), distance) )
                {
                    entry.distance = round(distance);
                }
            }

            emit CQSpot(entry);

            QString unit;
            qCDebug(runtime)
                    << "Continent" << entry.dxcc.cont.contains(contregexp)
                    << "Continent RegExp" << contregexp
                    << "DX Status" << (entry.status & dxccStatusFilter)
                    << "Distance" << (entry.distance >= distanceFilter)
                    << "Curr Distance" << entry.distance << distanceFilter
                    << "SNR"<< (entry.decode.snr >= snrFilter )
                    << "Current SNR" << snrFilter
                    << "Member" << ( dxMemberFilter.size() == 0
                         || (dxMemberFilter.size()
                             && entry.memberList2Set().intersects(dxMemberFilter)));

            if ( entry.dxcc.cont.contains(contregexp)
                 && ( entry.status & dxccStatusFilter )
                 && Gridsquare::distance2localeUnitDistance(entry.distance, unit, locale) >= distanceFilter
                 && entry.decode.snr >= snrFilter
                 && ( dxMemberFilter.size() == 0
                      || (dxMemberFilter.size() && entry.memberList2Set().intersects(dxMemberFilter)))
                  && entry.dupeCount == 0
               )
            {
                wsjtxTableModel->addOrReplaceEntry(entry);
                emit filteredCQSpot(entry);
            }
        }
    }
    else
    {
        const QStringList &decodedElements = decode.message.split(" ");

        if ( decodedElements.count() > 1 )
        {
            WsjtxEntry entry;
            entry.decode = decode;
            entry.callsign = decodedElements.at(1);

            if ( wsjtxTableModel->callsignExists(entry) )
            {
                entry.dateTime = QDateTime::currentDateTime().toTimeZone(QTimeZone::utc());
                entry.dxcc = Data::instance()->lookupDxcc(entry.callsign);
                entry.status = Data::instance()->currentDxccStatus(entry.dxcc.dxcc,
                                                                   client.band,
                                                                   BandPlan::MODE_GROUP_STRING_DIGITAL);
                entry.dxccStatusSatellite = Data::instance()->isSatelliteDxccContext();
                entry.receivedTime = QDateTime::currentDateTimeUtc();
                entry.freq = client.frequency;
                entry.band = client.band;
                entry.decodedMode = client.status.mode;
                entry.comment = decode.message;
                entry.bandPlanMode = ( client.status.mode == "FT8" ) ? BandPlan::BAND_MODE_FT8
                   : ( client.status.mode == "FT4" ) ? BandPlan::BAND_MODE_FT4
                   : ( client.status.mode == "FT2" ) ? BandPlan::BAND_MODE_FT2
                   : BandPlan::BAND_MODE_DIGITAL;
                entry.modeGroupString = BandPlan::bandMode2BandModeGroupString(entry.bandPlanMode);
                entry.spotPeriod = client.spotPeriod;
                entry.spotter = profile.callsign.toUpper();
                entry.dxcc_spotter = Data::instance()->lookupDxcc(entry.spotter);
                // fix dxcc_spotter based on station prifile setting - cont is not calculated
                entry.dxcc_spotter.country = profile.country;
                entry.dxcc_spotter.cqz = profile.cqz;
                entry.dxcc_spotter.ituz = profile.ituz;
                entry.dxcc_spotter.dxcc = profile.dxcc;
                entry.distance = 0.0;
                entry.dupeCount = Data::countDupe(entry.callsign, entry.band, BandPlan::MODE_GROUP_STRING_DIGITAL);
                // it is not needed to update entry.callsign_clubs because addOrReplaceEntry does not
                // update it. Only CQ provides the club membeship info

                entry.grid = wsjtxTableModel->getEntry(decode.id, entry.callsign).grid;
                wsjtxTableModel->addOrReplaceEntry(entry);
                emit filteredCQSpot(entry);
                emit updatedCQSpot(entry);
            }
        }
    }

    wsjtxTableModel->spotAging();

    ui->tableView->repaint();
}

void WsjtxWidget::statusReceived(WsjtxStatus newStatus)
{
    FCT_IDENTIFICATION;

    if ( newStatus.id.isEmpty() )
        return;

    ClientState &client = clients[newStatus.id];
    const bool firstStatus = !client.initialized;
    const bool frequencyHasChanged = firstStatus
                                     || client.status.dial_freq != newStatus.dial_freq;
    const bool modeHasChanged = firstStatus
                                || client.status.mode != newStatus.mode;
    const bool targetHasChanged = !firstStatus
                                  && (client.status.dx_call != newStatus.dx_call
                                      || client.status.dx_grid != newStatus.dx_grid);
    const bool replyConfirmed = pendingReplyId == newStatus.id
                                && !pendingReplyCallsign.isEmpty()
                                && pendingReplyCallsign == newStatus.dx_call;
    const bool claimsOwnership = !newStatus.dx_call.isEmpty()
                                 && (firstStatus
                                     || client.status.dx_call != newStatus.dx_call
                                     || replyConfirmed);

    if ( !firstStatus && (frequencyHasChanged || modeHasChanged) )
        clearSpots(newStatus.id);

    client.status = newStatus;
    client.frequency = Hz2MHz(newStatus.dial_freq);
    client.band = BandPlan::freq2Band(client.frequency).name;
    if ( modeHasChanged )
        client.spotPeriod = WsjtxUDPReceiver::modePeriodLength(newStatus.mode);
    client.initialized = true;

    bool ownerChanged = false;
    // Routine status traffic must not make another WSJT-X instance active.
    // A changed non-empty DX call is the protocol event that claims New QSO.
    if ( newQSOOwnerId.isEmpty()
         && (!ownerClaimRequired || claimsOwnership) )
    {
        newQSOOwnerId = newStatus.id;
        ownerClaimRequired = false;
        ownerChanged = true;
    }
    else if ( newQSOOwnerId != newStatus.id
              && claimsOwnership
              && (!firstStatus || replyConfirmed) )
    {
        newQSOOwnerId = newStatus.id;
        ownerClaimRequired = false;
        ownerChanged = true;
    }

    if ( newQSOOwnerId == newStatus.id )
    {
        emitOwnerState(client,
                       ownerChanged || frequencyHasChanged || modeHasChanged,
                       (ownerChanged && !client.status.dx_call.isEmpty())
                       || targetHasChanged || replyConfirmed);
    }

    if ( replyConfirmed )
    {
        pendingReplyId.clear();
        pendingReplyCallsign.clear();
    }

    wsjtxTableModel->spotAging();
    ui->tableView->repaint();
}

void WsjtxWidget::emitOwnerState(const ClientState &client,
                                 bool radioChanged,
                                 bool targetChanged)
{
    FCT_IDENTIFICATION;

    if ( radioChanged && !Rig::instance()->isRigConnected() )
    {
        emit frequencyChanged(VFO1,
                              client.frequency,
                              client.frequency,
                              client.frequency);

        const QPair<QString, QString> legacy = Data::instance()->legacyMode(client.status.mode);
        const QString mode = !legacy.first.isEmpty() ? legacy.first
                                                     : client.status.mode.toUpper();
        const QString submode = !legacy.first.isEmpty() ? legacy.second
                                                        : QString();
        emit modeChanged(VFO1, client.status.mode, mode, submode, 0);
    }

    if ( targetChanged )
        emit callsignSelected(client.status.dx_call,
                              client.status.dx_grid,
                              client.status.id);
}

void WsjtxWidget::clearSpots(const QString &id)
{
    FCT_IDENTIFICATION;

    if ( id.isEmpty() )
        wsjtxTableModel->clear();
    else
        wsjtxTableModel->removeSource(id);

    emit spotsCleared(id);
}

void WsjtxWidget::clientClosed(const QString &id)
{
    FCT_IDENTIFICATION;

    const bool ownerClosed = newQSOOwnerId == id;
    clients.remove(id);
    if ( pendingReplyId == id )
    {
        pendingReplyId.clear();
        pendingReplyCallsign.clear();
    }
    if ( ownerClosed )
        newQSOOwnerId.clear();

    if ( !newQSOOwnerId.isEmpty() )
        return;

    if ( clients.isEmpty() )
    {
        ownerClaimRequired = false;
        if ( ownerClosed )
            emit callsignSelected(QString(), QString(), id);
        return;
    }

    if ( clients.size() == 1 )
    {
        const auto replacement = clients.constBegin();
        newQSOOwnerId = replacement.key();
        ownerClaimRequired = false;
        emitOwnerState(replacement.value(), true, true);
    }
    else if ( ownerClosed )
    {
        ownerClaimRequired = true;
        emit callsignSelected(QString(), QString(), id);
    }
}

void WsjtxWidget::tableViewDoubleClicked(QModelIndex index)
{
    FCT_IDENTIFICATION;

    const QModelIndex &source_index = proxyModel->mapToSource(index);

    const WsjtxEntry entry = wsjtxTableModel->getEntry(source_index);
    //emit callsignSelected(entry.callsign, entry.grid); // it is not needed to send this - Qlog receives the change via WSJTX
    requestReply(entry);
}

void WsjtxWidget::callsignClicked(QString id, QString callsign)
{
    FCT_IDENTIFICATION;

    const WsjtxEntry entry = wsjtxTableModel->getEntry(id, callsign);
    if ( entry.callsign.isEmpty() )
        return;

    requestReply(entry);
}

void WsjtxWidget::requestReply(const WsjtxEntry &entry)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << entry.decode.id << entry.callsign;

    pendingReplyId = entry.decode.id;
    pendingReplyCallsign = entry.callsign;
    emit reply(entry);
}

void WsjtxWidget::tableViewClicked(QModelIndex)
{
    FCT_IDENTIFICATION;

    //const QModelIndex &source_index = proxyModel->mapToSource(index);

    //lastSelectedCallsign = wsjtxTableModel->getEntry(source_index).callsign;
}

void WsjtxWidget::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    wsjtxTableModel->removeSpot(record.value("callsign").toString(),
                                record.value("band").toString());
}

void WsjtxWidget::displayedColumns()
{
    FCT_IDENTIFICATION;

    ColumnSettingSimpleDialog dialog(ui->tableView);
    dialog.exec();
    saveTableHeaderState();
}

void WsjtxWidget::actionFilter()
{
    FCT_IDENTIFICATION;

    WsjtxFilterDialog dialog;

    if (dialog.exec() == QDialog::Accepted)
    {
        reloadSetting();
        clearTable();
    }
}

uint WsjtxWidget::dxccStatusFilterValue() const
{
    FCT_IDENTIFICATION;

    return LogParam::getWsjtxFilterDxccStatus();
}

QString WsjtxWidget::contFilterRegExp() const
{
    FCT_IDENTIFICATION;

    return LogParam::getWsjtxFilterContRE();
}

int WsjtxWidget::getDistanceFilterValue() const
{
    FCT_IDENTIFICATION;

    return LogParam::getWsjtxFilterDistance();
}

int WsjtxWidget::getSNRFilterValue() const
{
    FCT_IDENTIFICATION;

    return LogParam::getWsjtxFilterSNR();
}

QStringList WsjtxWidget::dxMemberList() const
{
    FCT_IDENTIFICATION;

    return LogParam::getWsjtxMemberlists();
}

void WsjtxWidget::reloadSetting()
{
    FCT_IDENTIFICATION;

    contregexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    contregexp.setPattern(contFilterRegExp());
    dxccStatusFilter = dxccStatusFilterValue();
    distanceFilter = getDistanceFilterValue();
    snrFilter = getSNRFilterValue();
    QStringList tmp = dxMemberList();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    dxMemberFilter = QSet<QString>(tmp.begin(), tmp.end());
#else /* Due to ubuntu 20.04 where qt5.12 is present */
    dxMemberFilter = QSet<QString>(QSet<QString>::fromList(tmp));
#endif

    ui->filteredLabel->setVisible(isFilterEnabled());
}

void WsjtxWidget::clearTable()
{
    FCT_IDENTIFICATION;

    clearSpots(QString());
}

void WsjtxWidget::refreshStatusColors()
{
    FCT_IDENTIFICATION;

    wsjtxTableModel->refreshStatusColors();
    redrawMapSpots();
}

void WsjtxWidget::recalculateDxccStatus()
{
    FCT_IDENTIFICATION;

    wsjtxTableModel->recalculateDxccStatus();
    redrawMapSpots();
}

void WsjtxWidget::updateSpotsDxccStatus(const QSet<uint> &entities)
{
    FCT_IDENTIFICATION;

    if ( wsjtxTableModel->updateSpotsDxccStatus(entities) )
        redrawMapSpots();
}

void WsjtxWidget::redrawMapSpots()
{
    FCT_IDENTIFICATION;

    emit spotsCleared(QString());
    const QList<WsjtxEntry> entries = wsjtxTableModel->entries();
    for ( const WsjtxEntry &entry : entries )
        emit filteredCQSpot(entry);
}

void WsjtxWidget::saveTableHeaderState()
{
    FCT_IDENTIFICATION;

    LogParam::setWsjtxWidgetState(ui->tableView->horizontalHeader()->saveState());
}

void WsjtxWidget::restoreTableHeaderState()
{
    FCT_IDENTIFICATION;

    const QByteArray &state = LogParam::getWsjtxWidgetState();

    if (!state.isEmpty())
    {
        ui->tableView->horizontalHeader()->restoreState(state);
    }
}

bool WsjtxWidget::isFilterEnabled()
{
    FCT_IDENTIFICATION;

    return dxccStatusFilter != (DxccStatus::NewEntity | DxccStatus::NewBand |
                                DxccStatus::NewMode   | DxccStatus::NewSlot |
                                DxccStatus::Worked    | DxccStatus::Confirmed)
        || contregexp.pattern().count("|") != 7
        || distanceFilter > 0
        || snrFilter > -41
        || !dxMemberFilter.isEmpty();
}

WsjtxWidget::~WsjtxWidget()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void WsjtxWidget::finalizeBeforeAppExit()
{
    FCT_IDENTIFICATION;

    saveTableHeaderState();
}
