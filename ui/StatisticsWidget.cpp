#include <QChart>
#include <QChartView>
#include <QBarSet>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QBarSeries>
#include <QSqlQuery>
#include <QDateTime>
#include <QDebug>
#include <QComboBox>
#include <QStringListModel>
#include <QGraphicsLayout>
#include <QMessageBox>
#include "StatisticsWidget.h"
#include "ui_StatisticsWidget.h"
#include "core/debug.h"
#include "models/SqlListModel.h"
#include <core/Gridsquare.h>

MODULE_IDENTIFICATION("qlog.ui.statisticswidget");

// default statistics interval [in days]
#define DEFAULT_STAT_RANGE -1

void StatisticsWidget::mainStatChanged(int idx)
{
     FCT_IDENTIFICATION;

     qCDebug(function_parameters) << idx;

     setSubTypesCombo(idx);
     refreshGraph();
}


void StatisticsWidget::refreshWidget()
{
    FCT_IDENTIFICATION;

    if ( !isVisible() )
        return;

    refreshCombos();
    refreshGraph();
}

void StatisticsWidget::refreshGraph()
{
     FCT_IDENTIFICATION;

     if ( !isVisible() )
         return;

     QStringList genericFilter;

     genericFilter << " 1 = 1 "; //just initialization - use only in case of empty Options

     if ( ui->myCallCombo->currentIndex() != 0 )
         genericFilter << " (station_callsign = '" + ui->myCallCombo->currentText() + "') ";

     if ( ui->myGridCombo->currentIndex() != 0 )
     {
         if ( ui->myGridCombo->currentText().isEmpty() )
             genericFilter << " (my_gridsquare is NULL) ";
         else
             genericFilter << " (my_gridsquare = '" + ui->myGridCombo->currentText() + "') ";
     }

     if ( ui->myRigCombo->currentIndex() != 0 )
     {
         if ( ui->myRigCombo->currentText().isEmpty() )
             genericFilter << " (my_rig is NULL) ";
         else
             genericFilter << " (my_rig = '" + ui->myRigCombo->currentText() + "') ";
     }

     if ( ui->myAntennaCombo->currentIndex() != 0 )
     {
         if ( ui->myAntennaCombo->currentText().isEmpty() )
             genericFilter << " (my_antenna is NULL) ";
         else
             genericFilter << " (my_antenna = '" + ui->myAntennaCombo->currentText() + "') ";
     }

     if ( ui->bandCombo->currentIndex() != 0 &&  ! ui->bandCombo->currentText().isEmpty() )
         genericFilter << " (band = '" + ui->bandCombo->currentText() + "') ";

     if ( ui->useDateRangeCheckBox->isChecked() )
         genericFilter << " (datetime(start_time) BETWEEN datetime('" + ui->startDateEdit->dateTime().toString("yyyy-MM-dd HH:mm:ss")
                          + "') AND datetime('" + ui->endDateEdit->dateTime().toString("yyyy-MM-dd HH:mm:ss") + "') ) ";

     qCDebug(runtime) << "main " << ui->statTypeMainCombo->currentIndex()
                      << " secondary " << ui->statTypeSecCombo->currentIndex();

     if ( ui->statTypeMainCombo->currentIndex() == 0 )
     {
         QString stmt;

         switch ( ui->statTypeSecCombo->currentIndex() )
         {

         case 0:
         case 1:
         case 2:
         case 3:
         {
             QString startGenerator = "1";
             QString endGenerator = "12";
             QString formatGenerator = "%m";
             QString XYMapping = "col1, SUM(cnt)";

             if ( ui->statTypeSecCombo->currentIndex() == 0 )
             {
                 if ( ui->useDateRangeCheckBox->isChecked() )
                 {
                     startGenerator = ui->startDateEdit->date().toString("yyyy");
                     endGenerator = ui->endDateEdit->date().toString("yyyy");
                 }
                 else
                 {
                    startGenerator = "CAST(MIN(start_time) as INTEGER) from contacts";
                    endGenerator = " (select strftime('%Y', DATE()))";
                 }
                 formatGenerator = "%Y";
             }
             else if ( ui->statTypeSecCombo->currentIndex() == 1 )
             {
                 startGenerator = "1";
                 endGenerator = "12";
                 formatGenerator = "%m";
             }
             else if ( ui->statTypeSecCombo->currentIndex() == 2 )
             {
                 startGenerator = "0";
                 endGenerator = "6";
                 formatGenerator = "%w";
                 XYMapping = "case col1 when 0 THEN '" + tr("Sun") + "' "
                             "WHEN 1 THEN '" + tr("Mon") + "' "
                             "WHEN 2 THEN '" + tr("Tue") + "' "
                             "WHEN 3 THEN '" + tr("Wed") + "' "
                             "WHEN 4 THEN '" + tr("Thu") + "' "
                             "WHEN 5 THEN '" + tr("Fri") + "' "
                             "ELSE '" + tr("Sat") + "' END, "
                             "SUM(cnt) ";
             }
             else if ( ui->statTypeSecCombo->currentIndex() == 3 )
             {
                 startGenerator = "0";
                 endGenerator = "23";
                 formatGenerator = "%H";
             }

             stmt = "WITH RECURSIVE cnt(incnt) AS ( "
                    " SELECT " + startGenerator + " "
                    " UNION ALL "
                    " SELECT incnt + 1 "
                    " FROM cnt "
                    " WHERE incnt < " + endGenerator + " "
                    " ) "
                    " SELECT " + XYMapping + " "
                    " FROM "
                    " ( "
                    "   SELECT  incnt as col1, 0 as cnt from cnt "
                    "   UNION ALL "
                    "   SELECT CAST(strftime('" + formatGenerator +"', start_time) as INTEGER) as col1, count(1) as cnt "
                    "   FROM contacts "
                    "   WHERE " + genericFilter.join(" AND ") + " "
                    "   GROUP BY col1 "
                    " ) "
                    " GROUP BY col1 "
                    " ORDER BY col1";
         }
             break;
         case 4:
             stmt = "SELECT mode, COUNT(1) FROM contacts WHERE "
                     + genericFilter.join(" AND ") + " GROUP BY mode ORDER BY mode";
             break;
         case 5:
             stmt = "SELECT band, cnt FROM (SELECT band, start_freq, COUNT(1) AS cnt FROM contacts c, bands b WHERE "
                    + genericFilter.join(" AND ")
                    + " AND c.band = b.name GROUP BY band, start_freq) ORDER BY start_freq";
             break;
         case 6:
             stmt = "SELECT cont, COUNT(1) FROM contacts WHERE "
                    + genericFilter.join(" AND ")
                    + " GROUP BY cont ORDER BY cont";
             break;
         case 7:
             stmt = "SELECT IFNULL(prop_mode, '" + tr("Not specified") + "'), COUNT(1) FROM contacts WHERE "
                    + genericFilter.join(" AND ") + " GROUP BY prop_mode ORDER BY prop_mode";
             break;
         }

         qCDebug(runtime) << stmt;

         QSqlQuery query(stmt);

         drawBarGraphs(ui->statTypeMainCombo->currentText()
                       + " "
                       + ui->statTypeSecCombo->currentText(),
                       query);
     }
     else if ( ui->statTypeMainCombo->currentIndex() == 1 )
     {
         QString stmt;

         switch ( ui->statTypeSecCombo->currentIndex() )
         {

         case 0:
             stmt = "SELECT (1.0 * COUNT(1)/(SELECT COUNT(1) AS total_cnt FROM contacts WHERE "
                    + genericFilter.join(" AND ") +")) * 100 FROM contacts WHERE "
                    + genericFilter.join(" AND ")
                    + " AND (eqsl_qsl_rcvd = 'Y' OR lotw_qsl_rcvd = 'Y' OR qsl_rcvd = 'Y')";
             break;
         }

         QSqlQuery query(stmt);

         qCDebug(runtime) << stmt;
         QPieSeries *series = new QPieSeries();

         query.next();

         float confirmed = query.value(0).toInt();
         float notConfirmed = 100.0 - confirmed;

         series->append(tr("Confirmed ") + QString::number(confirmed) + "%", confirmed);
         series->append(tr("Not Confirmed ") + QString::number(notConfirmed) + "%", notConfirmed);
         series->setLabelsVisible(true);

         drawPieGraph(QString(), series);
     }
     else if ( ui->statTypeMainCombo->currentIndex() == 2 )
     {
         QString stmt;

         switch ( ui->statTypeSecCombo->currentIndex() )
         {
         case 0:
             stmt = "SELECT d.name, COUNT(1) AS cnt FROM contacts c, dxcc_entities d WHERE "
                    + genericFilter.join(" AND ")
                    + " AND c.dxcc = d.id GROUP BY d.name ORDER BY cnt DESC LIMIT 10";
             break;
         case 1:
             stmt = "SELECT SUBSTR(gridsquare,1,4), COUNT(1) AS cnt FROM contacts WHERE gridsquare IS NOT NULL GROUP by SUBSTR(gridsquare,1,4) ORDER BY cnt DESC LIMIT 10";
             break;
         }

         qCDebug(runtime) << stmt;

         QSqlQuery query(stmt);

         drawBarGraphs(ui->statTypeMainCombo->currentText()
                       + " "
                       + ui->statTypeSecCombo->currentText(),
                       query);

     }
     else if ( ui->statTypeMainCombo->currentIndex() == 3 )
     {
         QString stmt;

         switch ( ui->statTypeSecCombo->currentIndex() )
         {
         case 0:
             QString distCoef = QString::number(Gridsquare::localeDistanceCoef());
             stmt = QString("WITH hist AS ( "
                    " SELECT CAST((distance * %1)/500.00 AS INTEGER) * 500 as dist_floor, "
                    " COUNT(1) AS count "
                    " FROM contacts "
                    " WHERE " + genericFilter.join(" AND ") + " AND distance IS NOT NULL "
                    " GROUP BY 1 "
                    " ORDER BY 1 "
                    " ) "
                    //" SELECT dist_floor || ' - ' || (dist_floor + 500) as dist_range, count "
                    "SELECT dist_floor as dist_range, count "
                    " FROM hist "
                    " ORDER BY 1").arg(distCoef);
             break;
         }

         qCDebug(runtime) << stmt;

         QSqlQuery query(stmt);

         drawBarGraphs(ui->statTypeMainCombo->currentText()
                       + " "
                       + ui->statTypeSecCombo->currentText(),
                       query);
     }
     else if ( ui->statTypeMainCombo->currentIndex() == 4 )
     {
         QStringList confirmed("1=2 ");

         if ( ui->eqslCheckBox->isChecked() )
             confirmed << " eqsl_qsl_rcvd = 'Y' ";

         if ( ui->lotwCheckBox->isChecked() )
             confirmed << " lotw_qsl_rcvd = 'Y' ";

         if ( ui->paperCheckBox->isChecked() )
             confirmed << " qsl_rcvd = 'Y' ";

         QString innerCase = " CASE WHEN (" + confirmed.join("or") + ") THEN 1 ELSE 0 END ";
         QString stmtMyLocations = "SELECT DISTINCT my_gridsquare FROM contacts WHERE " + genericFilter.join(" AND ");
         QSqlQuery myLocations(stmtMyLocations);

         qCDebug(runtime) << stmtMyLocations;

         drawMyLocationsOnMap(myLocations);

         QString stmt;

         switch ( ui->statTypeSecCombo->currentIndex() )
         {
         case 0:
         case 1:
             stmt = "SELECT callsign, gridsquare, my_gridsquare, SUM(confirmed) FROM (SELECT callsign, gridsquare, my_gridsquare,"
                        + innerCase +" AS confirmed FROM contacts WHERE gridsquare is not NULL AND "
                        + genericFilter.join(" AND ") +" ) GROUP BY callsign, gridsquare, my_gridsquare";
             break;
         case 2:
             QString unit;
             Gridsquare::distance2localeUnitDistance(0, unit);
             QString distCoef = QString::number(Gridsquare::localeDistanceCoef());
             QString sel = QString("SELECT callsign || '<br>' || CAST(ROUND(distance * %1,0) AS INT) || ' %2', gridsquare, my_gridsquare, ").arg(distCoef, unit);

             stmt = sel + innerCase + " AS confirmed FROM contacts WHERE "
                        + genericFilter.join(" AND ") + " AND distance = (SELECT MAX(distance) FROM contacts WHERE "
                        + genericFilter.join(" AND ") + ")";
             break;
         }

         QSqlQuery query(stmt);
         qCDebug(runtime) << stmt;

         switch ( ui->statTypeSecCombo->currentIndex() )
         {
         case 0:
         case 2:
             drawPointsOnMap(query);
             break;

         case 1:
             drawFilledGridsOnMap(query);
             break;
         }

         ui->stackedWidget->setCurrentIndex(1);
     }
}

