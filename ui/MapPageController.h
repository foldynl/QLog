#ifndef QLOG_UI_MAPPAGECONTROLLER_H
#define QLOG_UI_MAPPAGECONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWebChannel>

#include "ui/MapLayer.h"

class WebEnginePage;
class QJsonArray;
class QJsonObject;
class QWebEngineView;

struct MapCoordinate
{
    MapCoordinate() = default;
    MapCoordinate(double inLatitude, double inLongitude)
        : latitude(inLatitude),
          longitude(inLongitude)
    {
    }

    double latitude = 0.0;
    double longitude = 0.0;
};

struct MapPoint
{
    MapPoint() = default;
    MapPoint(const QString &inLabel,
             double inLatitude,
             double inLongitude,
             const QString &inIcon = QString())
        : label(inLabel),
          coordinate(inLatitude, inLongitude),
          icon(inIcon)
    {
    }

    QString label;
    MapCoordinate coordinate;
    QString icon;
};

struct MapPath
{
    MapPath() = default;
    MapPath(const MapCoordinate &inFrom,
            const MapCoordinate &inTo)
        : from(inFrom),
          to(inTo)
    {
    }

    MapCoordinate from;
    MapCoordinate to;
};

struct MapHeatPoint
{
    MapHeatPoint() = default;
    MapHeatPoint(double inLatitude,
                 double inLongitude,
                 double inValue)
        : coordinate(inLatitude, inLongitude),
          value(inValue)
    {
    }

    MapCoordinate coordinate;
    double value = 0.0;
};

class MapPageController : public QObject
{
    Q_OBJECT

public:
    explicit MapPageController(const QString &configID,
                               QObject *parent = nullptr);
    ~MapPageController() override;

    void attach(QWebEngineView *view,
                MapLayer::Layers layers);

    void setDarkTheme(bool isDark);
    void setStaticMapTime(const QDateTime &dateTime);

    void drawPoints(const QList<MapPoint> &points);
    void drawPointsBusy(const QList<MapPoint> &points,
                        const QString &text);
    void drawPointsAndShortPathsBusy(const QList<MapPoint> &points,
                                     const QList<MapPath> &paths,
                                     const QString &text);
    void drawHomePoints(const QList<MapPoint> &points);
    void drawChatPoints(const QList<MapPoint> &points);
    void flyToPoint(const MapPoint &point, int zoom);

    void drawPath(const QList<MapCoordinate> &points);
    void clearPath();
    void drawShortPaths(const QList<MapPath> &paths);
    void drawShortPathsBusy(const QList<MapPath> &paths,
                            const QString &text);
    void drawAntPath(const MapCoordinate &from,
                     double distance,
                     double azimuth,
                     double antAngle);
    void clearAntPath();

    void setGridLayers(const QStringList &confirmedGrids,
                       const QStringList &workedGrids);
    void clearGridLayers();
    void redrawGridLayer();
    void invalidateSize();
    void panToBoundsLongitudeCenter(const QList<MapCoordinate> &coordinates);

    void setAuroraData(const QList<MapHeatPoint> &points);
    void drawMuf(const QList<MapPoint> &points);
    void setCurrentBand(const QString &band);

    void addWsjtxSpot(const MapPoint &point,
                      const QString &color,
                      const QString &textColor);
    void clearWsjtxSpots();

signals:
    void loaded();
    void chatCallsignPressed(QString callsign);
    void wsjtxCallsignPressed(QString callsign);
    void IBPPressed(QString callsign, double frequency);

public slots:
    void handleLayerSelectionChanged(const QVariant &data,
                                     const QVariant &state);
    void chatCallsignClicked(const QVariant &data);
    void wsjtxCallsignClicked(const QVariant &data);
    void IBPCallsignClicked(const QVariant &callsign,
                            const QVariant &freq);

private:
    static QString jsonArray(const QJsonArray &array);
    static QString jsonObject(const QJsonObject &object);
    static QString jsonString(const QString &string);
    static QJsonObject pointObject(const MapPoint &point);
    static QJsonObject coordinateObject(const MapCoordinate &coordinate);
    static QJsonArray coordinateArray(const MapCoordinate &coordinate);
    static QJsonArray pointsArray(const QList<MapPoint> &points);
    static QJsonArray coordinatesArray(const QList<MapCoordinate> &coordinates);
    static QJsonArray pathsArray(const QList<MapPath> &paths);
    static QJsonArray stringArray(const QStringList &strings);
    static QJsonArray heatPointsArray(const QList<MapHeatPoint> &points);
    static QJsonArray ibpBandsArray();
    static QJsonArray ibpBeaconsArray();

    QString generateIbpDataJS();
    QString generateLayerControlJS(MapLayer::Layers layers);
    void restoreLayerControlStates();
    void connectWebChannel();
    void finishLoading(bool ok);
    void runJavaScript(const QString &js);

    QString configID;
    WebEnginePage *mainPage;
    QPointer<QWebEngineView> attachedView;
    bool pageLoaded;
    QStringList postponedScripts;
    QWebChannel channel;
    MapLayer::Layers mapLayers;
};

#endif // QLOG_UI_MAPPAGECONTROLLER_H
