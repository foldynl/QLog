#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMutableMapIterator>
#include <QMenu>
#include <QTextDocument>
#include <QScrollBar>
#include <QGraphicsSceneMouseEvent>
#include <algorithm>
#include <QWheelEvent>

#include "BandmapWidget.h"
#include "ui_BandmapWidget.h"
#include "data/Data.h"
#include "data/BandPlan.h"
#include "core/debug.h"
#include "rig/macros.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.bandmapwidget");

#define WIDGET_CENTER ( height()/2 - 50 )

QMap<double, DxSpot> BandmapWidget::spots;
QList<BandmapWidget *> BandmapWidget::nonVfoWidgets;
double BandmapWidget::lastSeenVFOFreq = 0.0;
BandmapWidget *BandmapWidget::vfoWidget = nullptr;

BandmapWidget::BandmapWidget(const QString &widgetID,
                             const Band &widgetBand,
                             QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BandmapWidget),
    rx_freq(0.0),
    tx_freq(0.0),
    bandmapScene(new GraphicsScene(this)),
    update_timer(new QTimer(this)),
    rxMark(nullptr),
    txMark(nullptr),
    keepRXCenter(true),
    pendingSpots(0),
    lastStationUpdate(0),
    bandmapAnimation(true),
    isNonVfo(!widgetID.isEmpty()),
    isActive(false)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    setObjectName((isNonVfo) ? widgetID : MAIN_WIDGET_OBJECT_NAME);

    double newContactFreq = (lastSeenVFOFreq == 0.0 ) ? LogParam::getNewContactFreq()
                                                      : lastSeenVFOFreq;
    double ritFreq = newContactFreq + RigProfilesManager::instance()->getCurProfile1().ritOffset;
    double xitFreq = newContactFreq + RigProfilesManager::instance()->getCurProfile1().xitOffset;
    const QString &mode = LogParam::getNewContactMode();
    const QString &submode = LogParam::getNewContactSubMode();

    keepRXCenter = LogParam::getBandmapCenterRX(objectName());

    if ( isNonVfo )
    {
        ui->bottomRow->setVisible(false);
        ui->clearAllButton->setVisible(false);
        nonVfoWidgets.append(this);
    }
    else
    {
        vfoWidget = this;
        ui->clearSpotOlderSpin->setValue(LogParam::getBandmapAging(objectName()));
    }

    setBand((widgetBand == Band()) ? BandPlan::freq2Band(ritFreq)
                                   : widgetBand,
            false);

    bandmapScene->setFocusOnTouch(false);

    connect(bandmapScene, &GraphicsScene::spotClicked,
            this, &BandmapWidget::spotClicked);
    connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &BandmapWidget::focusZoomFreq);

    ui->graphicsView->setScene(bandmapScene);
    ui->graphicsView->installEventFilter(this);
    //ui->scrollArea->verticalScrollBar()->setSingleStep(5);

    connect(update_timer, &QTimer::timeout, this, &BandmapWidget::updateStationTimer);
    update_timer->start(BANDMAP_MAX_REFRESH_TIME);

    updateMode(VFO1, QString(), mode, submode, 0);
    updateTunedFrequency(VFO1, newContactFreq, ritFreq, xitFreq);

    ui->zoomSlider->setSliderPosition(zoom);
}

void BandmapWidget::update()
{
    FCT_IDENTIFICATION;

    /****************
     * Restart Time *
     ****************/
    update_timer->setInterval(BANDMAP_MAX_REFRESH_TIME);

    /*************
     * Clear All *
     *************/
    clearAllCallsignFromScene();

    clearFreqMark(&rxMark);
    clearFreqMark(&txMark);

    bandmapScene->clear();

    // do not show bandmap for submm bands
    if ( rx_freq > 250000.0 || currentBand.start >= 300000.0 ) return;

    /*******************
     * Determine Scale *
     *******************/
    double step;
    int digits;

    determineStepDigits(step, digits);

    const int steps = static_cast<int>(round((currentBand.end - currentBand.start) / step));

    minHeight = steps * PIXELSPERSTEP + 30;
    ui->graphicsView->setFixedSize(270, minHeight);

    /****************/
    /* Draw bandmap */
    /****************/
    const QPen gridPen(QColor(192, 192, 192));
    const QBrush highlightBrush(QColor(102, 153, 255, 100));

    for ( int i = 0; i <= steps; i++ )
    {
        double plottedFreq = currentBand.start + step * i;
        const double y = i * PIXELSPERSTEP;

        // Add colored square
        if ( !currBandMode.isEmpty()
             && i < steps
             && currBandMode == BandPlan::freq2BandModeGroupString(plottedFreq) )
            bandmapScene->addRect(0, y, 10, 10, QPen(Qt::NoPen), highlightBrush);

        const int lineLength = (i % 5 == 0) ? 15 : 10;

        bandmapScene->addLine(0, y, lineLength, y, gridPen);

        if ( i % 5 == 0 )
        {
            QGraphicsTextItem* text = bandmapScene->addText(QString::number(plottedFreq, 'f', digits));
            const QRectF rect = text->boundingRect();
            text->setPos(-rect.width() - 5, y - (rect.height() / 2));
        }
    }

    const QString &endFreqDigits= QString::number(currentBand.end + step*steps, 'f', digits);
    bandmapScene->setSceneRect(135 - (endFreqDigits.size() * PIXELSPERSTEP),
                               0,
                               0,
                               steps * PIXELSPERSTEP + 10);

    /************************/
    /* Draw TX and RX Marks */
    /************************/
    drawTXRXMarks(step);

    /*****************
     * Draw Stations *
     *****************/
    updateStations();
}

