#ifndef QLOG_UI_STATISTICSWIDGET_H
#define QLOG_UI_STATISTICSWIDGET_H

#include <QWidget>
#include <QSqlQuery>
#include <QPieSeries>
#include <QComboBox>
#include <QWebChannel>

#include "ui/MapWebChannelHandler.h"
#include "ui/WebEnginePage.h"
#include "core/LogLocale.h"

namespace Ui {
class StatisticsWidget;
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
using namespace QtCharts;
#endif

class StatisticsWidget : public QWidget
{
    Q_OBJECT

public slots:
    void mainStatChanged(int);
    void dateRangeCheckBoxChanged(int);
    void mapLoaded(bool);
    void changeTheme(int, bool isDark);
    void refreshWidget();

private slots:
    void refreshGraph();

public:
    explicit StatisticsWidget(QWidget *parent = nullptr);
    ~StatisticsWidget();

protected:
    bool event(QEvent *event) override;

private:

    void drawBarGraphs(const QString &title, QSqlQuery &query);
    void drawPieGraph(const QString &title, QPieSeries* series);
    void drawMyLocationsOnMap(QSqlQuery &);
    void drawPointsOnMap(QSqlQuery&);
    void drawFilledGridsOnMap(QSqlQuery&);
    void refreshCombos();
    void setSubTypesCombo(int mainTypeIdx);
    void refreshCombo(QComboBox * combo, const QString &sqlQeury);

private:
    Ui::StatisticsWidget *ui;
    WebEnginePage *main_page;
    bool isMainPageLoaded;
    QString postponedScripts;
    QWebChannel channel;
    MapWebChannelHandler layerControlHandler;
    LogLocale locale;
};

#endif // QLOG_UI_STATISTICSWIDGET_H