void StatisticsWidget::dateRangeCheckBoxChanged(int)
{
    FCT_IDENTIFICATION;

    if ( ui->useDateRangeCheckBox->isChecked() )
    {
        ui->startDateEdit->setEnabled(true);
        ui->endDateEdit->setEnabled(true);
    }
    else
    {
        ui->startDateEdit->setEnabled(false);
        ui->endDateEdit->setEnabled(false);
    }

    refreshGraph();
}

void StatisticsWidget::mapLoaded(bool)
{
    FCT_IDENTIFICATION;

    isMainPageLoaded = true;

    /* which layers will be active */
    postponedScripts += layerControlHandler.generateMapMenuJS(true, false, false, false, false, false, false, false, true);
    main_page->runJavaScript(postponedScripts);

    layerControlHandler.restoreLayerControlStates(main_page);
}

void StatisticsWidget::changeTheme(int theme)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << theme;

    QString themeJavaScript;

    if ( theme == 1 ) /* dark mode */
        themeJavaScript = "map.getPanes().tilePane.style.webkitFilter=\"brightness(0.6) invert(1) contrast(3) hue-rotate(200deg) saturate(0.3) brightness(0.9)\";";
    else
        themeJavaScript = "map.getPanes().tilePane.style.webkitFilter=\"\";";

    if ( !isMainPageLoaded )
        postponedScripts.append(themeJavaScript);
    else
        main_page->runJavaScript(themeJavaScript);
}