void BandmapWidget::spotAging()
{
    FCT_IDENTIFICATION;

    // only master Widget removes spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    int clear_interval_sec = ui->clearSpotOlderSpin->value() * 60;

    qCDebug(function_parameters)<<clear_interval_sec;

    if ( clear_interval_sec <= 0 ) return;

    QMutableMapIterator<double, DxSpot> spotIterator(spots);

    while ( spotIterator.hasNext() )
    {
        spotIterator.next();
        //clear spots automatically
        if ( spotIterator.value().dateTime.addSecs(clear_interval_sec) <= QDateTime::currentDateTimeUtc() )
        {
            spotIterator.remove();
        }
    }
}

void BandmapWidget::updateStations()
{
    FCT_IDENTIFICATION;

    /****************
     * Restart Time *
     ****************/
    update_timer->setInterval(BANDMAP_MAX_REFRESH_TIME);

    clearAllCallsignFromScene();

    spotAging();

    // do not show bandmap for submm bands
    if ( rx_freq > 250000.0 || currentBand.start >= 300000.0 )return;

    double step;
    int digits;
    double min_y = 0;
    const QColor lineColor(192,192,192);
    const QColor defaultTextColor = qApp->palette().color(QPalette::Text);
    const QString timeFormat = locale.formatTimeShort();

    determineStepDigits(step, digits);

    QMap<double, DxSpot>::iterator lower = spots.lowerBound(currentBand.start);
    QMap<double, DxSpot>::iterator upper = spots.upperBound(currentBand.end);

    while ( lower != upper )
    {
        DxSpot &spot = lower.value();

        double freq_y = ((lower.key() - currentBand.start) / step) * PIXELSPERSTEP;
        double text_y = std::max(min_y + 5.0, freq_y);

        /*************************
         * Draw Line to Callsign *
         *************************/
        lineItemList.append(bandmapScene->addLine(17, freq_y, 40, text_y, QPen(lineColor)));

        const QString &callsignTmp = spot.callsign;
        const QString &timeTmp = locale.toString(spot.dateTime, timeFormat);

        QGraphicsTextItem* text = bandmapScene->addText(callsignTmp + " @ " + timeTmp);
        text->document()->setDocumentMargin(0);

        qreal halfHeight = text->boundingRect().height() / 2;
        text->setPos(40, text_y - halfHeight);
        text->setFlags(QGraphicsItem::ItemIsFocusable |
                       QGraphicsItem::ItemIsSelectable |
                       text->flags());
        text->setProperty("freq", lower.key());
        text->setProperty("bandmode", static_cast<int>(spot.bandPlanMode));

        QString unit;
        unsigned char decP;
        double spotFreq = Data::MHz2UserFriendlyFreq(lower.key(), unit, decP);
        text->setToolTip(QString("<b>%1</b> de %2<br/>%3 %4; %5<br/>%6").arg(callsignTmp,
                                                             spot.spotter,
                                                             QString::number(spotFreq, 'f', decP),
                                                             unit,
                                                             spot.modeGroupString,
                                                             spot.comment));

        min_y = text_y + halfHeight;

        text->setDefaultTextColor(Data::statusToColor(spot.status,
                                                      spot.dupeCount,
                                                      defaultTextColor));

        textItemList.append(text);
        ++lower;
    }

    // Resize scene and view dynamically
    const QRectF &itemsRect = bandmapScene->itemsBoundingRect();
    QRectF sceneRect = bandmapScene->sceneRect();

    double resultHeight = qMax(itemsRect.bottom(), minHeight);
    sceneRect.setBottom(resultHeight + 10);
    ui->graphicsView->setFixedSize(270, resultHeight + 30);
    bandmapScene->setSceneRect(sceneRect);

    pendingSpots = 0;
    lastStationUpdate = QDateTime::currentMSecsSinceEpoch();
    if ( !isNonVfo )
    {
        // signal to the non-vfo bandmaps that the spots should be redrawn
        emit spotsUpdated();
    }
}

