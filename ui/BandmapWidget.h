#ifndef QLOG_UI_BANDMAPWIDGET_H
#define QLOG_UI_BANDMAPWIDGET_H

#include <QWidget>
#include <QMap>
#include <QTimer>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMutex>
#include <QColor>
#include <QSqlRecord>

#include "data/DxSpot.h"
#include "data/Band.h"
#include "rig/Rig.h"
#include "core/LogLocale.h"
#include "component/ShutdownAwareWidget.h"

namespace Ui {
class BandmapWidget;
}

class QGraphicsScene;

class GraphicsScene : public QGraphicsScene
{
    Q_OBJECT;

public:
    explicit GraphicsScene(QObject *parent = nullptr) : QGraphicsScene(parent){};

signals:
    void spotClicked(QString, double, BandPlan::BandPlanMode mode);

protected:
    void mousePressEvent (QGraphicsSceneMouseEvent *evt) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt) override;
};

class BandmapWidget : public QWidget, public ShutdownAwareWidget
{
    Q_OBJECT

public:
    explicit BandmapWidget(const QString &widgetID = QString(),
                           const Band &widgetBand = Band(),
                           QWidget *parent = nullptr);
    ~BandmapWidget();
    const Band& getBand() const {return currentBand;};
    const QList<BandmapWidget *> getNonVfoWidgetList() {return nonVfoWidgets;};
    enum BandmapZoom {
        ZOOM_100HZ = 6,
        ZOOM_250HZ = 5,
        ZOOM_500HZ = 4,
        ZOOM_1KHZ = 3,
        ZOOM_2K5HZ = 2,
        ZOOM_5KHZ = 1,
        ZOOM_10KHZ = 0
    };

public slots:
    void update();
    void updateTunedFrequency(VFOID, double, double, double);
    void updateMode(VFOID, const QString &, const QString &mode,
                    const QString &subMode, qint32 width);
    void addSpot(DxSpot spot);
    void spotAgingChanged(int);
    void clearSpots();
    void setZoom(int);
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsStatusWhenQSOUpdated(const QSqlRecord &);
    void updateSpotsDupeWhenQSODeleted(const QSqlRecord &record);
    void updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities);
    void recalculateDxccStatus();
    void resetDupe();
    void recalculateDupe();
    void updateStations();
    void clearWidgetBand();
    virtual void finalizeBeforeAppExit() override;
    void increasePendingSpots() {pendingSpots++;};

signals:
    void tuneDx(DxSpot);
    void nearestSpotFound(const DxSpot &);
    void spotsUpdated();
    void requestNewNonVfoBandmapWindow(const QString &id, const QString &bandName);

private:
    void removeDuplicates(DxSpot &spot);
    void spotAging();

    void determineStepDigits(double &step, int &digits) const;
    void clearAllCallsignFromScene();
    void clearFreqMark(QGraphicsPolygonItem **);
    void drawFreqMark(const double, const double, const QColor&, QGraphicsPolygonItem **);
    void drawTXRXMarks(double);
    void drawMarkers(double frequency);
    void resizeEvent(QResizeEvent * event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void scrollToFreq(double freq);
    QPointF Freq2ScenePos(const double) const;
    double ScenePos2Freq(const QPointF &point) const;
    DxSpot nearestSpot(const double) const;
    void updateNearestSpot(bool force = false);
    void setBandmapAnimation(bool);
    void setBand(const Band &newBand, bool savePrevBandZoom = true);
    void saveCurrentZoom();
    BandmapWidget::BandmapZoom getSavedZoom(const Band &);
    void saveCurrentScrollFreq();
    double getSavedScrollFreq(const Band &);
    double visibleCentreFreq() const;
    bool isAlreadyOpened(const Band &band) const;
    void saveState();

private slots:
    void centerRXActionChecked(bool);
    void spotClicked(const QString&, double, BandPlan::BandPlanMode);
    void showContextMenu(const QPoint&);
    void updateStationTimer();
    void focusZoomFreq(int, int);
    void clickNewBandmapWindow();

private:
    Ui::BandmapWidget *ui;

    double rx_freq;
    double tx_freq;
    Band currentBand;
    BandmapZoom zoom;
    GraphicsScene* bandmapScene;
    static QMap<double, DxSpot> spots;
    static QList<BandmapWidget *> nonVfoWidgets;
    static BandmapWidget* vfoWidget;
    static double lastSeenVFOFreq;
    QTimer *update_timer;
    QList<QGraphicsLineItem *> lineItemList;
    QList<QGraphicsTextItem *> textItemList;
    QGraphicsPolygonItem* rxMark;
    QGraphicsPolygonItem* txMark;
    bool keepRXCenter;
    LogLocale locale;
    quint32 pendingSpots;
    qint64 lastStationUpdate;
    double zoomFreq;
    int zoomWidgetYOffset;
    bool bandmapAnimation;
    QString currBandMode;
    bool isNonVfo;
    bool isActive;
    struct LastTuneDx
    {
        QString callsign;
        double freq;
    };
    LastTuneDx lastTunedDX;
    DxSpot lastNearestSpot;

    double minHeight;
    const QString MAIN_WIDGET_OBJECT_NAME = "bandmapWidget";

    //Pixel between each step in BandMap
    const int PIXELSPERSTEP = 10;

    //Maximal Aging interval is 20s
    const int BANDMAP_AGING_CHECK_TIME = 20000;

    //Maximal refresh rate for bandmap is 1s
    const int BANDMAP_MAX_REFRESH_TIME = 1000;
};

Q_DECLARE_METATYPE(BandmapWidget::BandmapZoom)

#endif // QLOG_UI_BANDMAPWIDGET_H
