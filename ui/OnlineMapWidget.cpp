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
  main_page(new WebEnginePage(this)),
  isMainPageLoaded(false),
  webChannelHandler("onlinemap",parent),
  prop_cond(nullptr),
  contact(nullptr),
  lastSeenAzimuth(0.0),
  lastSeenElevation(0.0),
  isRotConnected(false)
{
    FCT_IDENTIFICATION;

    main_page->setWebChannel(&channel);

    setPage(main_page);
    main_page->load(QUrl(QLatin1String("qrc:/res/map/onlinemap.html")));
    connect(this, &OnlineMapWidget::loadFinished, this, &OnlineMapWidget::finishLoading);

    setFocusPolicy(Qt::ClickFocus);
    setContextMenuPolicy(Qt::NoContextMenu);
    channel.registerObject("layerControlHandler", &webChannelHandler);

    double freq = LogParam::getNewContactFreq();
    freq += RigProfilesManager::instance()->getCurProfile1().ritOffset;

    setIBPBand(VFO1, 0.0, freq, 0.0);

    connect(&webChannelHandler, &MapWebChannelHandler::chatCallsignPressed, this, &OnlineMapWidget::chatCallsignTrigger);
    connect(&webChannelHandler, &MapWebChannelHandler::wsjtxCallsignPressed, this, &OnlineMapWidget::wsjtxCallsignTrigger);
    connect(&webChannelHandler, &MapWebChannelHandler::IBPPressed, this, &OnlineMapWidget::IBPCallsignTrigger);
}

void OnlineMapWidget::setTarget(double lat, double lon)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << lat << " " << lon;

    QString targetJavaScript;

    if ( ! qIsNaN(lat) && ! qIsNaN(lon) )
    {
        /* Draw a new path */
        Gridsquare myGrid(StationProfilesManager::instance()->getCurProfile1().locator);

        if ( myGrid.isValid() )
        {
            targetJavaScript += QString("drawPath([{lat: %1, lng: %2}, {lat: %3, lng: %4}]);").arg(myGrid.getLatitude())
                                                                                              .arg(myGrid.getLongitude())
                                                                                              .arg(lat)
                                                                                              .arg(lon);
        }
    }
    else
        targetJavaScript = QLatin1String("drawPath([]);");

    runJavaScript(targetJavaScript);

    // redraw ant path because QSO distance can change
    antPositionChanged(lastSeenAzimuth, lastSeenElevation);
}

void OnlineMapWidget::changeTheme(int theme, bool isDark)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << theme << isDark;

    QString themeJavaScript;

    //theme == 1 dart
    themeJavaScript
        = (isDark == 1)
              ? QLatin1String(
                    "map.getPanes().tilePane.style.webkitFilter=\"brightness(0.6) invert(1) "
                    "contrast(3) hue-rotate(200deg) saturate(0.3) brightness(0.9)\";")
              : QLatin1String("map.getPanes().tilePane.style.webkitFilter=\"\";");

    runJavaScript(themeJavaScript);
}

void OnlineMapWidget::auroraDataUpdate()
{
    FCT_IDENTIFICATION;

    QStringList mapPoints;

    if ( !prop_cond ) return;

    if ( prop_cond->isAuroraMapValid() )
    {
        const QList<GenericValueMap<double>::MapPoint> &points = prop_cond->getAuroraPoints();

        for ( const GenericValueMap<double>::MapPoint &point : points )
        {
            if ( point.value > 10 )
            {
                mapPoints << QString("{lat: %1, lng: %2, count: %3}").arg(point.latitude)
                                                                     .arg(point.longitude)
                                                                     .arg(point.value)
                          << QString("{lat: %1, lng: %2, count: %3}").arg(point.latitude)
                                                                     .arg(point.longitude - 360)
                                                                     .arg(point.value);
            }
        }
    }

    runJavaScript(QString(" auroraLayer.setData({max: 100, data:[%1]});").arg(mapPoints.join(",")));
}

void OnlineMapWidget::mufDataUpdate()
{
    FCT_IDENTIFICATION;

    QStringList mapPoints;

    if ( !prop_cond ) return;

    if ( prop_cond->isMufMapValid() )
    {
        const QList<GenericValueMap<double>::MapPoint> &points = prop_cond->getMUFPoints();

        for ( const GenericValueMap<double>::MapPoint &point : points )
        {
            mapPoints << QString("['%1', %2, %3]").arg(QString::number(point.value,'f',0))
                                                .arg(point.latitude)
                                                .arg(point.longitude)
                      << QString("['%1', %2, %3]").arg(QString::number(point.value,'f',0))
                                                .arg(point.latitude)
                                                .arg(point.longitude - 360);
        }
    }

    runJavaScript(QString(" drawMuf([%1]);").arg(mapPoints.join(",")));
}

