#include "MapPageController.h"

#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QtMath>

#include "core/debug.h"
#include "core/IBPBeacon.h"
#include "core/LogParam.h"
#include "ui/WebEnginePage.h"

MODULE_IDENTIFICATION("qlog.ui.mappagecontroller");

QString MapPageController::jsonArray(const QJsonArray &array)
{
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QString MapPageController::jsonObject(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString MapPageController::jsonString(const QString &string)
{
    QJsonArray array;
    array.append(string);

    QString ret = jsonArray(array);
    ret.remove(0, 1);
    ret.chop(1);
    return ret;
}

QJsonObject MapPageController::coordinateObject(const MapCoordinate &coordinate)
{
    QJsonObject object;
    object.insert(QStringLiteral("lat"), coordinate.latitude);
    object.insert(QStringLiteral("lng"), coordinate.longitude);
    return object;
}

QJsonObject MapPageController::pointObject(const MapPoint &point)
{
    QJsonObject object;
    object.insert(QStringLiteral("label"), point.label);
    object.insert(QStringLiteral("lat"), point.coordinate.latitude);
    object.insert(QStringLiteral("lng"), point.coordinate.longitude);
    object.insert(QStringLiteral("icon"), point.icon);
    return object;
}

QJsonArray MapPageController::coordinateArray(const MapCoordinate &coordinate)
{
    QJsonArray array;
    array.append(coordinate.latitude);
    array.append(coordinate.longitude);
    return array;
}

QJsonArray MapPageController::pointsArray(const QList<MapPoint> &points)
{
    QJsonArray array;

    for ( const MapPoint &point : points )
        array.append(pointObject(point));

    return array;
}

QJsonArray MapPageController::coordinatesArray(const QList<MapCoordinate> &coordinates)
{
    QJsonArray array;

    for ( const MapCoordinate &coordinate : coordinates )
        array.append(coordinateArray(coordinate));

    return array;
}

QJsonArray MapPageController::pathsArray(const QList<MapPath> &paths)
{
    QJsonArray array;

    for ( const MapPath &path : paths )
    {
        QJsonObject item;
        item.insert(QStringLiteral("from"), coordinateObject(path.from));
        item.insert(QStringLiteral("to"), coordinateObject(path.to));
        array.append(item);
    }

    return array;
}

QJsonArray MapPageController::stringArray(const QStringList &strings)
{
    QJsonArray array;

    for ( const QString &string : strings )
        array.append(string);

    return array;
}

QJsonArray MapPageController::heatPointsArray(const QList<MapHeatPoint> &points)
{
    QJsonArray array;

    for ( const MapHeatPoint &point : points )
    {
        QJsonObject item;
        item.insert(QStringLiteral("lat"), point.coordinate.latitude);
        item.insert(QStringLiteral("lng"), point.coordinate.longitude);
        item.insert(QStringLiteral("count"), point.value);
        array.append(item);
    }

    return array;
}

QJsonArray MapPageController::ibpBandsArray()
{
    QJsonArray array;

    for ( const IBPBeacon::Band &band : IBPBeacon::bands() )
    {
        QJsonObject item;
        item.insert(QStringLiteral("name"), band.name);
        item.insert(QStringLiteral("frequency"), band.frequency);
        array.append(item);
    }

    return array;
}

QJsonArray MapPageController::ibpBeaconsArray()
{
    QJsonArray array;

    for ( const IBPBeacon::Station &beacon : IBPBeacon::beacons() )
    {
        QJsonObject item;
        item.insert(QStringLiteral("callsign"), beacon.callsign);
        item.insert(QStringLiteral("lat"), beacon.latitude);
        item.insert(QStringLiteral("lon"), beacon.longitude);
        item.insert(QStringLiteral("active"), beacon.active);
        array.append(item);
    }

    return array;
}

MapPageController::MapPageController(const QString &configID,
                                     QObject *parent)
    : QObject(parent),
      configID(configID),
      mainPage(new WebEnginePage(this)),
      pageLoaded(false)
{
    channel.registerObject("mapBridge", this);
}

MapPageController::~MapPageController()
{
    if ( attachedView && attachedView->page() == mainPage )
        attachedView->setPage(nullptr);

    if ( mainPage )
        mainPage->setWebChannel(nullptr);

    channel.deregisterObject(this);
}

void MapPageController::attach(QWebEngineView *view,
                               MapLayer::Layers layers)
{
    FCT_IDENTIFICATION;

    if ( !view )
        return;

    if ( attachedView )
    {
        disconnect(attachedView.data(), &QWebEngineView::loadFinished,
                   this, &MapPageController::finishLoading);
        if ( attachedView->page() == mainPage )
            attachedView->setPage(nullptr);
    }

    attachedView = view;
    mapLayers = layers;
    mainPage->setWebChannel(&channel);
    view->setPage(mainPage);
    connect(view, &QWebEngineView::loadFinished,
            this, &MapPageController::finishLoading,
            Qt::UniqueConnection);
    QUrl mapUrl(QStringLiteral("qrc:/res/map/onlinemap.html"));
    QUrlQuery mapQuery;
    mapQuery.addQueryItem(QStringLiteral("language"),
                          QCoreApplication::instance()->property("qlogLanguage").toString());
    mapUrl.setQuery(mapQuery);
    mainPage->load(mapUrl);
    view->setFocusPolicy(Qt::ClickFocus);
}

void MapPageController::runJavaScript(const QString &js)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << js;

    if ( !pageLoaded )
        postponedScripts.append(js);
    else
        mainPage->runJavaScript(js);
}

void MapPageController::setDarkTheme(bool isDark)
{
    FCT_IDENTIFICATION;

    const QString js = isDark
                           ? QLatin1String("if (typeof setMapDarkMode === \"function\") setMapDarkMode(true);")
                           : QLatin1String("if (typeof setMapDarkMode === \"function\") setMapDarkMode(false);");
    runJavaScript(js);
}

void MapPageController::setStaticMapTime(const QDateTime &dateTime)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("setStaticMapTime(new Date(%1));")
                  .arg(dateTime.toMSecsSinceEpoch()));
}

