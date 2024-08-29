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

     ui->statTypeSecCombo->blockSignals(true);

     ui->statTypeSecCombo->clear();

     switch ( idx )
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
     }
     break;
     }

     if ( idx == 4 )
     {
         ui->lotwCheckBox->setEnabled(true);
         ui->eqslCheckBox->setEnabled(true);
         ui->paperCheckBox->setEnabled(true);
     }
     else
     {
         ui->lotwCheckBox->setEnabled(false);
         ui->eqslCheckBox->setEnabled(false);
         ui->paperCheckBox->setEnabled(false);
     }

     ui->statTypeSecCombo->blockSignals(false);

     refreshGraph();
}

void StatisticsWidget::refreshGraph()
{
     FCT_IDENTIFICATION;

     QStringList genericFilter;

     genericFilter << " 1 = 1 "; //just initialization - use only in case of empty Options

     refreshCallCombo();
     refreshRigCombo();
     refreshAntCombo();
     refreshBandCombo();
     refreshGridCombo();

     if ( ui->myCallCombo->currentIndex() != 0 )
     {
         genericFilter << " (station_callsign = '" + ui->myCallCombo->currentText() + "') ";
     }

     if ( ui->myGridCombo->currentIndex() != 0 )
     {
         if ( ui->myGridCombo->currentText().isEmpty() )
         {
             genericFilter << " (my_gridsquare is NULL) ";
         }
         else
         {
             genericFilter << " (my_gridsquare = '" + ui->myGridCombo->currentText() + "') ";
         }
     }

     if ( ui->myRigCombo->currentIndex() != 0 )
     {
         if ( ui->myRigCombo->currentText().isEmpty() )
         {
             genericFilter << " (my_rig is NULL) ";
         }
         else
         {
             genericFilter << " (my_rig = '" + ui->myRigCombo->currentText() + "') ";
         }
     }

     if ( ui->myAntennaCombo->currentIndex() != 0 )
     {
         if ( ui->myAntennaCombo->currentText().isEmpty() )
         {
             genericFilter << " (my_antenna is NULL) ";
         }
         else
         {
             genericFilter << " (my_antenna = '" + ui->myAntennaCombo->currentText() + "') ";
         }
     }

     if ( ui->bandCombo->currentIndex() != 0 )
     {
         if ( ! ui->bandCombo->currentText().isEmpty() )
         {
             genericFilter << " (band = '" + ui->bandCombo->currentText() + "') ";
         }
     }

     if ( ui->useDateRangeCheckBox->isChecked() )
     {
         genericFilter << " (date(start_time) BETWEEN date('" + ui->startDateEdit->date().toString("yyyy-MM-dd")
                          + " 00:00:00') AND date('" + ui->endDateEdit->date().toString("yyyy-MM-dd") + " 23:59:59') ) ";
     }

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
         {
             confirmed << " eqsl_qsl_rcvd = 'Y' ";
         }

         if ( ui->lotwCheckBox->isChecked() )
         {
             confirmed << " lotw_qsl_rcvd = 'Y' ";
         }

         if ( ui->paperCheckBox->isChecked() )
         {
             confirmed << " qsl_rcvd = 'Y' ";
         }

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
             stmt = "SELECT callsign, gridsquare, SUM(confirmed) FROM (SELECT callsign, gridsquare, "
                        + innerCase +" AS confirmed FROM contacts WHERE gridsquare is not NULL AND "
                        + genericFilter.join(" AND ") +" ) GROUP BY callsign, gridsquare";
             break;
         case 2:
             QString unit;
             Gridsquare::distance2localeUnitDistance(0, unit);
             QString distCoef = QString::number(Gridsquare::localeDistanceCoef());
             QString sel = QString("SELECT callsign || '<br>' || CAST(ROUND(distance * %1,0) AS INT) || ' %2', gridsquare, ").arg(distCoef, unit);

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
    postponedScripts += layerControlHandler.generateMapMenuJS();
    main_page->runJavaScript(postponedScripts);

    layerControlHandler.restoreLayerControlStates(main_page);
}

