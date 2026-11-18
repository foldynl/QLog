#include <QGraphicsTextItem>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QPainter>
#include <QVector3D>
#include <QtMath>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>
#include "OnlineMapWidget.h"
#include "core/debug.h"
#include "data/Gridsquare.h"
#include "data/StationProfile.h"
#include "data/AntProfile.h"
#include "core/debug.h"
#include "core/PropConditions.h"
#include "data/Band.h"
#include "data/Data.h"
#include "rotator/Rotator.h"
#include "rig/Rig.h"
#include "data/BandPlan.h"
#include "rig/macros.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.onlinemapwidget");

OnlineMapWidget::OnlineMapWidget(QWidget *parent):
  QWebEngineView(parent),
  mapController(new MapPageController(QStringLiteral("onlinemap"), this)),
  prop_cond(nullptr),
  lastSeenAzimuth(0.0),
  lastSeenElevation(0.0),
  isRotConnected(false)
{
    FCT_IDENTIFICATION;

    mapController->attach(this,
                          MapLayer::Grid
                          | MapLayer::GridStatus
                          | MapLayer::Grayline
                          | MapLayer::Aurora
                          | MapLayer::Muf
                          | MapLayer::Ibp
                          | MapLayer::Beam
                          | MapLayer::Chat
                          | MapLayer::Wsjtx
                          | MapLayer::HeardMe);
    connect(mapController.data(), &MapPageController::loaded,
            this, &OnlineMapWidget::finishLoading);
    setFocusPolicy(Qt::ClickFocus);
    setContextMenuPolicy(Qt::NoContextMenu);

    double freq = LogParam::getNewContactFreq();
    freq += RigProfilesManager::instance()->getCurProfile1().ritOffset;

    setIBPBand(VFO1, 0.0, freq, 0.0);

    connect(mapController.data(), &MapPageController::chatCallsignPressed, this, &OnlineMapWidget::chatCallsignTrigger);
    connect(mapController.data(), &MapPageController::wsjtxCallsignPressed, this, &OnlineMapWidget::wsjtxCallsignTrigger);
    connect(mapController.data(), &MapPageController::IBPPressed, this, &OnlineMapWidget::IBPCallsignTrigger);
    connect(mapController.data(), &MapPageController::antennaAzimuthRequested, this, &OnlineMapWidget::antennaAzimuthRequested);
    connect(mapController.data(), &MapPageController::layerVisibilityChanged,
            this, [this](const QString &key, bool visible)
    {
        if ( key == QLatin1String("heardMeLayer") )
            emit heardMeLayerVisibilityChanged(visible);
        else if ( key == QLatin1String("gridStatusLayer") && visible )
            refreshGridStatus();
    });
}

bool OnlineMapWidget::isHeardMeLayerVisible() const
{
    FCT_IDENTIFICATION;

    const QString key = QStringLiteral("heardMeLayer");
    const QStringList states = LogParam::getMapLayerStates(QStringLiteral("onlinemap"));
    return states.contains(key)
           && LogParam::getMapLayerState(QStringLiteral("onlinemap"), key);
}

void OnlineMapWidget::setTarget(double lat, double lon)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << lat << " " << lon;

    if ( ! qIsNaN(lat) && ! qIsNaN(lon) )
    {
        /* Draw a new path */
        const Gridsquare myGrid = Gridsquare::mapDisplayGrid(StationProfilesManager::instance()->getCurProfile1().locator);

        if ( myGrid.isValid() )
        {
            mapController->drawPath(QList<MapCoordinate>()
                                    << MapCoordinate(myGrid.getLatitude(), myGrid.getLongitude())
                                    << MapCoordinate(lat, lon));
        }
        else
        {
            mapController->clearPath();
        }
    }
    else
    {
        mapController->clearPath();
    }
}

void OnlineMapWidget::setAntennaTarget(double azimuth)
{
    FCT_IDENTIFICATION;

    mapController->setAntennaTarget(azimuth);
}

void OnlineMapWidget::changeTheme(int theme, bool isDark)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << theme << isDark;

    //theme == 1 dart
    mapController->setDarkTheme(isDark);
}