void BandmapWidget::clearWidgetBand()
{
    FCT_IDENTIFICATION;

    auto begin = spots.lowerBound(currentBand.start);
    auto end = spots.upperBound(currentBand.end);

    while (begin != end)
        begin = spots.erase(begin);

    if ( vfoWidget )
        vfoWidget->updateStations(); // this causes that all bandmap will be updated
    updateNearestSpot();
}

void BandmapWidget::finalizeBeforeAppExit()
{
    FCT_IDENTIFICATION;

    saveState();
}

void BandmapWidget::determineStepDigits(double &step, int &digits) const
{
    FCT_IDENTIFICATION;

    switch (zoom) {
    case ZOOM_100HZ: step = 0.0001; digits = 4; break;
    case ZOOM_250HZ: step = 0.00025; digits = 4; break;
    case ZOOM_500HZ: step = 0.0005; digits = 4; break;
    case ZOOM_1KHZ: step = 0.001; digits = 3; break;
    case ZOOM_2K5HZ: step = 0.0025; digits = 3; break;
    case ZOOM_5KHZ: step = 0.005; digits = 3; break;
    case ZOOM_10KHZ: step = 0.01; digits = 2; break;
    }

    /* bands below are too wide for BandMap, therefore it is needed to short them */
    if ( currentBand.start >= 28.0 && currentBand.start < 420.0 )
    {
        step = step * 10;
    }
    if ( ( currentBand.start >= 420.0 && currentBand.start < 2300.0 )
         || currentBand.start == 119980 )
    {
        step = step * 100;
    }
    else if ( currentBand.start >= 2300.0 && currentBand.start < 75500.0 )
    {
        step = step * 1000;
    }
    else if (currentBand.start == 75500.0 || currentBand.start >= 142000.0)
    {
        step = step * 10000;
    }
}

void BandmapWidget::clearAllCallsignFromScene()
{
    FCT_IDENTIFICATION;

    QMutableListIterator<QGraphicsLineItem*> lineIterator(lineItemList);

    while ( lineIterator.hasNext() )
    {
        lineIterator.next();
        bandmapScene->removeItem(lineIterator.value());
        delete lineIterator.value();
    }

    lineItemList.clear();

    QMutableListIterator<QGraphicsTextItem*> textIterator(textItemList);

    while ( textIterator.hasNext() )
    {
        textIterator.next();
        bandmapScene->removeItem(textIterator.value());
        delete textIterator.value();
    }

    textItemList.clear();
}

void BandmapWidget::clearFreqMark(QGraphicsPolygonItem **currentPolygon)
{
    FCT_IDENTIFICATION;

    if ( *currentPolygon != nullptr )
    {
        bandmapScene->removeItem(*currentPolygon);
        delete *currentPolygon;
        *currentPolygon = nullptr;
    }
}

void BandmapWidget::drawFreqMark(const double freq,
                                 const double step,
                                 const QColor &color,
                                 QGraphicsPolygonItem **currentPolygon)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq << step << color;

    clearFreqMark(currentPolygon);

    /* do not show the freq mark if it is outside the bandmap */
    if ( freq < currentBand.start || freq > currentBand.end )
    {
        return;
    }

    int Yposition = Freq2ScenePos(freq).y();

    QPolygonF poly;
    poly << QPointF(-1, Yposition)
         << QPointF(-7, Yposition - 7)
         << QPointF(-7, Yposition + 7);

    *currentPolygon = bandmapScene->addPolygon(poly,
                                              QPen(Qt::NoPen),
                                              QBrush(color, Qt::SolidPattern));
}