void StatisticsWidget::changeTheme(int theme)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << theme;

    QString themeJavaScript;

    if ( theme == 1 ) /* dark mode */
    {
        themeJavaScript = "map.getPanes().tilePane.style.webkitFilter=\"brightness(0.6) invert(1) contrast(3) hue-rotate(200deg) saturate(0.3) brightness(0.9)\";";
    }
    else
    {
        themeJavaScript = "map.getPanes().tilePane.style.webkitFilter=\"\";";
    }

    if ( !isMainPageLoaded )
    {
        postponedScripts.append(themeJavaScript);
    }
    else
    {
        main_page->runJavaScript(themeJavaScript);
    }
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
    refreshCallCombo();

    ui->myGridCombo->setModel(new QStringListModel(this));
    refreshGridCombo();

    ui->myRigCombo->setModel(new QStringListModel(this));
    refreshRigCombo();
    ui->myRigCombo->setCurrentIndex(0);

    ui->myAntennaCombo->setModel(new QStringListModel(this));
    refreshAntCombo();
    ui->myAntennaCombo->setCurrentIndex(0);

    ui->bandCombo->setModel(new QStringListModel(this));
    refreshBandCombo();

    ui->startDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->startDateEdit->setDate(QDate::currentDate().addDays(DEFAULT_STAT_RANGE));
    ui->endDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->endDateEdit->setDate(QDate::currentDate());

    ui->graphView->setRenderHint(QPainter::Antialiasing);
    ui->graphView->setChart(new QChart());

    main_page->setWebChannel(&channel);
    ui->mapView->setPage(main_page);
    main_page->load(QUrl(QStringLiteral("qrc:/res/map/onlinemap.html")));
    ui->mapView->setFocusPolicy(Qt::ClickFocus);
    connect(ui->mapView, &QWebEngineView::loadFinished, this, &StatisticsWidget::mapLoaded);
    channel.registerObject("layerControlHandler", &layerControlHandler);

    mainStatChanged(0);
}

StatisticsWidget::~StatisticsWidget()
{
    FCT_IDENTIFICATION;
    main_page->deleteLater();
    delete ui;
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
    {
        chart->deleteLater();
    }

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
    {
        chart->deleteLater();
    }


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


    if ( query.lastQuery().isEmpty() ) return;

    QList<QString> locations;

    while ( query.next() )
    {
        QString loc = query.value(0).toString();
        Gridsquare stationGrid(loc);

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
    {
        postponedScripts.append(javaScript);
    }
    else
    {
        main_page->runJavaScript(javaScript);
    }
}

void StatisticsWidget::drawPointsOnMap(QSqlQuery &query)
{
    FCT_IDENTIFICATION;


    if ( query.lastQuery().isEmpty() ) return;

    QList<QString> stations;

    while ( query.next() )
    {
        Gridsquare stationGrid(query.value(1).toString());

        if ( stationGrid.isValid() )
        {
            double lat = stationGrid.getLatitude();
            double lon = stationGrid.getLongitude();
            stations.append(QString("[\"%1\", %2, %3, %4]").arg(query.value(0).toString())
                                                           .arg(lat)
                                                           .arg(lon)
                                                           .arg((query.value(2).toInt()) > 0 ? "greenIcon" : "yellowIcon"));
        }
    }

    QString javaScript = QString("grids_confirmed = [];"
                                 "grids_worked = [];"
                                 "drawPoints([%1]);"
                                 "maidenheadConfWorked.redraw();").arg(stations.join(","));

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
    {
        postponedScripts.append(javaScript);
    }
    else
    {
        main_page->runJavaScript(javaScript);
    }
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
        {
            confirmedGrids << QString("\"" + query.value(1).toString() + "\"");
        }
        else
        {
            workedGrids << QString("\"" + query.value(1).toString() + "\"");
        }
    }

    QString javaScript = QString("grids_confirmed = [ %1 ]; "
                                 "grids_worked = [ %2 ];"
                                 "mylocations = [];"
                                 "drawPoints([]);"
                                 "maidenheadConfWorked.redraw();").arg(confirmedGrids.join(","), workedGrids.join(","));

    qCDebug(runtime) << javaScript;

    if ( !isMainPageLoaded )
    {
        postponedScripts.append(javaScript);
    }
    else
    {
        main_page->runJavaScript(javaScript);
    }
}

void StatisticsWidget::refreshCallCombo()
{
    FCT_IDENTIFICATION;

    refreshCombo(ui->myCallCombo, "SELECT DISTINCT UPPER(station_callsign) FROM contacts ORDER BY station_callsign");
}

void StatisticsWidget::refreshRigCombo()
{
    FCT_IDENTIFICATION;

    refreshCombo(ui->myRigCombo, "SELECT DISTINCT my_rig FROM contacts ORDER BY my_rig");
}

void StatisticsWidget::refreshAntCombo()
{
    FCT_IDENTIFICATION;

    refreshCombo(ui->myAntennaCombo, "SELECT DISTINCT my_antenna FROM contacts ORDER BY my_antenna");
}

void StatisticsWidget::refreshBandCombo()
{
    FCT_IDENTIFICATION;

    refreshCombo(ui->bandCombo, "SELECT DISTINCT band FROM contacts c, bands b WHERE c.band = b.name ORDER BY b.start_freq;");
}

void StatisticsWidget::refreshGridCombo()
{
    FCT_IDENTIFICATION;

    refreshCombo(ui->myGridCombo, "SELECT DISTINCT UPPER(my_gridsquare) FROM contacts ORDER BY my_gridsquare");
}

void StatisticsWidget::refreshCombo(QComboBox * combo, QString sqlQeury)
{
    FCT_IDENTIFICATION;

    QString currSelection = combo->currentText();

    combo->blockSignals(true);
    //combo->clear();

    combo->setModel(new SqlListModel(sqlQeury,tr("All"), this));

    combo->setCurrentText(currSelection);
    combo->blockSignals(false);

}
