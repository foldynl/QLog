#ifndef QLOG_UI_ONLINEMAPWIDGET_H
#define QLOG_UI_ONLINEMAPWIDGET_H

#include <QWidget>
#include <QWebEngineView>
#include <QScopedPointer>
#include "ui/MapPageController.h"
#include "core/PropConditions.h"
#include "rig/Rig.h"
#include "service/kstchat/KSTChat.h"
#include "service/pskreporter/PSKReporter.h"
#include "data/HeardMeSpot.h"
#include "data/PskDecode.h"
#include "data/WsjtxEntry.h"

namespace Ui {
class OnlineMapWidget;
}

class OnlineMapWidget : public QWebEngineView
{
    Q_OBJECT

public:
    explicit OnlineMapWidget(QWidget* parent = nullptr);
    ~OnlineMapWidget();

    void assignPropConditions(PropConditions *);
    bool isHeardMeLayerVisible() const;

signals:
    void chatCallsignPressed(QString);
    void wsjtxCallsignPressed(QString);
    void heardMeLayerVisibilityChanged(bool visible);
    void antennaAzimuthRequested(double azimuth);

public slots:
    void setTarget(double lat, double lon);
    void changeTheme(int, bool isDark);
    void auroraDataUpdate();
    void mufDataUpdate();
    void setCurrentBand(const QString &band);
    void setIBPBand(VFOID, double, double, double);
    void setAntennaTarget(double azimuth);
    void antPositionChanged(double in_azimuth, double in_elevation);
    void rotConnected();
    void rotDisconnected();
    void flyToMyQTH();
    void drawChatUsers(const QList<KSTUsersInfo> &list);
    void drawWSJTXSpot(const WsjtxEntry &spot);
    void clearWSJTXSpots();
    void clearHeardMeSpots();
    void setHeardMeMode(const QString &mode);
    void addHeardMePoint(const PskDecode &spot,
                         PSKReporter::Direction direction);
    void addHeardMeSpot(const HeardMeSpot &spot);

protected slots:
    void finishLoading();
    void chatCallsignTrigger(const QString&);
    void wsjtxCallsignTrigger(const QString&);
    void IBPCallsignTrigger(const QString&, double);

private:

    QScopedPointer<MapPageController> mapController;
    PropConditions *prop_cond;
    double lastSeenAzimuth, lastSeenElevation;
    bool isRotConnected;
};

#endif // QLOG_UI_ONLINEMAPWIDGET_H