void BandmapWidget::drawTXRXMarks(double step)
{
    FCT_IDENTIFICATION;

    // do not show bandmap for submm bands
    if ( !isActive || rx_freq > 250000.0 || currentBand.start >= 300000.0 )
    {
        return;
    }

    /**************************/
    /* Draw RX frequency mark */
    /**************************/
    drawFreqMark(rx_freq, step, QColor(30, 180, 30), &rxMark);

    /**************************/
    /* Draw TX frequency mark */
    /**************************/
    if ( tx_freq >= currentBand.start
         && tx_freq <= currentBand.end
         && tx_freq != rx_freq )
    {
        drawFreqMark(tx_freq, step, QColor(255, 0, 0), &txMark);
    }
    else
    {
        clearFreqMark(&txMark);
    }
}

void BandmapWidget::removeDuplicates(DxSpot &spot)
{
    FCT_IDENTIFICATION;

    QMap<double, DxSpot>::iterator lower = spots.lowerBound(spot.freq - 0.005);
    QMap<double, DxSpot>::iterator upper = spots.upperBound(spot.freq + 0.005);

    while ( lower != upper )
    {
        if ( lower.value().callsign.compare(spot.callsign, Qt::CaseInsensitive) == 0 )
        {
            lower = spots.erase(lower);
        }
        else
        {
            ++lower;
        }
    }
}

void BandmapWidget::addSpot(DxSpot spot)
{
    FCT_IDENTIFICATION;

    // only master Widget adds spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    qCDebug(function_parameters) << spot.freq << spot.callsign;

    this->removeDuplicates(spot);
    spots.insert(spot.freq, spot);

    if ( spot.band == currentBand.name )
    {
        qint64 currTime = QDateTime::currentMSecsSinceEpoch();

        /* if the spots are received slowly, this will guarantee
         * that Spots will be displayed as soon as they are received.
         * QLog does not have to wait for the timer to tick to update the stations.
         */
        if ( currTime -  BANDMAP_MAX_REFRESH_TIME >= lastStationUpdate )
            updateStations();
        else
            /* If the spot are received quickly then store them and wait for QTimer tick */
            increasePendingSpots();
    }

    //update NonVFO Pending spot counters
    for ( BandmapWidget *nonVfoWidget : static_cast<const QList<BandmapWidget *>>(nonVfoWidgets) )
        if ( nonVfoWidget->getBand().name == spot.band )
            nonVfoWidget->increasePendingSpots();
}

void BandmapWidget::updateStationTimer()
{
    FCT_IDENTIFICATION;

    /* This function handle QTime tick to update Stations */

    qint64 currTime = QDateTime::currentMSecsSinceEpoch();

    /* If there is (are) station(s) or Time to Aging occured then update the bandmap */
    if ( pendingSpots > 0
         || currTime - BANDMAP_AGING_CHECK_TIME >= lastStationUpdate )
    {
        updateStations();
    }
}

DxSpot BandmapWidget::nearestSpot(const double freq) const
{
    FCT_IDENTIFICATION;

    QMap<double, DxSpot>::const_iterator it = spots.constFind(freq);

    if( it == spots.cend() )
    {
        QMap<double, DxSpot>::const_iterator lower = spots.lowerBound(freq - Hz2MHz(1000));
        QMap<double, DxSpot>::const_iterator upper = spots.upperBound(freq + Hz2MHz(1000));

        it = std::min_element( lower, upper,
                [freq](const DxSpot &p1,
                       const DxSpot &p2)
                {
                return
                    qAbs(p1.freq - freq) <
                    qAbs(p2.freq - freq);
                });

        if ( it != upper )
        {
            /* FOUND */
            return it.value();
        }
        else
        {
            /* Not found */
            return DxSpot();
        }
    }

    /* Exact Match */
    return it.value();
}

void BandmapWidget::updateNearestSpot(bool force)
{
    FCT_IDENTIFICATION;

    // only master Widget updates spot
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    DxSpot currNearestSpot;

    currNearestSpot = nearestSpot(rx_freq);

    if ( force || currNearestSpot.callsign != lastNearestSpot.callsign )
    {
        emit nearestSpotFound(currNearestSpot);
        lastNearestSpot = currNearestSpot;
    }
}

void BandmapWidget::setBandmapAnimation(bool isEnable)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << isEnable;

    bandmapAnimation = isEnable;
}

void BandmapWidget::setBand(const Band &newBand, bool savePrevBandZoom)
{
    FCT_IDENTIFICATION;

    if ( savePrevBandZoom ) saveState();

    currentBand = newBand;
    zoom = getSavedZoom(newBand);
    zoomFreq = getSavedScrollFreq(newBand);
    zoomWidgetYOffset = WIDGET_CENTER;
    ui->zoomSlider->blockSignals(true);
    ui->zoomSlider->setSliderPosition(zoom);
    ui->zoomSlider->blockSignals(false);

    QWidget *dock = parentWidget();
    if ( dock )
        dock->setWindowTitle(tr("Bandmap") + " " + newBand.name);
}

