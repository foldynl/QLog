#include "MapPageController.h"

#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUrl>
#include <QWebEngineView>

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
    mainPage->load(QUrl(QStringLiteral("qrc:/res/map/onlinemap.html")));
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
                           ? QLatin1String("map.getPanes().tilePane.style.webkitFilter=\"brightness(0.6) invert(1) contrast(3) hue-rotate(200deg) saturate(0.3) brightness(0.9)\";")
                           : QLatin1String("map.getPanes().tilePane.style.webkitFilter=\"\";");
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
                                    double distance,
                                    double azimuth,
                                    double antAngle)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("drawAntPath(%1, %2, %3, %4);")
                  .arg(jsonObject(coordinateObject(from)))
                  .arg(distance)
                  .arg(azimuth)
                  .arg(antAngle));
}

void MapPageController::clearAntPath()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("drawAntPath({}, 0, 0, 0);"));
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

    runJavaScript(QStringLiteral("setIbpCurrentBand(%1);")
                  .arg(jsonString(band)));
}

void MapPageController::addWsjtxSpot(const MapPoint &point,
                                     const QString &color,
                                     const QString &textColor)
{
    FCT_IDENTIFICATION;

    runJavaScript(QStringLiteral("addWSJTXSpot(%1, %2, %3);")
                  .arg(jsonObject(pointObject(point)),
                       jsonString(color),
                       jsonString(textColor)));
}

void MapPageController::clearWsjtxSpots()
{
    FCT_IDENTIFICATION;

    runJavaScript(QLatin1String("clearWSJTXSpots();"));
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

    LogParam::setMapLayerState(configID, data.toString(),
                               (state.toString().toLower() == "on") ? true : false);
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

void MapPageController::finishLoading(bool ok)
{
    FCT_IDENTIFICATION;

    if ( pageLoaded || !ok )
        return;

    pageLoaded = true;
    postponedScripts.append(generateIbpDataJS());
    postponedScripts.append(generateLayerControlJS(mapLayers));
    mainPage->runJavaScript(postponedScripts.join(QLatin1Char('\n')));
    postponedScripts.clear();

    restoreLayerControlStates();
    emit loaded();
}