StatisticsWidget::StatisticsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StatisticsWidget),
    main_page(new WebEnginePage(this)),
    isMainPageLoaded(false),
    layerControlHandler("statistics", parent)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->myCallCombo->setModel(new QStringListModel(this));
    ui->myGridCombo->setModel(new QStringListModel(this));
    ui->myRigCombo->setModel(new QStringListModel(this));
    ui->myAntennaCombo->setModel(new QStringListModel(this));
    ui->bandCombo->setModel(new QStringListModel(this));

    ui->startDateEdit->setDisplayFormat(locale.formatDateTimeShortWithYYYY());
    ui->startDateEdit->setDate(QDate::currentDate().addDays(DEFAULT_STAT_RANGE));
    ui->startDateEdit->setTime(QTime::fromMSecsSinceStartOfDay(0));
    ui->endDateEdit->setDisplayFormat(locale.formatDateTimeShortWithYYYY());
    ui->endDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setTime(QTime::fromMSecsSinceStartOfDay(86399999));

    ui->graphView->setRenderHint(QPainter::Antialiasing);
    ui->graphView->setChart(new QChart());

    main_page->setWebChannel(&channel);
    ui->mapView->setPage(main_page);
    connect(ui->mapView, &QWebEngineView::loadFinished, this, &StatisticsWidget::mapLoaded);
    main_page->load(QUrl(QStringLiteral("qrc:/res/map/onlinemap.html")));
    ui->mapView->setFocusPolicy(Qt::ClickFocus);
    channel.registerObject("layerControlHandler", &layerControlHandler);
}