void BandmapWidget::saveCurrentZoom()
{
    FCT_IDENTIFICATION;

    LogParam::setBandmapZoom(objectName(), currentBand.name, zoom);
}

BandmapWidget::BandmapZoom BandmapWidget::getSavedZoom(const Band &band)
{
    FCT_IDENTIFICATION;

    // if a zoom is not set, try to get it from the main Bandmap widget
    const QVariant &zoomVariantMain = LogParam::getBandmapZoom(MAIN_WIDGET_OBJECT_NAME, band.name, ZOOM_10KHZ);
    QVariant zoomVariant = LogParam::getBandmapZoom(objectName(), band.name, zoomVariantMain);
    return zoomVariant.value<BandmapWidget::BandmapZoom>();
}

void BandmapWidget::saveCurrentScrollFreq()
{
    FCT_IDENTIFICATION;

    LogParam::setBandmapScrollFreq(objectName(), currentBand.name, visibleCentreFreq());
}

double BandmapWidget::getSavedScrollFreq(const Band &band)
{
    FCT_IDENTIFICATION;

    return LogParam::getBandmapScrollFreq(objectName(), band.name);
}

double BandmapWidget::visibleCentreFreq() const
{
    FCT_IDENTIFICATION;

    QPoint point(0,ui->scrollArea->verticalScrollBar()->value() + WIDGET_CENTER);
    double ret = ScenePos2Freq(ui->graphicsView->mapToScene(point));
    qCDebug(runtime) << "Centre freq" << ret;
    return ret;
}

bool BandmapWidget::isAlreadyOpened(const Band &band) const
{
    FCT_IDENTIFICATION;

    for ( const BandmapWidget *widget : static_cast<const QList<BandmapWidget *>>(nonVfoWidgets))
    {
        if ( widget && widget->isVisible() && widget->getBand() == band )
            return true;
    }
    return false;
}

void BandmapWidget::saveState()
{
    FCT_IDENTIFICATION;

    saveCurrentZoom();
    saveCurrentScrollFreq();
}

void BandmapWidget::spotAgingChanged(int)
{
    FCT_IDENTIFICATION;

    LogParam::setBandmapAging(objectName(), ui->clearSpotOlderSpin->value());
}

void BandmapWidget::clearSpots()
{
    FCT_IDENTIFICATION;

    spots.clear();
    updateStations();
    updateNearestSpot();
}

void BandmapWidget::setZoom(int zoomParam)
{
    FCT_IDENTIFICATION;

    zoomWidgetYOffset = WIDGET_CENTER;
    zoomFreq = ( keepRXCenter ) ? rx_freq
                              : visibleCentreFreq();

    zoom = static_cast<BandmapZoom>(static_cast<int>(zoomParam));
    setBandmapAnimation(false);
    update();
    scrollToFreq(zoomFreq);
    setBandmapAnimation(true);
}

void BandmapWidget::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // only master Widget modifies spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    qint32 dxcc = record.value("dxcc").toInt();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());
    const QString &callsign = record.value("callsign").toString();

    for ( auto it = spots.begin(); it != spots.end(); ++it )
    {
        DxSpot &spot =  it.value();
        spot.status = Data::dxccNewStatusWhenQSOAdded(spot.status,
                                                      spot.dxcc.dxcc,
                                                      spot.band,
                                                      ( ( spot.modeGroupString == BandPlan::MODE_GROUP_STRING_FT8 )
                                                           ? BandPlan::MODE_GROUP_STRING_DIGITAL
                                                           : dxccModeGroup ),
                                                      dxcc,
                                                      band,
                                                      dxccModeGroup);
        if ( spot.callsign == callsign )
            spot.dupeCount = Data::dupeNewCountWhenQSOAdded(spot.dupeCount,
                                                            spot.band,
                                                            spot.modeGroupString,
                                                            band,
                                                            dxccModeGroup);
    }
    updateStations();
    if ( callsign == lastNearestSpot.callsign )
        updateNearestSpot(true);
}

void BandmapWidget::updateSpotsStatusWhenQSOUpdated(const QSqlRecord &)
{
    FCT_IDENTIFICATION;

    // at this point, we don't know if callsign has been changed or other field.
    // TODO: DXCC status
    // TODO: Dupe status update

    // for ( auto it = spots.begin(); it != spots.end(); ++it )
    // {
    //     DxSpot &spot =  it.value();
    //     spot.dupeCount = Data::countDupe(spot.callsign, spot.band, spot.modeGroupString);
    // }
}

