#include <QGraphicsTextItem>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QPainter>
#include <QVector3D>
#include <QtMath>
#include <QFile>
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
  contact(nullptr),
  lastSeenAzimuth(0.0),
  lastSeenElevation(0.0),
  isRotConnected(false)
{
    FCT_IDENTIFICATION;

    mapController->attach(this,
                          MapLayer::Grid
                          | MapLayer::Grayline
                          | MapLayer::Aurora
                          | MapLayer::Muf
                          | MapLayer::Ibp
                          | MapLayer::Beam
                          | MapLayer::Chat
                          | MapLayer::Wsjtx);
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

    // redraw ant path because QSO distance can change
    antPositionChanged(lastSeenAzimuth, lastSeenElevation);
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
                mapPoints << MapHeatPoint(point.latitude, point.longitude, point.value)
                          << MapHeatPoint(point.latitude, point.longitude - 360, point.value);
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
            mapPoints << MapPoint(label, point.latitude, point.longitude)
                      << MapPoint(label, point.latitude, point.longitude - 360);
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

    mapController->setCurrentBand(BandPlan::freq2Band(ritFreq).name);
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
        double beamLen = 3000; // in km
        double azimuthBeamWidth = AntProfilesManager::instance()->getCurProfile1().azimuthBeamWidth;

        if ( contact )
        {
            double newBeamLen = contact->getQSODistance();
            if ( !qIsNaN(newBeamLen) )
            {
                beamLen = newBeamLen;
            }
        }
        mapController->drawAntPath(MapCoordinate(myGrid.getLatitude(), myGrid.getLongitude()),
                                   beamLen,
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

void OnlineMapWidget::wsjtxCallsignTrigger(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    emit wsjtxCallsignPressed(callsign);
}

void OnlineMapWidget::IBPCallsignTrigger(const QString &callsign, double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign << freq;

    Rig::instance()->setFrequency(MHz(freq));
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
    // redraw ant path because QSO distance can change
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
        mapController->addWsjtxSpot(MapPoint(spot.callsign,
                                             spotGrid.getLatitude(),
                                             spotGrid.getLongitude()),
                                    Data::colorToHTMLColor(Data::statusToColor(spot.status, spot.dupeCount, QColor(Qt::white))));
    }
}

void OnlineMapWidget::clearWSJTXSpots()
{
    FCT_IDENTIFICATION;

    mapController->clearWsjtxSpots();
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

void OnlineMapWidget::registerContactWidget(const NewContactWidget *contactWidget)
{
    FCT_IDENTIFICATION;
    contact = contactWidget;
}