void OnlineMapWidget::setIBPBand(VFOID , double, double ritFreq, double)
{
    FCT_IDENTIFICATION;

    runJavaScript(QString("currentBand=\"%1\";").arg(BandPlan::freq2Band(ritFreq).name));
}

void OnlineMapWidget::antPositionChanged(double in_azimuth, double in_elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth << " " << in_elevation;

    if ( ! isRotConnected )
        return;

    QString targetJavaScript;
    lastSeenAzimuth = in_azimuth;
    lastSeenElevation = in_elevation;

    /* Draw a new path */
    Gridsquare myGrid(StationProfilesManager::instance()->getCurProfile1().locator);

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
        targetJavaScript += QString("drawAntPath({lat: %1, lng: %2}, %3, %4, %5);").arg(myGrid.getLatitude())
                                                                                   .arg(myGrid.getLongitude())
                                                                                   .arg(beamLen)
                                                                                   .arg(in_azimuth)
                                                                                   .arg(azimuthBeamWidth);
    }
    else
    {
        // clean paths
        targetJavaScript = QLatin1String("drawAntPath({}, 0, 0, 0);");
    }

    runJavaScript(targetJavaScript);
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
    runJavaScript(QLatin1String("drawAntPath({}, 0, 0, 0);"));
}

void OnlineMapWidget::finishLoading(bool)
{
    FCT_IDENTIFICATION;

    isMainPageLoaded = true;

    /* which layers will be active */
    postponedScripts += webChannelHandler.generateMapMenuJS(true, true, true, true, true, true, true, true);
    main_page->runJavaScript(postponedScripts);
    postponedScripts = QString();

    webChannelHandler.restoreLayerControlStates(main_page);

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

void OnlineMapWidget::runJavaScript(const QString &js)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << js;

    if ( !isMainPageLoaded )
        postponedScripts.append(js);
    else
        main_page->runJavaScript(js);
}

void OnlineMapWidget::flyToMyQTH()
{
    FCT_IDENTIFICATION;

    /* focus current location */
    Gridsquare myGrid(StationProfilesManager::instance()->getCurProfile1().locator);

    if ( myGrid.isValid() )
    {
        QString currentProfilePosition(QString("[\"\", %1, %2, yellowIcon]").arg(myGrid.getLatitude())
                                                                            .arg(myGrid.getLongitude()));
        runJavaScript(QString("flyToPoint(%1, 4);").arg(currentProfilePosition));
    }
    // redraw ant path because QSO distance can change
    antPositionChanged(lastSeenAzimuth, lastSeenElevation);
}

void OnlineMapWidget::drawChatUsers(const QList<KSTUsersInfo> &list)
{
    FCT_IDENTIFICATION;

    QList<QString> chatUsers;

    for ( const KSTUsersInfo &user : list )
    {
        if ( user.grid.isValid() )
        {
            chatUsers.append(QString("[\"%1\", %2, %3, %4]").arg(user.callsign)
                                                            .arg(user.grid.getLatitude())
                                                            .arg(user.grid.getLongitude())
                                                            .arg("yellowIcon"));
        }
    }

    runJavaScript(QString("drawPointsGroup3([%1]);").arg(chatUsers.join(",")));
}

void OnlineMapWidget::drawWSJTXSpot(const WsjtxEntry &spot)
{
    FCT_IDENTIFICATION;

    Gridsquare spotGrid(spot.grid);

    if ( spotGrid.isValid() )
    {
        runJavaScript(QString("addWSJTXSpot(%1, %2, \"%3\", \"%4\");").arg(spotGrid.getLatitude())
                                                                      .arg(spotGrid.getLongitude())
                                                                      .arg(spot.callsign, Data::colorToHTMLColor(Data::statusToColor(spot.status, spot.dupeCount, QColor(Qt::white)))));
    }
}

void OnlineMapWidget::clearWSJTXSpots()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("clearWSJTXSpots();"));
}

OnlineMapWidget::~OnlineMapWidget()
{
    FCT_IDENTIFICATION;

    main_page->deleteLater();
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