void BandmapWidget::updateSpotsDupeWhenQSODeleted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // only master Widget updates spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    // Pay attention: this method is called before the QSO is deleted from contacts
    const QString &callsign = record.value("callsign").toString();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());

    for ( auto it = spots.begin(); it != spots.end(); ++it )
    {
        DxSpot &spot =  it.value();

        if ( spot.dupeCount && spot.callsign == callsign )
            spot.dupeCount = Data::dupeNewCountWhenQSODelected(spot.dupeCount,
                                                               spot.band,
                                                               spot.modeGroupString,
                                                               band,
                                                               dxccModeGroup);
    }
    // do not call updateStation. it will be updated at the end of delete procedure
    // by updateSpotsDxccStatusWhenQSODeleted;
}

void BandmapWidget::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    FCT_IDENTIFICATION;

    // only master Widget updates spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    // this method is called at the end of QSO Delete (after commit).

    if ( entities.isEmpty() )
        return;

    for ( auto it = spots.begin(); it != spots.end(); ++it )
    {
        DxSpot &spot =  it.value();

        if ( !entities.contains(spot.dxcc.dxcc) )
            continue;

        spot.status = Data::instance()->dxccStatus(spot.dxcc.dxcc, spot.band, spot.modeGroupString);
    }
    updateStations();
    updateNearestSpot(true);
}

void BandmapWidget::recalculateDxccStatus()
{
    FCT_IDENTIFICATION;

    // only master Widget updates spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    for ( auto it = spots.begin(); it != spots.end(); ++it )
    {
        DxSpot &spot = it.value();
        spot.status = Data::instance()->dxccStatus(spot.dxcc.dxcc, spot.band, spot.modeGroupString);
    }
    updateStations();
    updateNearestSpot(true);
}

void BandmapWidget::resetDupe()
{
    FCT_IDENTIFICATION;

    // only master Widget updates spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    for ( auto it = spots.begin(); it != spots.end(); ++it )
        it.value().dupeCount = 0;

    updateStations();
}

void BandmapWidget::recalculateDupe()
{
    FCT_IDENTIFICATION;

    // only master Widget updates spots
    if ( isNonVfo )
    {
        qCDebug(runtime) << "NonVFO widget - skipping";
        return;
    }

    for ( auto it = spots.begin(); it != spots.end(); ++it )
    {
        DxSpot &spot = it.value();
        spot.dupeCount = Data::countDupe(spot.callsign,
                                         spot.band,
                                         spot.modeGroupString);
    }
    updateStations();
}

void BandmapWidget::focusZoomFreq(int, int)
{
    FCT_IDENTIFICATION;

    if ( zoomFreq > 0.0 )
    {
        int newScrollValue = qMin(qMax(Freq2ScenePos(zoomFreq).y() - ( zoomWidgetYOffset ), 0.0),
                                  (double)ui->scrollArea->verticalScrollBar()->maximum());
        ui->scrollArea->verticalScrollBar()->setValue(newScrollValue);
        zoomFreq = 0.0;
    }
}

void BandmapWidget::clickNewBandmapWindow()
{
    FCT_IDENTIFICATION;

    const QString widgetID = QString("bandmap%1").arg(QDateTime::currentMSecsSinceEpoch());
    saveCurrentZoom();
    emit requestNewNonVfoBandmapWindow(widgetID, currentBand.name);
}

void BandmapWidget::spotClicked(const QString &call,
                                double freq,
                                BandPlan::BandPlanMode mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << call << freq << mode;
    qCDebug(runtime) << "Last Tuned DX" << lastTunedDX.callsign << lastTunedDX.freq;

    /* Do not emit the Spot two times - double click*/
    if ( lastTunedDX.callsign == call
         && lastTunedDX.freq == freq )
        return;

    emit tuneDx(spots.value(freq));

    lastTunedDX.callsign = call;
    lastTunedDX.freq = freq;

    // allow to re-click the spot.
    // I don't want to use double click here because users expect to single-clicking on the map and double click
    // can emit tuneDX two times what causes an issue in Callbooks section and other parts of QLog.
    // However, to be able to click on the callsign again, it needs to be disabled for a short while.
    // That's why there is a delay.
    QTimer::singleShot(3000, this, [this]() {
        lastTunedDX.callsign.clear();
        lastTunedDX.freq = 0.0;
    });

}