StatisticsWidget::~StatisticsWidget()
{
    FCT_IDENTIFICATION;
    main_page->deleteLater();
    delete ui;
}

bool StatisticsWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Show)
    {
        // We will not use refreshWidget here, even though at first glance it appears
        // to do the same thing. The difference is that we want class constructor to be as fast as possible.
        // Therefore, in the constructor, we do not populate the combo boxes. As a result, they are empty
        // when first displayed and need to be loaded and then combos for Rig, Ant, etc., can be set.
        refreshCombos();
        ui->statTypeMainCombo->blockSignals(true);
        ui->statTypeMainCombo->setCurrentIndex(0);
        ui->statTypeMainCombo->blockSignals(false);
        setSubTypesCombo(ui->statTypeMainCombo->currentIndex());
        ui->myRigCombo->blockSignals(true);
        ui->myRigCombo->setCurrentIndex(0);
        ui->myRigCombo->blockSignals(false);
        ui->myAntennaCombo->blockSignals(true);
        ui->myAntennaCombo->setCurrentIndex(0);
        ui->myAntennaCombo->blockSignals(false);
        refreshGraph();
    }
    return QWidget::event(event);  // Propagate the event further
}

void StatisticsWidget::drawBarGraphs(const QString &title, QSqlQuery &query)
{
    FCT_IDENTIFICATION;

    if ( query.lastQuery().isEmpty() ) return;

    QChart *chart = ui->graphView->chart();
    QBarSet* set = new QBarSet(title);
    QBarCategoryAxis* axisX = new QBarCategoryAxis();
    QBarSeries* series = new QBarSeries();
    QValueAxis *axisY = new QValueAxis();

    if ( chart != nullptr )
        chart->deleteLater();

    chart = new QChart();

    while ( query.next() )
    {
        axisX->append(query.value(0).toString());
        *set << query.value(1).toInt();
    }

    series->append(set);
    chart->addSeries(series);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    axisY->setTickCount(10);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    axisY->applyNiceNumbers();
    axisY->setLabelFormat("%d");

    series->setLabelsPosition(QAbstractBarSeries::LabelsInsideEnd);
    series->setLabelsVisible(true);

    chart->setTitle(title);
    chart->legend()->hide();
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    ui->stackedWidget->setCurrentIndex(0);
    ui->graphView->setChart(chart);
}

