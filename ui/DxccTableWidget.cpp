#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>
#include <QDate>
#include <QSettings>
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

void DxccTableWidget::setDxcc(int dxcc, Band highlightedBand)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << dxcc;

    if ( dxcc )
    {
        const QList<Band>& dxccBands = BandPlan::bandsList(true, true);

        if ( dxccBands.size() == 0 )
        {
            return;
        }

        QString filter("1 = 1");
        StationProfile profile = StationProfilesManager::instance()->getCurProfile1();
        QStringList stmt_band_part1;
        QStringList stmt_band_part2;

        if ( profile != StationProfile() )
        {
            filter.append(QString(" AND c.my_dxcc = %1").arg(profile.dxcc));
        }

        for ( int i = 0; i < dxccBands.size(); i++ )
        {
            stmt_band_part1 << QString(" MAX(CASE WHEN band = '%1' THEN  CASE WHEN (eqsl_qsl_rcvd = 'Y') THEN 2 ELSE 1 END  ELSE 0 END) as '%2_eqsl',"
                                       " MAX(CASE WHEN band = '%3' THEN  CASE WHEN (lotw_qsl_rcvd = 'Y') THEN 2 ELSE 1 END  ELSE 0 END) as '%4_lotw',"
                                       " MAX(CASE WHEN band = '%5' THEN  CASE WHEN (qsl_rcvd = 'Y')      THEN 2 ELSE 1 END  ELSE 0 END) as '%6_paper' ")
                                        .arg(dxccBands[i].name, dxccBands[i].name,
                                             dxccBands[i].name, dxccBands[i].name,
                                             dxccBands[i].name, dxccBands[i].name);
            stmt_band_part2 << QString(" c.'%1_eqsl' || c.'%2_lotw'|| c.'%3_paper' as '%4'").arg(dxccBands[i].name, dxccBands[i].name,
                                                                                                 dxccBands[i].name, dxccBands[i].name);
        }

        QString stmt = QString("WITH dxcc_summary AS "
                               "             ("
                               "			  SELECT  "
                               "			  m.dxcc , "
                               "		      %1 "
                               "		      FROM contacts c"
                               "		           LEFT OUTER JOIN modes m on c.mode = m.name"
                               "		      WHERE %2 AND c.dxcc = %3 GROUP BY m.dxcc ) "
                               " SELECT m.dxcc,"
                               "	   %4 "
                               " FROM (SELECT DISTINCT dxcc"
                               "	   FROM modes) m"
                               "        LEFT OUTER JOIN dxcc_summary c ON c.dxcc = m.dxcc "
                               " ORDER BY m.dxcc").arg(stmt_band_part1.join(","))
                                                  .arg(filter)
                                                  .arg(dxcc)
                                                  .arg(stmt_band_part2.join(","));

        qCDebug(runtime) << stmt;

        dxccTableModel->setQuery(stmt);

        // get default Brush from Mode column - Mode Column has always the default color
        QVariant defaultBrush = dxccTableModel->headerData(0, Qt::Horizontal, Qt::BackgroundRole);

        dxccTableModel->setHeaderData(0, Qt::Horizontal, tr("Mode"));

        for ( int i = 0; i < dxccBands.size(); i++)
        {
            QVariant headerBrush;
            if ( highlightedBand == dxccBands[i] )
            {
                headerBrush = QBrush(Qt::darkGray);
            }
            else
            {
                headerBrush = defaultBrush;
            }
            dxccTableModel->setHeaderData(i+1, Qt::Horizontal, headerBrush, Qt::BackgroundRole);
            dxccTableModel->setHeaderData(i+1, Qt::Horizontal, dxccBands[i].name);
        }
    }
    else
    {
        dxccTableModel->clear();
    }

    show();
}