void BandmapWidget::showContextMenu(const QPoint &point)
{
    FCT_IDENTIFICATION;

    if ( ui->graphicsView->itemAt(point) )
    {
        return;
    }

    QMenu contextMenu(this);
    QMenu bandsMenu(tr("Show Band"), &contextMenu);

    for ( const Band &enabledBand : BandPlan::bandsList(false, true))
    {
        QAction* action = new QAction(enabledBand.name);
        connect(action, &QAction::triggered, this, [this, enabledBand]()
        {
            setBand(enabledBand);
            update();
        });
        if ( enabledBand == currentBand )
        {
            action->setCheckable(true);
            action->setChecked(true);
        }
        bandsMenu.addAction(action);
    }

    QAction* centerRXAction = new QAction(tr("Center RX"), &contextMenu);
    centerRXAction->setCheckable(true);
    centerRXAction->setChecked(keepRXCenter);
    connect(centerRXAction, &QAction::triggered, this, &BandmapWidget::centerRXActionChecked);

    contextMenu.addMenu(&bandsMenu);
    contextMenu.addAction(centerRXAction);

    contextMenu.exec(ui->graphicsView->mapToGlobal(point));
}

void BandmapWidget::updateTunedFrequency(VFOID, double vfoFreq, double ritFreq, double xitFreq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << vfoFreq << ritFreq << xitFreq;

    lastSeenVFOFreq = vfoFreq;

    isActive = (ritFreq >= currentBand.start && ritFreq <= currentBand.end);

    if ( isNonVfo )
    {
        if ( isActive )
        {
            rx_freq = ritFreq;
            tx_freq = xitFreq;
            // bandmap is showing the tuned frequency, so show the marker
            drawMarkers(rx_freq);
        }
        else
        {
            rx_freq = (rx_freq == 0.0) ? getSavedScrollFreq(currentBand) : rx_freq;
            tx_freq = (tx_freq == 0.0) ? getSavedScrollFreq(currentBand) : tx_freq;
        }
    }
    else
    {
        if ( !isActive )
        {
            /* Operator switched a band */
            const Band& newBand = BandPlan::freq2Band(ritFreq);
            if ( !newBand.name.isEmpty() && !isAlreadyOpened(newBand) )
            {
                rx_freq = ritFreq;
                tx_freq = xitFreq;
                isActive = true;
                setBand(newBand);
                /**********************/
                /* Redraw all bandmap */
                /**********************/
                update();
            }
            else
            {
                rx_freq = (rx_freq == 0.0) ? getSavedScrollFreq(currentBand) : rx_freq;
                tx_freq = (tx_freq == 0.0) ? getSavedScrollFreq(currentBand) : tx_freq;
            }
        }
        else
        {
            rx_freq = ritFreq;
            tx_freq = xitFreq;
            drawMarkers(rx_freq);
        }
        updateNearestSpot();
    }

    if ( !isActive )
    {
        clearFreqMark(&txMark);
        clearFreqMark(&rxMark);
    }
}

void BandmapWidget::drawMarkers(double frequency)
{
    FCT_IDENTIFICATION;

    double step;
    int digits;

    determineStepDigits(step, digits);

    /************************/
    /* Draw TX and RX Marks */
    /************************/
    drawTXRXMarks(step);
    scrollToFreq(frequency);
}
void BandmapWidget::updateMode(VFOID, const QString &, const QString &mode,
                               const QString &subMode, qint32 width)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << subMode << width;

    const QString &newMode = BandPlan::modeToModeGroup(mode);

    if ( currBandMode != newMode )
    {
        currBandMode = newMode;
        update();
    }
}

BandmapWidget::~BandmapWidget()
{
    FCT_IDENTIFICATION;

    if ( update_timer )
    {
        update_timer->stop();
        update_timer->deleteLater();
    }

    nonVfoWidgets.removeAll(this);

    delete ui;
}

void BandmapWidget::resizeEvent(QResizeEvent *event)
{
    FCT_IDENTIFICATION;

    QWidget::resizeEvent(event);

    scrollToFreq(rx_freq);
}