void StatisticsWidget::drawPieGraph(const QString &title, QPieSeries *series)
{
    FCT_IDENTIFICATION;

    QChart *chart = ui->graphView->chart();

    if ( chart != nullptr )
        chart->deleteLater();

    chart = new QChart();

    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->addSeries(series);
    chart->legend()->hide();
    chart->setTitle(title);

    ui->stackedWidget->setCurrentIndex(0);
    ui->graphView->setChart(chart);
}

void StatisticsWidget::drawMyLocationsOnMap(QSqlQuery &query)
{
    FCT_IDENTIFICATION;

    if ( query.lastQuery().isEmpty() )
        return;

    QList<QString> locations;

    while ( query.next() )
    {
        const QString &loc = query.value(0).toString();
        const Gridsquare stationGrid(loc);

        if ( stationGrid.isValid() )
        {
            double lat = stationGrid.getLatitude();
            double lon = stationGrid.getLongitude();
            locations.append(QString("[\"%1\", %2, %3, homeIcon]").arg(loc).arg(lat).arg(lon));
        }
    }

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "drawPointsGroup2([%1]);"
                                 "maidenheadConfWorked.redraw();").arg(locations.join(","));

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
        postponedScripts.append(javaScript);
    else
        main_page->runJavaScript(javaScript);
}

void StatisticsWidget::drawPointsOnMap(QSqlQuery &query)
{
    FCT_IDENTIFICATION;

    if ( query.lastQuery().isEmpty() )
        return;

    QList<QString> stations;
    QList<QString> shortPaths;

    qulonglong count = 0;

    while ( query.next() )
    {
        const Gridsquare stationGrid(query.value(1).toString());
        const Gridsquare myStationGrid(query.value(2).toString());
        if ( stationGrid.isValid() )
        {
            count++;
            double lat = stationGrid.getLatitude();
            double lon = stationGrid.getLongitude();
            stations.append(QString("[\"%1\", %2, %3, %4]").arg(query.value(0).toString())
                                                           .arg(lat)
                                                           .arg(lon)
                                                           .arg((query.value(3).toInt()) > 0 ? "greenIconSmall" : "yellowIconSmall"));
            shortPaths.append(QString("[%1, %2, %3, %4]")
                                  .arg(myStationGrid.getLatitude())
                                  .arg(myStationGrid.getLongitude())
                                  .arg(lat)
                                  .arg(lon));
        }
    }

    if ( count > 50000 )
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Statistics"), tr("Over 50000 QSOs. Display them?"),
                                      QMessageBox::Yes|QMessageBox::No);

        if ( reply != QMessageBox::Yes )
            stations.clear();
    }

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "drawPoints([%1]);"
                                 "drawShortPaths([%2]);"
                                 "maidenheadConfWorked.redraw();").arg(stations.join(",")).arg(shortPaths.join(","));

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
        postponedScripts.append(javaScript);
    else
        main_page->runJavaScript(javaScript);
}