void OnlineMapWidget::auroraDataUpdate()
{
    FCT_IDENTIFICATION;

    QList<MapHeatPoint> mapPoints;

    if ( !prop_cond ) return;

    if ( prop_cond->isAuroraMapValid() )
    {
        const QList<GenericValueMap<double>::MapPoint> &points = prop_cond->getAuroraPoints();

        for ( const GenericValueMap<double>::MapPoint &point : points )
        {
            if ( point.value > 10 )
            {
                const double wrappedLongitude = point.longitude < 0.0
                                                ? point.longitude + 360.0
                                                : point.longitude - 360.0;
                mapPoints << MapHeatPoint(point.latitude, point.longitude, point.value)
                          << MapHeatPoint(point.latitude, wrappedLongitude, point.value);
            }
        }
    }

    mapController->setAuroraData(mapPoints);
}

void OnlineMapWidget::mufDataUpdate()
{
    FCT_IDENTIFICATION;

    QList<MapPoint> mapPoints;

    if ( !prop_cond ) return;

    if ( prop_cond->isMufMapValid() )
    {
        const QList<GenericValueMap<double>::MapPoint> &points = prop_cond->getMUFPoints();

        for ( const GenericValueMap<double>::MapPoint &point : points )
        {
            const QString label = QString::number(point.value, 'f', 0);
            const double wrappedLongitude = point.longitude < 0.0
                                            ? point.longitude + 360.0
                                            : point.longitude - 360.0;
            mapPoints << MapPoint(label, point.latitude, point.longitude)
                      << MapPoint(label, point.latitude, wrappedLongitude);
        }
    }

    mapController->drawMuf(mapPoints);
}

void OnlineMapWidget::setIBPBand(VFOID vfoid, double, double ritFreq, double)
{
    FCT_IDENTIFICATION;

    // Online map tracks the RX frequency only
    if ( vfoid == VFO2 )
        return;

    setCurrentBand(BandPlan::freq2Band(ritFreq).name);
}

void OnlineMapWidget::setCurrentBand(const QString &band)
{
    FCT_IDENTIFICATION;

    mapController->setCurrentBand(band);

    // called on every rig frequency change - query the log only when the band changes
    if ( band == gridStatusBand )
        return;

    gridStatusBand = band;
    refreshGridStatus();
}

bool OnlineMapWidget::isGridStatusLayerVisible() const
{
    FCT_IDENTIFICATION;

    const QString key = QStringLiteral("gridStatusLayer");
    const QStringList states = LogParam::getMapLayerStates(QStringLiteral("onlinemap"));
    return states.contains(key)
           && LogParam::getMapLayerState(QStringLiteral("onlinemap"), key);
}