void MapPageController::drawPoints(const QList<MapPoint> &points)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawPoints(%1);")
                  .arg(jsonArray(pointsArray(points))));
}

void MapPageController::drawPointsBusy(const QList<MapPoint> &points,
                                       const QString &text)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawPointsBusy(%1, %2);")
                  .arg(jsonArray(pointsArray(points)),
                       jsonString(text)));
}

void MapPageController::drawPointsAndShortPathsBusy(const QList<MapPoint> &points,
                                                    const QList<MapPath> &paths,
                                                    const QString &text)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawPointsAndShortPathsBusy(%1, %2, %3);")
                  .arg(jsonArray(pointsArray(points)),
                       jsonArray(pathsArray(paths)),
                       jsonString(text)));
}

void MapPageController::drawHomePoints(const QList<MapPoint> &points)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawPointsGroup2(%1);")
                  .arg(jsonArray(pointsArray(points))));
}

void MapPageController::drawChatPoints(const QList<MapPoint> &points)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawPointsGroup3(%1);")
                  .arg(jsonArray(pointsArray(points))));
}

void MapPageController::flyToPoint(const MapPoint &point, int zoom)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("flyToPoint(%1, %2);")
                  .arg(jsonObject(pointObject(point)))
                  .arg(zoom));
}

void MapPageController::drawPath(const QList<MapCoordinate> &points)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawPath(%1);")
                  .arg(jsonArray(coordinatesArray(points))));
}

void MapPageController::clearPath()
{
    FCT_IDENTIFICATION;

    drawPath(QList<MapCoordinate>());
}

void MapPageController::drawShortPaths(const QList<MapPath> &paths)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawShortPaths(%1);")
                  .arg(jsonArray(pathsArray(paths))));
}

void MapPageController::drawShortPathsBusy(const QList<MapPath> &paths,
                                           const QString &text)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawShortPathsBusy(%1, %2);")
                  .arg(jsonArray(pathsArray(paths)),
                       jsonString(text)));
}

void MapPageController::drawAntPath(const MapCoordinate &from,
                                    double azimuth,
                                    double antAngle)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawAntPath(%1, %2, %3);")
                  .arg(jsonObject(coordinateObject(from)))
                  .arg(azimuth)
                  .arg(antAngle));
}

void MapPageController::clearAntPath()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("drawAntPath({});"));
}

void MapPageController::setAntennaTarget(double azimuth)
{
    FCT_IDENTIFICATION;

    if ( qIsFinite(azimuth) )
        runJavaScript(QStringLiteral("setAntennaTarget(%1);")
                      .arg(azimuth, 0, 'g', 16));
}