bool BandmapWidget::eventFilter(QObject *, QEvent *event)
{
    FCT_IDENTIFICATION;

    if (event->type() == QEvent::Wheel)
    {
        QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent*>(event);

        if ( wheelEvent )
        {
            if ( QApplication::keyboardModifiers() == Qt::ControlModifier )
            {
                /*
                 * CRTL + Mouse Wheel
                 *
                 * Zoom In/Out
                 */

                QPoint zoomViewPoint = ui->graphicsView->mapFromGlobal(QCursor::pos());
                zoomWidgetYOffset = zoomViewPoint.y() - ui->scrollArea->verticalScrollBar()->value();
                zoomFreq = ScenePos2Freq(ui->graphicsView->mapToScene(zoomViewPoint));

                QPoint wheelDelta(wheelEvent->angleDelta());
                int delta = (wheelDelta.y() > 0) ? 1 : -1;
                ui->zoomSlider->setValue(ui->zoomSlider->value() + delta * ui->zoomSlider->singleStep());

                /*
                 * DO NOT focus zoomed Freq here because the scrollbar
                 * is not resized yet and it is not possible to compute
                 * a correct value for scrollbar value (scrollbar min/max
                 * is recomputed later and it emits RangeChanged signal).
                 * SO focus zoomed Freq in SLOT for RangeChanged signal.
                 */
                event->accept();
                return true;
            }
        }
    }
    return false;
}

void BandmapWidget::scrollToFreq(double freq)
{
    FCT_IDENTIFICATION;

    qreal freqScenePos = Freq2ScenePos(freq).y();

    QPropertyAnimation *anim = new QPropertyAnimation(ui->scrollArea->verticalScrollBar(), "value", this);
    anim->setDuration((bandmapAnimation) ? 300 : 0);
    anim->setStartValue(ui->scrollArea->verticalScrollBar()->value());

    if ( keepRXCenter )
    {
        /* If RX freq should be center then center it */
        anim->setEndValue(freqScenePos - (WIDGET_CENTER));
    }
    else
    {
        /* If RX freq is out-of-scene then keep the RX mark visible - this is not centering !!! */
        int sceneSize = height() - 60;
        int sliderSceneMin = ui->scrollArea->verticalScrollBar()->value();
        int sliderSceneMax = ui->scrollArea->verticalScrollBar()->value() + sceneSize;

        if ( freqScenePos < sliderSceneMin )
        {
            anim->setEndValue(sliderSceneMin - (sliderSceneMin - freqScenePos) - 40);
        }
        else if ( freqScenePos > sliderSceneMax - 20 ) //asymetric becuase possible slider below
        {
            anim->setEndValue(sliderSceneMin + (freqScenePos - sliderSceneMax) + 60);
        }
        else
        {
            anim->setEndValue(ui->scrollArea->verticalScrollBar()->value());
        }
    }
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QPointF BandmapWidget::Freq2ScenePos(const double freq) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    if ( freq < currentBand.start || freq > currentBand.end )
    {
        return QPointF();
    }

    double step;
    int digits;

    determineStepDigits(step, digits);

    QPointF ret(0, ((freq - currentBand.start) / step) * PIXELSPERSTEP);

    qCDebug(runtime) << ret;

    return ret;
}

double BandmapWidget::ScenePos2Freq(const QPointF &point) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << point;

    double step;
    int digits;

    determineStepDigits(step, digits);

    double ret = currentBand.start + (point.y() / PIXELSPERSTEP) * step;

    if ( ret > currentBand.end )
    {
        ret = currentBand.end;
    }

    if ( ret < currentBand.start )
    {
        ret = currentBand.start;
    }

    qCDebug(runtime) << ret;

    return ret;
}

void BandmapWidget::centerRXActionChecked(bool state)
{
    FCT_IDENTIFICATION;

    keepRXCenter = state;
    zoomFreq = 0.0;
    LogParam::setBandmapCenterRX(objectName(), keepRXCenter);

    if ( keepRXCenter )
        scrollToFreq(rx_freq);
}

void GraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *evt)
{
    FCT_IDENTIFICATION;

    if ( evt->button() & Qt::LeftButton )
    {
        QGraphicsItem *item = itemAt(evt->scenePos(), QTransform());
        QGraphicsTextItem *focusedSpot = dynamic_cast<QGraphicsTextItem*>(item);

        if ( focusedSpot && focusedSpot->property("freq").isValid() )
            emit spotClicked(focusedSpot->toPlainText().split(" ").first(),
                             focusedSpot->property("freq").toDouble(),
                             static_cast<BandPlan::BandPlanMode>(focusedSpot->property("bandmode").toInt()));
    }
    evt->accept();
}

void GraphicsScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *evt)
{
    FCT_IDENTIFICATION;

    evt->accept();
}

#undef WIDGET_CENTER