void OnlineMapWidget::refreshGridStatus()
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << gridStatusBand;

    QStringList confirmedGrids;
    QStringList workedGrids;

    // The layer is off - nothing to show; data are loaded again when it is enabled
    if ( gridStatusBand.isEmpty() || !isGridStatusLayerVisible() )
    {
        mapController->setBandGridStatus(confirmedGrids, workedGrids);
        return;
    }

    // Same rules as the DXCC statistics: satellite QSOs are not counted on the band,
    // only QSOs from the current station profile's DXCC are used
    QString stmt(QLatin1String("SELECT SUBSTR(UPPER(gridsquare), 1, 4), UPPER(vucc_grids), "
                               "       MAX(CASE WHEN eqsl_qsl_rcvd = 'Y' OR lotw_qsl_rcvd = 'Y' OR qsl_rcvd = 'Y' "
                               "                THEN 1 ELSE 0 END) "
                               "FROM contacts "
                               "WHERE band = :band "
                               "      AND UPPER(COALESCE(prop_mode, '')) <> 'SAT' "
                               "      AND (gridsquare IS NOT NULL OR vucc_grids IS NOT NULL) "));

    // StationProfile::operator!= is not const
    StationProfile profile = StationProfilesManager::instance()->getCurProfile1();
    const bool filterDxcc = ( profile != StationProfile() );

    if ( filterDxcc )
        stmt.append(QLatin1String(" AND my_dxcc = :mydxcc "));

    stmt.append(QLatin1String("GROUP BY 1, 2"));

    QSqlQuery query;

    if ( !query.prepare(stmt) )
    {
        qCWarning(runtime) << "Cannot prepare grid status query:" << query.lastError();
        return;
    }

    query.bindValue(":band", gridStatusBand);
    if ( filterDxcc )
        query.bindValue(":mydxcc", profile.dxcc);

    if ( !query.exec() )
    {
        qCWarning(runtime) << "Cannot get grid status:" << query.lastError();
        return;
    }

    static const QRegularExpression squareRE(QStringLiteral("^[A-R]{2}[0-9]{2}$"));
    QHash<QString, bool> squares; // square -> confirmed

    auto addSquare = [&squares](const QString &square, bool confirmed)
    {
        if ( !squareRE.match(square).hasMatch() )
            return;

        bool &squareConfirmed = squares[square];
        squareConfirmed = squareConfirmed || confirmed;
    };

    while ( query.next() )
    {
        const bool confirmed = query.value(2).toInt() > 0;

        addSquare(query.value(0).toString(), confirmed);

        // VUCC_GRIDS holds the squares of grid-line/grid-corner QSOs (empty items fail the check)
        const QStringList vuccGrids = query.value(1).toString().split(',');
        for ( const QString &vuccGrid : vuccGrids )
            addSquare(vuccGrid.trimmed().left(4), confirmed);
    }

    for ( auto it = squares.constBegin(); it != squares.constEnd(); ++it )
        ( it.value() ? confirmedGrids : workedGrids ) << it.key();

    qCDebug(runtime) << "Band" << gridStatusBand << "confirmed" << confirmedGrids.size()
                     << "worked" << workedGrids.size();

    mapController->setBandGridStatus(confirmedGrids, workedGrids);
}

void OnlineMapWidget::antPositionChanged(double in_azimuth, double in_elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth << " " << in_elevation;

    if ( ! isRotConnected )
        return;

    lastSeenAzimuth = in_azimuth;
    lastSeenElevation = in_elevation;

    /* Draw a new path */
    const Gridsquare myGrid = Gridsquare::mapDisplayGrid(StationProfilesManager::instance()->getCurProfile1().locator);

    if ( myGrid.isValid() )
    {
        double azimuthBeamWidth = AntProfilesManager::instance()->getCurProfile1().azimuthBeamWidth;

        mapController->drawAntPath(MapCoordinate(myGrid.getLatitude(), myGrid.getLongitude()),
                                   in_azimuth,
                                   azimuthBeamWidth);
    }
    else
    {
        // clean paths
        mapController->clearAntPath();
    }
}

void OnlineMapWidget::rotConnected()
{
    FCT_IDENTIFICATION;

    isRotConnected = true;
    Rotator::instance()->sendState();
}

void OnlineMapWidget::rotDisconnected()
{
    FCT_IDENTIFICATION;

    isRotConnected = false;

    // clear the Ant Path
    mapController->clearAntPath();
}

void OnlineMapWidget::finishLoading()
{
    FCT_IDENTIFICATION;

    flyToMyQTH();
    auroraDataUpdate();
}

void OnlineMapWidget::chatCallsignTrigger(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    emit chatCallsignPressed(callsign);
}

void OnlineMapWidget::wsjtxCallsignTrigger(const QString &id, const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << id << callsign;

    emit wsjtxCallsignPressed(id, callsign);
}

void OnlineMapWidget::IBPCallsignTrigger(const QString &callsign, double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign << freq;

    Rig::instance()->setFrequency(MHz2Hz(freq));
    Rig::instance()->setMode("CW", QString());
}

void OnlineMapWidget::flyToMyQTH()
{
    FCT_IDENTIFICATION;

    /* focus current location */
    const Gridsquare myGrid = Gridsquare::mapDisplayGrid(StationProfilesManager::instance()->getCurProfile1().locator);

    if ( myGrid.isValid() )
    {
        mapController->flyToPoint(MapPoint(QString(),
                                           myGrid.getLatitude(),
                                           myGrid.getLongitude(),
                                           QStringLiteral("yellowIcon")),
                                  4);
    }
    antPositionChanged(lastSeenAzimuth, lastSeenElevation);
}