void MapPageController::setGridLayers(const QStringList &confirmedGrids,
                                      const QStringList &workedGrids)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("grids_confirmed = %1;"
                                 "grids_worked = %2;")
                  .arg(jsonArray(stringArray(confirmedGrids)),
                       jsonArray(stringArray(workedGrids))));
}

void MapPageController::clearGridLayers()
{
    FCT_IDENTIFICATION;

    setGridLayers(QStringList(), QStringList());
}

void MapPageController::redrawGridLayer()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("maidenheadConfWorked.redraw();"));
}

void MapPageController::invalidateSize()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("if (typeof map !== 'undefined') "
                                "setTimeout(function() { "
                                "map.invalidateSize({pan: false, animate: false}); "
                                "}, 0);"));
}

void MapPageController::panToBoundsLongitudeCenter(const QList<MapCoordinate> &coordinates)
{
    FCT_IDENTIFICATION;

    if ( coordinates.isEmpty() )
        return;

    runJavaScript(QStringLiteral("map.panTo([0, L.latLngBounds(%1).getCenter().lng]);")
                  .arg(jsonArray(coordinatesArray(coordinates))));
}

void MapPageController::setAuroraData(const QList<MapHeatPoint> &points)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("auroraLayer.setData({max: 100, data:%1});")
                  .arg(jsonArray(heatPointsArray(points))));
}

void MapPageController::drawMuf(const QList<MapPoint> &points)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawMuf(%1);")
                  .arg(jsonArray(pointsArray(points))));
}

void MapPageController::setCurrentBand(const QString &band)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("setCurrentBand(%1);")
                  .arg(jsonString(band)));
}

void MapPageController::setHeardMeMode(const QString &mode)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("setHeardMeMode(%1);")
                  .arg(jsonString(mode)));
}

void MapPageController::addWsjtxSpot(const MapPoint &point,
                                     const QString &color,
                                     const QString &textColor,
                                     bool halo)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("addWSJTXSpot(%1, %2, %3, %4);")
                  .arg(jsonObject(pointObject(point)))
                  .arg(jsonString(color))
                  .arg(jsonString(textColor))
                  .arg(halo ? QLatin1String("true") : QLatin1String("false")));
}

void MapPageController::clearWsjtxSpots()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("clearWSJTXSpots();"));
}

void MapPageController::clearHeardMeSpots()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("clearHeardMeSpots();"));
}

void MapPageController::addHeardMePoint(const MapPoint &point,
                                        qint32 report,
                                        const QString &band,
                                        const QString &displayGroup,
                                        const QString &color,
                                        double opacity,
                                        bool halo)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("addHeardMePoint(%1, %2, %3, %4, %5, %6, %7);")
                  .arg(jsonObject(pointObject(point)))
                  .arg(report)
                  .arg(jsonString(band))
                  .arg(jsonString(displayGroup))
                  .arg(jsonString(color))
                  .arg(opacity)
                  .arg(halo ? QLatin1String("true") : QLatin1String("false")));
}

QString MapPageController::generateIbpDataJS()
{
    FCT_IDENTIFICATION;

    return QStringLiteral("configureIbpData(%1, %2);")
           .arg(jsonArray(ibpBandsArray()),
                jsonArray(ibpBeaconsArray()));
}

QString MapPageController::generateLayerControlJS(MapLayer::Layers layers)
{
    FCT_IDENTIFICATION;
    QJsonArray options;

    auto appendOption = [&options, layers](MapLayer::Layer layer,
                                           const QString &label,
                                           const QString &key)
    {
        if ( !layers.testFlag(layer) )
            return;

        QJsonObject option;
        option.insert(QStringLiteral("label"), label);
        option.insert(QStringLiteral("key"), key);
        options.append(option);
    };

    appendOption(MapLayer::Aurora, tr("Aurora"), QStringLiteral("auroraLayer"));

    appendOption(MapLayer::Beam, tr("Beam"), QStringLiteral("antPathLayer"));

    appendOption(MapLayer::Chat, tr("Chat"), QStringLiteral("chatStationsLayer"));

    appendOption(MapLayer::Grid, tr("Grid"), QStringLiteral("maidenheadConfWorked"));

    appendOption(MapLayer::Grayline, tr("Gray-Line"), QStringLiteral("grayline"));

    appendOption(MapLayer::HeardMe, tr("Heard Me"), QStringLiteral("heardMeLayer"));

    appendOption(MapLayer::Ibp, tr("IBP"), QStringLiteral("IBPLayer"));

    appendOption(MapLayer::Muf, tr("MUF"), QStringLiteral("mufLayer"));

    appendOption(MapLayer::Wsjtx, tr("WSJTX - CQ"), QStringLiteral("wsjtxStationsLayer"));

    appendOption(MapLayer::Path, tr("Path"), QStringLiteral("pathLayer"));

    QString ret = QStringLiteral("configureLayerControl(%1);")
                  .arg(jsonArray(options));

    qCDebug(runtime) << ret;

    return ret;
}

