#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>
#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include "models/DxccTableModel.h"
#include "DxccTableWidget.h"
#include "core/debug.h"
#include "data/StationProfile.h"
#include "data/BandPlan.h"

MODULE_IDENTIFICATION("qlog.ui.dxcctablewidget");

DxccTableWidget::DxccTableWidget(QWidget *parent) : QTableView(parent)
{
    FCT_IDENTIFICATION;

    dxccTableModel = new DxccTableModel;

    this->setObjectName("dxccTableView");
    this->setModel(dxccTableModel);
    this->verticalHeader()->setVisible(false);
}

void DxccTableWidget::clear()
{
    FCT_IDENTIFICATION;

    dxccTableModel->clear();
    dxccTableModel->setQuery(QString());
    show();
}

void DxccTableWidget::updateDxTable(const QString &condition,
                                    const QVariant &conditionValue,
                                    const Band &highlightedBand)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << condition << conditionValue;

    const QList<Band>& dxccBands = BandPlan::bandsList(false, true);

    if ( dxccBands.isEmpty() )
        return;

    QString filter(QLatin1String("1 = 1"));
    StationProfile profile = StationProfilesManager::instance()->getCurProfile1();
    QStringList stmt_band_part1;
    QStringList stmt_band_part2;

    if ( profile != StationProfile() )
        filter.append(QString(" AND c.my_dxcc = %1").arg(profile.dxcc));

    for ( const Band &band : dxccBands )
    {
        stmt_band_part1 << QString(" MAX(CASE WHEN band = '%0' THEN  CASE WHEN (eqsl_qsl_rcvd = 'Y') THEN 2 ELSE 1 END  ELSE 0 END) as '%0_eqsl',"
                                   " MAX(CASE WHEN band = '%0' THEN  CASE WHEN (lotw_qsl_rcvd = 'Y') THEN 2 ELSE 1 END  ELSE 0 END) as '%0_lotw',"
                                   " MAX(CASE WHEN band = '%0' THEN  CASE WHEN (qsl_rcvd = 'Y')      THEN 2 ELSE 1 END  ELSE 0 END) as '%0_paper' ")
                                  .arg(band.name);
        stmt_band_part2 << QString(" c.'%0_eqsl' || c.'%0_lotw'|| c.'%0_paper' as '%0'").arg(band.name);
    }

    QString stmt = QString("WITH dxcc_summary AS "
                           "             ("
                           "			  SELECT  "
                           "			  m.dxcc , "
                           "		      %1 "
                           "		      FROM contacts c"
                           "		           LEFT OUTER JOIN modes m on c.mode = m.name"
                           "		      WHERE %2 AND %3 GROUP BY m.dxcc ) "
                           " SELECT m.dxcc,"
                           "	   %4 "
                           " FROM (SELECT DISTINCT dxcc"
                           "	   FROM modes) m"
                           "        LEFT OUTER JOIN dxcc_summary c ON c.dxcc = m.dxcc "
                           " ORDER BY m.dxcc").arg(stmt_band_part1.join(","),
                                                   filter,
                                                   condition.arg(conditionValue.toString()),
                                                   stmt_band_part2.join(","));

    qCDebug(runtime) << stmt;

    dxccTableModel->setQuery(stmt);

    // get default Brush from Mode column - Mode Column has always the default color
    const QVariant &defaultBrush = dxccTableModel->headerData(0, Qt::Horizontal, Qt::BackgroundRole);

    dxccTableModel->setHeaderData(0, Qt::Horizontal, tr("Mode"));

    for ( int i = 0; i < dxccBands.size(); i++ )
    {
        dxccTableModel->setHeaderData(i+1, Qt::Horizontal, ( highlightedBand == dxccBands.at(i) ) ? QBrush(Qt::darkGray)
                                                                                               : defaultBrush, Qt::BackgroundRole);
        dxccTableModel->setHeaderData(i+1, Qt::Horizontal, dxccBands.at(i).name);
    }
    setColumnWidth(0,65);
    show();
}

void DxccTableWidget::setDxCallsign(const QString &dxCallsign, const Band &band)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxCallsign;

    if (!dxCallsign.isEmpty())
        updateDxTable("c.callsign = '%1'", dxCallsign.toUpper(), band);
    else
        clear();

}

void DxccTableWidget::setDxcc(int dxcc, const Band &highlightedBand)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxcc;

    if ( dxcc )
        updateDxTable("c.dxcc = %1", dxcc, highlightedBand);
    else
        clear();
}