void OnlineMapWidget::drawChatUsers(const QList<KSTUsersInfo> &list)
{
    FCT_IDENTIFICATION;

    QList<MapPoint> chatUsers;

    for ( const KSTUsersInfo &user : list )
    {
        if ( user.grid.isValid() )
        {
            chatUsers << MapPoint(user.callsign,
                                  user.grid.getLatitude(),
                                  user.grid.getLongitude(),
                                  QStringLiteral("yellowIcon"));
        }
    }

    mapController->drawChatPoints(chatUsers);
}

void OnlineMapWidget::drawWSJTXSpot(const WsjtxEntry &spot)
{
    FCT_IDENTIFICATION;

    const Gridsquare spotGrid = Gridsquare::mapDisplayGrid(spot.grid);

    if ( spotGrid.isValid() )
    {
        const QColor background = Data::statusToColor(spot.status, spot.dupeCount, QColor(Qt::white));
        mapController->addWsjtxSpot(MapPoint(spot.callsign,
                                             spotGrid.getLatitude(),
                                             spotGrid.getLongitude()),
                                    spot.decode.id,
                                    Data::colorToHTMLColor(background),
                                    Data::colorToHTMLColor(Data::textColorForBackground(background,
                                                                                       QColor(Qt::black))),
                                    spot.decode.snr > -15);
    }
}

void OnlineMapWidget::clearWSJTXSpots(const QString &id)
{
    FCT_IDENTIFICATION;

    mapController->clearWsjtxSpots(id);
}

void OnlineMapWidget::clearHeardMeSpots()
{
    FCT_IDENTIFICATION;

    mapController->clearHeardMeSpots();
}

void OnlineMapWidget::setHeardMeMode(const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;
    mapController->setHeardMeMode(mode);
}

void OnlineMapWidget::addHeardMePoint(const PskDecode &spot,
                                      PSKReporter::Direction direction)
{
    FCT_IDENTIFICATION;

    const bool sentBy = (direction == PSKReporter::Direction::SentBy);
    HeardMeSpot heardMeSpot;
    heardMeSpot.callsign = sentBy ? spot.receiverCallsign
                                  : spot.senderCallsign;
    heardMeSpot.locator = sentBy ? spot.receiverLocator
                                 : spot.senderLocator;
    heardMeSpot.frequency = spot.frequency;
    heardMeSpot.report = spot.report;
    heardMeSpot.status = spot.status;
    heardMeSpot.dupeCount = spot.dupeCount;

    addHeardMeSpot(heardMeSpot);
}

void OnlineMapWidget::addHeardMeSpot(const HeardMeSpot &spot)
{
    FCT_IDENTIFICATION;

    const Gridsquare spotGrid = Gridsquare::mapDisplayGrid(spot.locator);
    const Band bandDecoded = BandPlan::freq2Band(spot.frequency);
    const bool pskReporterSpot = spot.displayGroup == HeardMeSpot::DisplayGroup::PskReporter;
    const QString displayGroup = spot.displayGroup == HeardMeSpot::DisplayGroup::CW
                                 ? QStringLiteral("CW")
                                 : spot.displayGroup == HeardMeSpot::DisplayGroup::RTTY
                                   ? QStringLiteral("RTTY")
                                   : QStringLiteral("PSK");

    if ( spotGrid.isValid() && !bandDecoded.name.isEmpty() )
    {
        const QColor background = Data::statusToColor(spot.status,
                                                       spot.dupeCount,
                                                       QColor(Qt::white));
        mapController->addHeardMePoint(MapPoint(spot.callsign,
                                                spotGrid.getLatitude(),
                                                spotGrid.getLongitude()),
                                       spot.report,
                                       bandDecoded.name,
                                       displayGroup,
                                       Data::colorToHTMLColor(background),
                                       0.8,
                                       spot.report > (pskReporterSpot ? -15 : 15));
    }
}

OnlineMapWidget::~OnlineMapWidget()
{
    FCT_IDENTIFICATION;
}

void OnlineMapWidget::assignPropConditions(PropConditions *conditions)
{
    FCT_IDENTIFICATION;

    prop_cond = conditions;
}