void MapPageController::restoreLayerControlStates()
{
    FCT_IDENTIFICATION;

    QJsonArray layerStates;

    const QStringList &keys = LogParam::getMapLayerStates(configID);

    for ( const QString &key : keys )
    {
        qCDebug(runtime) << "key:" << key << "value:" << LogParam::getMapLayerState(configID, key);

        QJsonObject layerState;
        layerState.insert(QStringLiteral("key"), key);
        layerState.insert(QStringLiteral("visible"), LogParam::getMapLayerState(configID, key));
        layerStates.append(layerState);
    }

    const QString js = QStringLiteral("restoreQLogLayerStates(%1);")
                       .arg(jsonArray(layerStates));
    qCDebug(runtime) << js;

    mainPage->runJavaScript(js);

    connectWebChannel();
}

void MapPageController::connectWebChannel()
{
    FCT_IDENTIFICATION;

    QFile file(":/qtwebchannel/qwebchannel.js");

    if ( !file.open(QIODevice::ReadOnly) )
    {
        qCInfo(runtime) << "Cannot read qwebchannel.js";
        return;
    }

    QTextStream stream(&file);
    QString js;

    js.append(stream.readAll());
    js += " var webChannel = new QWebChannel(qt.webChannelTransport, function(channel) "
          "{ "
          "  window.mapBridge = channel.objects.mapBridge; "
          "  if (window.connectQtMapBridge) "
          "    window.connectQtMapBridge(window.mapBridge); "
          "});";
    mainPage->runJavaScript(js);
}

void MapPageController::handleLayerSelectionChanged(const QVariant &data, const QVariant &state)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data << state;

    const QString key = data.toString();
    const bool visible = state.toString().compare(QLatin1String("on"), Qt::CaseInsensitive) == 0;
    LogParam::setMapLayerState(configID, key, visible);
    emit layerVisibilityChanged(key, visible);
}

void MapPageController::chatCallsignClicked(const QVariant &data)
{
    FCT_IDENTIFICATION;

    emit chatCallsignPressed(data.toString());
}

void MapPageController::wsjtxCallsignClicked(const QVariant &data)
{
    FCT_IDENTIFICATION;

    emit wsjtxCallsignPressed(data.toString());
}

void MapPageController::IBPCallsignClicked(const QVariant &callsign, const QVariant &freq)
{
    FCT_IDENTIFICATION;

    emit IBPPressed(callsign.toString(), freq.toDouble());
}

void MapPageController::requestAntennaAzimuth(double azimuth)
{
    FCT_IDENTIFICATION;

    if ( qIsFinite(azimuth) && azimuth >= 0.0 && azimuth < 360.0 )
        emit antennaAzimuthRequested(azimuth);
}

void MapPageController::finishLoading(bool ok)
{
    FCT_IDENTIFICATION;

    if ( pageLoaded || !ok )
        return;

    pageLoaded = true;
    postponedScripts.append(generateIbpDataJS());
    postponedScripts.append(generateLayerControlJS(mapLayers));
    postponedScripts.append(QStringLiteral("configureAntennaContextMenu(%1, %2, %3, %4);")
                            .arg(jsonString(tr("Target Antenna Here")),
                                 jsonString(tr("QSO Short Path")),
                                 jsonString(tr("QSO Long Path")),
                                 jsonString(tr("Stop Antenna"))));
    mainPage->runJavaScript(postponedScripts.join(QLatin1Char('\n')));
    postponedScripts.clear();

    restoreLayerControlStates();
    emit loaded();
}
