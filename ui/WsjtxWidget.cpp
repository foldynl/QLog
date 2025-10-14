#include <QDebug>
#include <QSortFilterProxyModel>
#include <QScrollBar>

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

MODULE_IDENTIFICATION("qlog.ui.wsjtxswidget");

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
    ui->tableView->sortByColumn(WsjtxTableModel::COLUMN_LAST_ACTIVITY, Qt::DescendingOrder);
    ui->tableView->setItemDelegateForColumn(WsjtxTableModel::COLUMN_DISTANCE,
                                            new DistanceFormatDelegate(1, 0.1, ui->tableView));
    ui->tableView->horizontalHeader()->setSectionsMovable(true);
    ui->tableView->addAction(ui->actionFilter);
    ui->tableView->addAction(ui->actionDisplayedColumns);

    restoreTableHeaderState();

    reloadSetting();
}

void WsjtxWidget::decodeReceived(WsjtxDecode decode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<decode.message;

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();

    if ( decode.message.startsWith("CQ") )
    {
        QRegularExpressionMatch match = cqRE.match((decode.message));

        if (  match.hasMatch() )
        {
            WsjtxEntry entry;

            entry.dateTime = QDateTime::currentDateTime().toTimeZone(QTimeZone::utc());
            entry.decode = decode;
            entry.callsign = match.captured(3);
            entry.grid = match.captured(4);
            entry.dxcc = Data::instance()->lookupDxcc(entry.callsign);
            entry.status = Data::instance()->dxccStatus(entry.dxcc.dxcc, currBand, BandPlan::MODE_GROUP_STRING_DIGITAL);
            entry.receivedTime = QDateTime::currentDateTimeUtc();
            entry.freq = currFreq;
            entry.band = currBand;
            entry.decodedMode = status.mode;
            entry.bandPlanMode = (status.mode == "FT8") ?  BandPlan::BAND_MODE_FT8 : BandPlan::BAND_MODE_DIGITAL;
            entry.modeGroupString = BandPlan::bandMode2BandModeGroupString(entry.bandPlanMode);
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
            entry.callsign = decodedElements.at(1);

            if ( wsjtxTableModel->callsignExists(entry) )
            {
                entry.dateTime = QDateTime::currentDateTime().toTimeZone(QTimeZone::utc());
                entry.dxcc = Data::instance()->lookupDxcc(entry.callsign);
                entry.status = Data::instance()->dxccStatus(entry.dxcc.dxcc, currBand, BandPlan::MODE_GROUP_STRING_DIGITAL);
                entry.decode = decode;
                entry.receivedTime = QDateTime::currentDateTimeUtc();
                entry.freq = currFreq;
                entry.band = currBand;
                entry.decodedMode = status.mode;
                entry.comment = decode.message;
                entry.bandPlanMode = (status.mode == "FT8") ?  BandPlan::BAND_MODE_FT8 : BandPlan::BAND_MODE_DIGITAL;
                entry.modeGroupString = BandPlan::bandMode2BandModeGroupString(entry.bandPlanMode);
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

                wsjtxTableModel->addOrReplaceEntry(entry);
                emit filteredCQSpot(entry);
            }
        }
    }

    wsjtxTableModel->spotAging();

    ui->tableView->repaint();
}

void WsjtxWidget::statusReceived(WsjtxStatus newStatus)
{
    FCT_IDENTIFICATION;

    if ( this->status.dial_freq != newStatus.dial_freq )
    {
        currFreq = Hz2MHz(newStatus.dial_freq);
        currBand = BandPlan::freq2Band(currFreq).name;
        ui->freqLabel->setText(QString("%1 MHz").arg(QSTRING_FREQ(currFreq)));
        clearTable();
    }

    if ( this->status.dx_call != newStatus.dx_call
         || this->status.dx_grid != newStatus.dx_grid )
    {
        emit callsignSelected(newStatus.dx_call, newStatus.dx_grid);
    }

    if ( this->status.mode != newStatus.mode )
    {
        ui->modeLabel->setText(newStatus.mode);
        wsjtxTableModel->setCurrentSpotPeriod(WsjtxUDPReceiver::modePeriodLenght(newStatus.mode)); /*currently, only Status has a correct Mode in the message */
        clearTable();
    }

    if ( !Rig::instance()->isRigConnected() )
    {
        const QPair<QString, QString>& legacy = Data::instance()->legacyMode(newStatus.mode);
        QString mode = ( !legacy.first.isEmpty() ) ? legacy.first
                                                   : newStatus.mode.toUpper();
        QString submode = ( !legacy.first.isEmpty() ) ? legacy.second
                                                      : QString();
        emit modeChanged(VFO1, newStatus.mode, mode, submode, 0);
        emit frequencyChanged(VFO1, currFreq, currFreq, currFreq);
    }

    status = newStatus;
    wsjtxTableModel->spotAging();
    ui->tableView->repaint();
}

void WsjtxWidget::tableViewDoubleClicked(QModelIndex index)
{
    FCT_IDENTIFICATION;

    const QModelIndex &source_index = proxyModel->mapToSource(index);

    const WsjtxEntry &entry = wsjtxTableModel->getEntry(source_index);
    //emit callsignSelected(entry.callsign, entry.grid); // it is not needed to send this - Qlog receives the change via WSJTX
    emit reply(entry.decode);
}

void WsjtxWidget::callsignClicked(QString callsign)
{
    FCT_IDENTIFICATION;

    const WsjtxEntry &entry = wsjtxTableModel->getEntry(callsign);
    if ( entry.callsign.isEmpty() )
        return;

    emit callsignSelected(callsign, entry.grid);
    emit reply(entry.decode);
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

    wsjtxTableModel->removeSpot(record.value("callsign").toString());
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

    ui->filteredLabel->setHidden(!isFilterEnabled());
}

void WsjtxWidget::clearTable()
{
    FCT_IDENTIFICATION;

    wsjtxTableModel->clear();
    emit spotsCleared();
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
