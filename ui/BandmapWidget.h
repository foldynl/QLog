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
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
};

class BandmapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BandmapWidget(QWidget *parent = nullptr);
    ~BandmapWidget();

    enum BandmapZoom {
        ZOOM_100HZ,
        ZOOM_250HZ,
        ZOOM_500HZ,
        ZOOM_1KHZ,
        ZOOM_2K5HZ,
        ZOOM_5KHZ,
        ZOOM_10KHZ
    };

public slots:
    void update();
    void updateTunedFrequency(VFOID, double, double, double);
    void updateMode(VFOID, const QString &, const QString &mode,
                    const QString &subMode, qint32 width);
    void addSpot(DxSpot spot);
    void spotAgingChanged(int);
    void clearSpots();
    void zoomIn();
    void zoomOut();
    void spotsDxccStatusRecal(const QSqlRecord &record);

signals:
    void tuneDx(QString, double,  BandPlan::BandPlanMode);
    void nearestSpotFound(const DxSpot &);

private:
    void removeDuplicates(DxSpot &spot);
    void spotAging();
    void updateStations();
    void determineStepDigits(double &steps, int &digits) const;
    void clearAllCallsignFromScene();
    void clearFreqMark(QGraphicsPolygonItem **);
    void drawFreqMark(const double, const double, const QColor&, QGraphicsPolygonItem **);
    void drawTXRXMarks(double);
    void resizeEvent(QResizeEvent * event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void scrollToFreq(double freq);
    QPointF Freq2ScenePos(const double) const;
    double ScenePos2Freq(const QPointF &point) const;
    DxSpot nearestSpot(const double) const;
    void updateNearestSpot();
    void setBandmapAnimation(bool);
    void setBand(const Band &newBand, bool savePrevBandZoom = true);
    void saveCurrentZoom();
    BandmapWidget::BandmapZoom getSavedZoom(Band);
    void saveCurrentScrollFreq();
    double getSavedScrollFreq(Band);
    double visibleCentreFreq() const;

private slots:
    void centerRXActionChecked(bool);
    void spotClicked(const QString&, double, BandPlan::BandPlanMode);
    void showContextMenu(const QPoint&);
    void updateStationTimer();
    void focusZoomFreq(int, int);

private:
    Ui::BandmapWidget *ui;

    double rx_freq;
    double tx_freq;
    Band currentBand;
    BandmapZoom zoom;
    GraphicsScene* bandmapScene;
    QMap<double, DxSpot> spots;
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
    QSettings settings;

    struct LastTuneDx
    {
        QString callsign;
        double freq;
    };
    LastTuneDx lastTunedDX;
};

Q_DECLARE_METATYPE(BandmapWidget::BandmapZoom)

#endif // QLOG_UI_BANDMAPWIDGET_H