void StatisticsWidget::drawFilledGridsOnMap(QSqlQuery &query)
{
    FCT_IDENTIFICATION;


    if ( query.lastQuery().isEmpty() ) return;

    QList<QString> confirmedGrids;
    QList<QString> workedGrids;

    while ( query.next() )
    {
        if ( query.value(2).toInt() > 0 && ! confirmedGrids.contains(query.value(1).toString()) )
            confirmedGrids << QString("\"" + query.value(1).toString() + "\"");
        else
            workedGrids << QString("\"" + query.value(1).toString() + "\"");
    }

    QString javaScript = QString("grids_confirmed = [ %1 ]; "
                                 "grids_worked = [ %2 ];"
                                 "mylocations = [];"
                                 "drawPoints([]);"
                                 "drawShortPaths([]);"
                                 "maidenheadConfWorked.redraw();").arg(confirmedGrids.join(","), workedGrids.join(","));

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
        postponedScripts.append(javaScript);
    else
        main_page->runJavaScript(javaScript);
}

void StatisticsWidget::refreshCombos()
{
    FCT_IDENTIFICATION;

    refreshCombo(ui->myCallCombo, QLatin1String("SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign"));
    refreshCombo(ui->myRigCombo, QLatin1String("SELECT DISTINCT my_rig FROM contacts ORDER BY my_rig"));
    refreshCombo(ui->myAntennaCombo, QLatin1String("SELECT DISTINCT my_antenna FROM contacts ORDER BY my_antenna"));
    refreshCombo(ui->bandCombo, QLatin1String("SELECT DISTINCT band FROM contacts c, bands b WHERE c.band = b.name ORDER BY b.start_freq;"));
    refreshCombo(ui->myGridCombo, QLatin1String("SELECT DISTINCT UPPER(my_gridsquare) FROM contacts ORDER BY my_gridsquare"));
}

void StatisticsWidget::setSubTypesCombo(int mainTypeIdx)
{
    FCT_IDENTIFICATION;

    ui->statTypeSecCombo->blockSignals(true);

    ui->statTypeSecCombo->clear();

    ui->lotwCheckBox->setEnabled(false);
    ui->eqslCheckBox->setEnabled(false);
    ui->paperCheckBox->setEnabled(false);

    switch ( mainTypeIdx )
    {
    /* QSOs per */
    case 0:
    {
        ui->statTypeSecCombo->addItem(tr("Year"));
        ui->statTypeSecCombo->addItem(tr("Month"));
        ui->statTypeSecCombo->addItem(tr("Day in Week"));
        ui->statTypeSecCombo->addItem(tr("Hour"));
        ui->statTypeSecCombo->addItem(tr("Mode"));
        ui->statTypeSecCombo->addItem(tr("Band"));
        ui->statTypeSecCombo->addItem(tr("Continent"));
        ui->statTypeSecCombo->addItem(tr("Propagation Mode"));
    }
    break;

    /* Percents */
    case 1:
    {
        ui->statTypeSecCombo->addItem(tr("Confirmed / Not Confirmed"));
    }
    break;

    /* TOP 10 */
    case 2:
    {
        ui->statTypeSecCombo->addItem(tr("Countries"));
        ui->statTypeSecCombo->addItem(tr("Big Gridsquares"));
    }
    break;

    /* Histogram */
    case 3:
    {
        QString unit;
        Gridsquare::distance2localeUnitDistance(0, unit);
        ui->statTypeSecCombo->addItem(tr("Distance") + QString(" [%1]").arg(unit));
    }
    break;

    /* Show on Map */
    case 4:
    {
        ui->statTypeSecCombo->addItem(tr("QSOs"));
        ui->statTypeSecCombo->addItem(tr("Confirmed/Worked Grids"));
        ui->statTypeSecCombo->addItem(tr("ODX"));
        ui->lotwCheckBox->setEnabled(true);
        ui->eqslCheckBox->setEnabled(true);
        ui->paperCheckBox->setEnabled(true);
    }
    break;
    }

    ui->statTypeSecCombo->blockSignals(false);
}

void StatisticsWidget::refreshCombo(QComboBox * combo,
                                    const QString &sqlQeury)
{
    FCT_IDENTIFICATION;

    QString currSelection = combo->currentText();

    combo->blockSignals(true);
    combo->setModel(new SqlListModel(sqlQeury,tr("All"), this));
    combo->setCurrentText(currSelection);
    combo->blockSignals(false);
}
