#include "LogbookModel.h"
#include "data/Data.h"
#include "data/Dxcc.h"
#include "core/Gridsquare.h"
#include "core/Callsign.h"
#include "data/BandPlan.h"

#include <QIcon>

LogbookModel::LogbookModel(QObject* parent, QSqlDatabase db)
        : QSqlTableModel(parent, db)
{
    setTable("contacts");
    setEditStrategy(QSqlTableModel::OnFieldChange);
    setSort(COLUMN_TIME_ON, Qt::DescendingOrder);

    setHeaderData(COLUMN_ID, Qt::Horizontal, tr("QSO ID"));
    setHeaderData(COLUMN_TIME_ON, Qt::Horizontal, tr("Time on"));
    setHeaderData(COLUMN_TIME_OFF, Qt::Horizontal, tr("Time off"));
    setHeaderData(COLUMN_CALL, Qt::Horizontal, tr("Call"));
    setHeaderData(COLUMN_RST_SENT, Qt::Horizontal, tr("RSTs"));
    setHeaderData(COLUMN_RST_RCVD, Qt::Horizontal, tr("RSTr"));
    setHeaderData(COLUMN_FREQUENCY, Qt::Horizontal, tr("Frequency"));
    setHeaderData(COLUMN_BAND, Qt::Horizontal, tr("Band"));
    setHeaderData(COLUMN_MODE, Qt::Horizontal, tr("Mode"));
    setHeaderData(COLUMN_SUBMODE, Qt::Horizontal, tr("Submode"));
    setHeaderData(COLUMN_NAME, Qt::Horizontal, tr("Name (ASCII)"));
    setHeaderData(COLUMN_QTH, Qt::Horizontal, tr("QTH (ASCII)"));
    setHeaderData(COLUMN_GRID, Qt::Horizontal, tr("Gridsquare"));
    setHeaderData(COLUMN_DXCC, Qt::Horizontal, tr("DXCC"));
    setHeaderData(COLUMN_COUNTRY, Qt::Horizontal, tr("Country (ASCII)"));
    setHeaderData(COLUMN_CONTINENT, Qt::Horizontal, tr("Continent"));
    setHeaderData(COLUMN_CQZ, Qt::Horizontal, tr("CQZ"));
    setHeaderData(COLUMN_ITUZ, Qt::Horizontal, tr("ITU"));
    setHeaderData(COLUMN_PREFIX, Qt::Horizontal, tr("Prefix"));
    setHeaderData(COLUMN_STATE, Qt::Horizontal, tr("State"));
    setHeaderData(COLUMN_COUNTY, Qt::Horizontal, tr("County"));
    setHeaderData(COLUMN_IOTA, Qt::Horizontal, tr("IOTA"));
    setHeaderData(COLUMN_QSL_RCVD, Qt::Horizontal, tr("QSLr"));
    setHeaderData(COLUMN_QSL_RCVD_DATE, Qt::Horizontal, tr("QSLr Date"));
    setHeaderData(COLUMN_QSL_SENT, Qt::Horizontal, tr("QSLs"));
    setHeaderData(COLUMN_QSL_SENT_DATE, Qt::Horizontal, tr("QSLs Date"));
    setHeaderData(COLUMN_LOTW_RCVD, Qt::Horizontal, tr("LoTWr"));
    setHeaderData(COLUMN_LOTW_RCVD_DATE, Qt::Horizontal, tr("LoTWr Date"));
    setHeaderData(COLUMN_LOTW_SENT, Qt::Horizontal, tr("LoTWs"));
    setHeaderData(COLUMN_LOTW_SENT_DATE, Qt::Horizontal, tr("LoTWs Date"));
    setHeaderData(COLUMN_TX_POWER, Qt::Horizontal, tr("TX PWR"));
    setHeaderData(COLUMN_FIELDS, Qt::Horizontal, tr("Additional Fields"));
    setHeaderData(COLUMN_ADDRESS, Qt::Horizontal, tr("Address (ASCII)"));
    setHeaderData(COLUMN_ADDRESS_INTL, Qt::Horizontal, tr("Address"));
    setHeaderData(COLUMN_AGE, Qt::Horizontal, tr("Age"));
    setHeaderData(COLUMN_ALTITUDE, Qt::Horizontal, tr("Altitude"));
    setHeaderData(COLUMN_A_INDEX, Qt::Horizontal, tr("A-Index"));
    setHeaderData(COLUMN_ANT_AZ, Qt::Horizontal, tr("Antenna Az"));
    setHeaderData(COLUMN_ANT_EL, Qt::Horizontal, tr("Antenna El"));
    setHeaderData(COLUMN_ANT_PATH, Qt::Horizontal, tr("Signal Path"));
    setHeaderData(COLUMN_ARRL_SECT, Qt::Horizontal, tr("ARRL Section"));
    setHeaderData(COLUMN_AWARD_SUBMITTED, Qt::Horizontal, tr("Award Submitted"));
    setHeaderData(COLUMN_AWARD_GRANTED, Qt::Horizontal, tr("Award Granted"));
    setHeaderData(COLUMN_BAND_RX, Qt::Horizontal, tr("Band RX"));
    setHeaderData(COLUMN_GRID_EXT, Qt::Horizontal, tr("Gridsquare Extended"));
    setHeaderData(COLUMN_CHECK, Qt::Horizontal, tr("Contest Check"));
    setHeaderData(COLUMN_CLASS, Qt::Horizontal, tr("Class"));
    setHeaderData(COLUMN_CLUBLOG_QSO_UPLOAD_DATE, Qt::Horizontal, tr("ClubLog Upload Date"));
    setHeaderData(COLUMN_CLUBLOG_QSO_UPLOAD_STATUS, Qt::Horizontal, tr("ClubLog Upload State"));
    setHeaderData(COLUMN_COMMENT, Qt::Horizontal, tr("Comment (ASCII)"));
    setHeaderData(COLUMN_COMMENT_INTL, Qt::Horizontal, tr("Comment"));
    setHeaderData(COLUMN_CONTACTED_OP, Qt::Horizontal, tr("Contacted Operator"));
    setHeaderData(COLUMN_CONTEST_ID, Qt::Horizontal, tr("Contest ID"));
    setHeaderData(COLUMN_COUNTRY_INTL, Qt::Horizontal, tr("Country"));
    setHeaderData(COLUMN_CREDIT_SUBMITTED, Qt::Horizontal, tr("Credit Submitted"));
    setHeaderData(COLUMN_CREDIT_GRANTED, Qt::Horizontal, tr("Credit Granted"));
    setHeaderData(COLUMN_DARC_DOK, Qt::Horizontal, tr("DOK"));
    setHeaderData(COLUMN_DISTANCE, Qt::Horizontal, tr("Distance"));
    setHeaderData(COLUMN_EMAIL, Qt::Horizontal, tr("Email"));
    setHeaderData(COLUMN_EQ_CALL, Qt::Horizontal, tr("Owner Callsign"));
    setHeaderData(COLUMN_EQSL_QSLRDATE, Qt::Horizontal, tr("eQSLr Date"));
    setHeaderData(COLUMN_EQSL_QSLSDATE, Qt::Horizontal, tr("eQSLs Date"));
    setHeaderData(COLUMN_EQSL_QSL_RCVD, Qt::Horizontal, tr("eQSLr"));
    setHeaderData(COLUMN_EQSL_QSL_SENT, Qt::Horizontal, tr("eQSLs"));
    setHeaderData(COLUMN_FISTS, Qt::Horizontal, tr("FISTS Number"));
    setHeaderData(COLUMN_FISTS_CC, Qt::Horizontal, tr("FISTS CC"));
    setHeaderData(COLUMN_FORCE_INIT, Qt::Horizontal, tr("EME Init"));
    setHeaderData(COLUMN_FREQ_RX, Qt::Horizontal, tr("Frequency RX"));
    setHeaderData(COLUMN_GUEST_OP, Qt::Horizontal, tr("Guest Operator"));
    setHeaderData(COLUMN_HAMLOGEU_QSO_UPLOAD_DATE, Qt::Horizontal, tr("HamlogEU Upload Date"));
    setHeaderData(COLUMN_HAMLOGEU_QSO_UPLOAD_STATUS, Qt::Horizontal, tr("HamlogEU Upload Status"));
    setHeaderData(COLUMN_HAMQTH_QSO_UPLOAD_DATE, Qt::Horizontal, tr("HamQTH Upload Date"));
    setHeaderData(COLUMN_HAMQTH_QSO_UPLOAD_STATUS, Qt::Horizontal, tr("HamQTH Upload Status"));
    setHeaderData(COLUMN_HRDLOG_QSO_UPLOAD_DATE, Qt::Horizontal, tr("HRDLog Upload Date"));
    setHeaderData(COLUMN_HRDLOG_QSO_UPLOAD_STATUS, Qt::Horizontal, tr("HRDLog Upload Status"));
    setHeaderData(COLUMN_IOTA_ISLAND_ID, Qt::Horizontal, tr("IOTA Island ID"));
    setHeaderData(COLUMN_K_INDEX, Qt::Horizontal, tr("K-Index"));
    setHeaderData(COLUMN_LAT, Qt::Horizontal, tr("Latitude"));
    setHeaderData(COLUMN_LON, Qt::Horizontal, tr("Longitude"));
    setHeaderData(COLUMN_MAX_BURSTS, Qt::Horizontal, tr("Max Bursts"));
    setHeaderData(COLUMN_MS_SHOWER, Qt::Horizontal, tr("MS Shower Name"));
    setHeaderData(COLUMN_MY_ALTITUDE, Qt::Horizontal, tr("My Altitude"));
    setHeaderData(COLUMN_MY_ANTENNA, Qt::Horizontal, tr("My Antenna (ASCII)"));
    setHeaderData(COLUMN_MY_ANTENNA_INTL, Qt::Horizontal, tr("My Antenna"));
    setHeaderData(COLUMN_MY_CITY, Qt::Horizontal, tr("My City (ASCII)"));
    setHeaderData(COLUMN_MY_CITY_INTL, Qt::Horizontal, tr("My City"));
    setHeaderData(COLUMN_MY_CNTY, Qt::Horizontal, tr("My County"));
    setHeaderData(COLUMN_MY_COUNTRY, Qt::Horizontal, tr("My Country (ASCII)"));
    setHeaderData(COLUMN_MY_COUNTRY_INTL, Qt::Horizontal, tr("My Country"));
    setHeaderData(COLUMN_MY_CQ_ZONE, Qt::Horizontal, tr("My CQZ"));
    setHeaderData(COLUMN_MY_DXCC, Qt::Horizontal, tr("My DXCC"));
    setHeaderData(COLUMN_MY_FISTS, Qt::Horizontal, tr("My FISTS"));
    setHeaderData(COLUMN_MY_GRIDSQUARE, Qt::Horizontal, tr("My Gridsquare"));
    setHeaderData(COLUMN_MY_GRIDSQUARE_EXT, Qt::Horizontal, tr("My Gridsquare Extended"));
    setHeaderData(COLUMN_MY_IOTA, Qt::Horizontal, tr("My IOTA"));
    setHeaderData(COLUMN_MY_IOTA_ISLAND_ID, Qt::Horizontal, tr("My IOTA Island ID"));
    setHeaderData(COLUMN_MY_ITU_ZONE, Qt::Horizontal, tr("My ITU"));
    setHeaderData(COLUMN_MY_LAT, Qt::Horizontal, tr("My Latitude"));
    setHeaderData(COLUMN_MY_LON, Qt::Horizontal, tr("My Longitude"));
    setHeaderData(COLUMN_MY_NAME, Qt::Horizontal, tr("My Name (ASCII)"));
    setHeaderData(COLUMN_MY_NAME_INTL, Qt::Horizontal, tr("My Name"));
    setHeaderData(COLUMN_MY_POSTAL_CODE, Qt::Horizontal, tr("My Postal Code (ASCII)"));
    setHeaderData(COLUMN_MY_POSTAL_CODE_INTL, Qt::Horizontal, tr("My Postal Code"));
    setHeaderData(COLUMN_MY_POTA_REF, Qt::Horizontal, tr("My POTA Ref"));
    setHeaderData(COLUMN_MY_RIG, Qt::Horizontal, tr("My Rig (ASCII)"));
    setHeaderData(COLUMN_MY_RIG_INTL, Qt::Horizontal, tr("My Rig"));
    setHeaderData(COLUMN_MY_SIG, Qt::Horizontal, tr("My Special Interest Activity (ASCII)"));
    setHeaderData(COLUMN_MY_SIG_INTL, Qt::Horizontal, tr("My Special Interest Activity"));
    setHeaderData(COLUMN_MY_SIG_INFO, Qt::Horizontal, tr("My Spec. Interes Activity Info (ASCII)"));
    setHeaderData(COLUMN_MY_SIG_INFO_INTL, Qt::Horizontal, tr("My Spec. Interest Activity Info"));
    setHeaderData(COLUMN_MY_SOTA_REF, Qt::Horizontal, tr("My SOTA"));
    setHeaderData(COLUMN_MY_STATE, Qt::Horizontal, tr("My State"));
    setHeaderData(COLUMN_MY_STREET, Qt::Horizontal, tr("My Street"));
    setHeaderData(COLUMN_MY_STREET_INTL, Qt::Horizontal, tr("My Street"));
    setHeaderData(COLUMN_MY_USACA_COUNTIES, Qt::Horizontal, tr("My USA-CA Counties"));
    setHeaderData(COLUMN_MY_VUCC_GRIDS, Qt::Horizontal, tr("My VUCC Grids"));
    setHeaderData(COLUMN_NAME_INTL, Qt::Horizontal, tr("Name"));
    setHeaderData(COLUMN_NOTES, Qt::Horizontal, tr("Notes (ASCII)"));
    setHeaderData(COLUMN_NOTES_INTL, Qt::Horizontal, tr("Notes"));
    setHeaderData(COLUMN_NR_BURSTS, Qt::Horizontal, tr("#MS Bursts"));
    setHeaderData(COLUMN_NR_PINGS, Qt::Horizontal, tr("#MS Pings"));
    setHeaderData(COLUMN_OPERATOR, Qt::Horizontal, tr("Logging Operator"));
    setHeaderData(COLUMN_OWNER_CALLSIGN, Qt::Horizontal, tr("Owner Callsign"));
    setHeaderData(COLUMN_POTA_REF, Qt::Horizontal, tr("POTA"));
    setHeaderData(COLUMN_PRECEDENCE, Qt::Horizontal, tr("Contest Precedence"));
    setHeaderData(COLUMN_PROP_MODE, Qt::Horizontal, tr("Propagation Mode"));
    setHeaderData(COLUMN_PUBLIC_KEY, Qt::Horizontal, tr("Public Encryption Key"));
    setHeaderData(COLUMN_QRZCOM_QSO_UPLOAD_DATE, Qt::Horizontal, tr("QRZ Upload Date"));
    setHeaderData(COLUMN_QRZCOM_QSO_UPLOAD_STATUS, Qt::Horizontal, tr("QRZ Upload Status"));
    setHeaderData(COLUMN_QSLMSG, Qt::Horizontal, tr("QSL Message (ASCII)"));
    setHeaderData(COLUMN_QSLMSG_INTL, Qt::Horizontal, tr("QSL Message"));
    setHeaderData(COLUMN_QSL_RCVD_VIA, Qt::Horizontal, tr("QSLr Via"));
    setHeaderData(COLUMN_QSL_SENT_VIA, Qt::Horizontal, tr("QSLs Via"));
    setHeaderData(COLUMN_QSL_VIA, Qt::Horizontal, tr("QSL Via"));
    setHeaderData(COLUMN_QSO_COMPLETE, Qt::Horizontal, tr("QSO Completed"));
    setHeaderData(COLUMN_QSO_RANDOM, Qt::Horizontal, tr("QSO Random"));
    setHeaderData(COLUMN_QTH_INTL, Qt::Horizontal, tr("QTH"));
    setHeaderData(COLUMN_REGION, Qt::Horizontal, tr("Region"));
    setHeaderData(COLUMN_RIG, Qt::Horizontal, tr("Rig (ASCII)"));
    setHeaderData(COLUMN_RIG_INTL, Qt::Horizontal, tr("Rig"));
    setHeaderData(COLUMN_RX_PWR, Qt::Horizontal, tr("Contact PWR"));
    setHeaderData(COLUMN_SAT_MODE, Qt::Horizontal, tr("SAT Mode"));
    setHeaderData(COLUMN_SAT_NAME, Qt::Horizontal, tr("SAT Name"));
    setHeaderData(COLUMN_SFI, Qt::Horizontal, tr("Solar Flux"));
    setHeaderData(COLUMN_SIG, Qt::Horizontal, tr("SIG (ASCII)"));
    setHeaderData(COLUMN_SIG_INTL, Qt::Horizontal, tr("SIG"));
    setHeaderData(COLUMN_SIG_INFO, Qt::Horizontal, tr("SIG Info (ASCII)"));
    setHeaderData(COLUMN_SIG_INFO_INTL, Qt::Horizontal, tr("SIG Info"));
    setHeaderData(COLUMN_SILENT_KEY, Qt::Horizontal, tr("Silent Key"));
    setHeaderData(COLUMN_SKCC, Qt::Horizontal, tr("SKCC Member"));
    setHeaderData(COLUMN_SOTA_REF, Qt::Horizontal, tr("SOTA"));
    setHeaderData(COLUMN_SRX, Qt::Horizontal, tr("Contest Serial Number RX"));
    setHeaderData(COLUMN_SRX_STRING, Qt::Horizontal, tr("Contest Exchange RX"));
    setHeaderData(COLUMN_STATION_CALLSIGN, Qt::Horizontal, tr("Logging Station Callsign"));
    setHeaderData(COLUMN_STX, Qt::Horizontal, tr("Contest Serial Number TX"));
    setHeaderData(COLUMN_STX_STRING, Qt::Horizontal, tr("Contest Exchange TX"));
    setHeaderData(COLUMN_SWL, Qt::Horizontal, tr("SWL"));
    setHeaderData(COLUMN_TEN_TEN, Qt::Horizontal, tr("Ten-Ten Number"));
    setHeaderData(COLUMN_UKSMG, Qt::Horizontal, tr("UKSMG Member"));
    setHeaderData(COLUMN_USACA_COUNTIES, Qt::Horizontal, tr("USA-CA Counties"));
    setHeaderData(COLUMN_VE_PROV, Qt::Horizontal, tr("VE Prov"));
    setHeaderData(COLUMN_VUCC_GRIDS, Qt::Horizontal, tr("VUCC"));
    setHeaderData(COLUMN_WEB, Qt::Horizontal, tr("Web"));
    setHeaderData(COLUMN_MY_ARRL_SECT, Qt::Horizontal, tr("My ARRL Section"));
    setHeaderData(COLUMN_MY_WWFF_REF, Qt::Horizontal, tr("My WWFF"));
    setHeaderData(COLUMN_WWFF_REF, Qt::Horizontal, tr("WWFF"));
}

QVariant LogbookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DecorationRole && index.column() == COLUMN_CALL) {
        const QString &flag = Data::instance()->dxccFlag(QSqlTableModel::data(this->index(index.row(), COLUMN_DXCC), Qt::DisplayRole).toInt());

        return ( !flag.isEmpty() ) ? QIcon(QString(":/flags/16/%1.png").arg(flag))
                                   : QIcon(":/flags/16/unknown.png");
    }

    if (role == Qt::DecorationRole && (index.column() == COLUMN_QSL_RCVD || index.column() == COLUMN_QSL_SENT ||
                                       index.column() == COLUMN_LOTW_RCVD || index.column() == COLUMN_LOTW_SENT ||
                                       index.column() == COLUMN_EQSL_QSL_RCVD || index.column() == COLUMN_EQSL_QSL_SENT))
    {
        QVariant value = QSqlTableModel::data(index, Qt::DisplayRole);
        if (value.toString() == "Y") {
            return QIcon(":/icons/done-24px.svg");
        }
//        else {
//            return QIcon(":/icons/close-24px.svg");
//        }
    }

    if ( role == Qt::ToolTipRole && index.column() == COLUMN_CALL )
    {
        QString flag = Data::instance()->dxccFlag(QSqlTableModel::data(this->index(index.row(), COLUMN_DXCC), Qt::DisplayRole).toInt());

        return QString("<img src=':/flags/64/%1.png'>").arg(flag) +
               "<h2>" + QSqlTableModel::data(index, Qt::DisplayRole).toString() + "</h2>   " +
               "<table>" +
                " <tr>" +
                "   <td><b>" + tr("Country") + ": </b></td>" +
                "   <td>" + QCoreApplication::translate("DBStrings", QSqlTableModel::data(this->index(index.row(), COLUMN_COUNTRY), Qt::DisplayRole).toString().toUtf8().constData()) + "</td>" +
                " </tr>" +
               " <tr>" +
               "   <td><b>" + tr("Band") + ": </b></td>" +
               "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_BAND), Qt::DisplayRole).toString() + "</td>" +
               " </tr>" +
               " <tr>" +
                "   <td><b>" + tr("Mode") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_MODE), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
                " <tr>" +
                "   <td><b>" + tr("RST Sent") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_RST_SENT), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
                " <tr>" +
                "   <td><b>" + tr("RST Rcvd") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_RST_RCVD), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
                " <tr>" +
                "   <td><b>" + tr("Gridsquare") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_GRID), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
                " <tr>" +
                "   <td><b>" + tr("QSL Message") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_QSLMSG), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
                " <tr>" +
                "   <td><b>" + tr("Comment") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_COMMENT_INTL), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
                " <tr>" +
                "   <td><b>" + tr("Notes") + ": </b></td>" +
                "   <td>" + QSqlTableModel::data(this->index(index.row(), COLUMN_NOTES_INTL), Qt::DisplayRole).toString() + "</td>" +
                " </tr>" +
               "</table>" +
               "<br>" +
               "<table>" +
               "  <tr> " +
               "  <th></th><th>" + tr("Paper") + "</th><th>" + tr("LoTW") +"</th><th>" + tr("eQSL") +"</th>" +
               "  </tr>" +
               "  <tr> " +
               "  <td><b>" + tr("QSL Received") + "</b></td>" +
               QString("  <td><img src=':/icons/%1-24px.svg'></td>").arg((QSqlTableModel::data(this->index(index.row(),
                                                                                                           COLUMN_QSL_RCVD),
                                                                                               Qt::DisplayRole).toString() == "Y") ? "done" : "close") +
               QString("  <td><img src=':/icons/%1-24px.svg'></td>").arg((QSqlTableModel::data(this->index(index.row(),
                                                                                                           COLUMN_LOTW_RCVD),
                                                                                               Qt::DisplayRole).toString() == "Y") ? "done" : "close") +
               QString("  <td><img src=':/icons/%1-24px.svg'></td>").arg((QSqlTableModel::data(this->index(index.row(),
                                                                                                           COLUMN_EQSL_QSL_RCVD),
                                                                                               Qt::DisplayRole).toString() == "Y") ? "done" : "close") +
                "  </tr> " +
                "  <tr> " +
                "  <td><b>" + tr("QSL Sent") + "</b></td>" +
                QString("  <td><img src=':/icons/%1-24px.svg'></td>").arg((QSqlTableModel::data(this->index(index.row(),
                                                                                                            COLUMN_QSL_SENT),
                                                                                                Qt::DisplayRole).toString() == "Y") ? "done" : "close") +
                QString("  <td><img src=':/icons/%1-24px.svg'></td>").arg((QSqlTableModel::data(this->index(index.row(),
                                                                                                            COLUMN_LOTW_SENT),
                                                                                                Qt::DisplayRole).toString() == "Y") ? "done" : "close") +
                QString("  <td><img src=':/icons/%1-24px.svg'></td>").arg((QSqlTableModel::data(this->index(index.row(),
                                                                                                            COLUMN_EQSL_QSL_SENT),
                                                                                                Qt::DisplayRole).toString() == "Y") ? "done" : "close") +
                "  </tr> " +
               "</table>";
    }
    else if ( role == Qt::ToolTipRole && (index.column() == COLUMN_FIELDS
                                          || index.column() == COLUMN_NOTES
                                          || index.column() == COLUMN_NOTES_INTL) )
    {
        return QSqlTableModel::data(index, Qt::DisplayRole);
    }

    return QSqlTableModel::data(index, role);
}

bool LogbookModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool main_update_result = true;
    bool depend_update_result = true;

    if ( role == Qt::EditRole )
    {
        switch ( index.column() )
        {
        case COLUMN_TIME_ON:
        {
            QDateTime time_on = QSqlTableModel::data(this->index(index.row(), COLUMN_TIME_ON), Qt::DisplayRole).toDateTime();
            QDateTime time_off = QSqlTableModel::data(this->index(index.row(), COLUMN_TIME_OFF), Qt::DisplayRole).toDateTime();
            qint64 diff = time_on.secsTo(time_off);

            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_TIME_OFF), QVariant(value.toDateTime().addSecs(diff)), role);

            break;
        }

        case COLUMN_TIME_OFF:
        {
            QDateTime time_on = QSqlTableModel::data(this->index(index.row(), COLUMN_TIME_ON), Qt::DisplayRole).toDateTime();

            if ( value.toDateTime() < time_on )
            {
                depend_update_result = false;
            }
            break;
        }

        case COLUMN_CALL:
        {
            QString new_callsign = value.toString();
            DxccEntity dxccEntity = Data::instance()->lookupDxcc(new_callsign);
            if ( dxccEntity.dxcc )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COUNTRY), QVariant(dxccEntity.country),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_CQZ), QVariant(dxccEntity.cqz),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_ITUZ), QVariant(dxccEntity.ituz),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_DXCC), QVariant(dxccEntity.dxcc),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_CONTINENT), QVariant(dxccEntity.cont),role);
            }
            else
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COUNTRY), QVariant(QString()),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_CQZ), QVariant(QString()),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_ITUZ), QVariant(QString()),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_DXCC), QVariant(QString()),role);
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_CONTINENT), QVariant(QString()),role);
            }

            const QString &pfxRef = Callsign(new_callsign).getWPXPrefix();

            if ( !pfxRef.isEmpty() )
            {
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_PREFIX), QVariant(pfxRef), role);
            }
            break;
        }

        case COLUMN_FREQUENCY:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_BAND), QVariant(BandPlan::freq2Band(value.toDouble()).name), role );
            break;
        }

        case COLUMN_BAND:
        {
            double freq = QSqlTableModel::data(this->index(index.row(), COLUMN_FREQUENCY), Qt::DisplayRole).toDouble();
            depend_update_result = ( freq == 0.0 && !value.toString().isEmpty() );
            break;
        }

        case COLUMN_FREQ_RX:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_BAND_RX), QVariant(BandPlan::freq2Band(value.toDouble()).name), role );
            break;
        }

        case COLUMN_BAND_RX:
        {
            double freq = QSqlTableModel::data(this->index(index.row(), COLUMN_FREQ_RX), Qt::DisplayRole).toDouble();
            depend_update_result = ( freq == 0.0 && !value.toString().isEmpty() );
            break;
        }

        case COLUMN_GRID:
        {
            if ( ! value.toString().isEmpty() )
            {
                Gridsquare newgrid(value.toString());

                if ( newgrid.isValid() )
                {
                    Gridsquare mygrid(QSqlTableModel::data(this->index(index.row(), COLUMN_MY_GRIDSQUARE), Qt::DisplayRole).toString());
                    double distance;

                    if ( mygrid.distanceTo(newgrid, distance) )
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(distance),role);
                    }
                    else
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
                    }
                }
                else
                {
                    /* do not update field with invalid Grid */
                    depend_update_result = false;
                }
            }
            else
            {
                /* empty grid is valid (when removing a value); need to remove also Distance */
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
            }
            break;
        }

        case COLUMN_MY_GRIDSQUARE:
        {
            if ( ! value.toString().isEmpty() )
            {
                Gridsquare mynewGrid(value.toString());

                if ( mynewGrid.isValid() )
                {
                    Gridsquare dxgrid(QSqlTableModel::data(this->index(index.row(), COLUMN_GRID), Qt::DisplayRole).toString());
                    double distance;

                    if ( mynewGrid.distanceTo(dxgrid, distance) )
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(distance),role);
                    }
                    else
                    {
                        depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
                    }
                }
                else
                {
                    /* do not update field with invalid Grid */
                    depend_update_result = false;
                }
            }
            else
            {
                /* empty grid is valid (when removing a value); need to remove also Distance */
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_DISTANCE), QVariant(),role);
            }
            break;
        }

        case COLUMN_GRID_EXT:
        case COLUMN_MY_GRIDSQUARE_EXT:
        {
            if ( ! value.toString().isEmpty() )
            {
                QRegularExpressionMatch match = Gridsquare::gridExtRegEx().match(value.toString());

                if ( match.hasMatch() )
                {
                    depend_update_result = true;
                }
                else
                {
                    /* grid has an incorrect format */
                    depend_update_result = false;
                }
            }
            else
            {
                /* empty grid is valid (when removing a value) */
                depend_update_result = true;
            }
            break;
        }

        case COLUMN_SAT_MODE:
        case COLUMN_SAT_NAME:
        {
            if ( !value.toString().isEmpty() )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_PROP_MODE), "SAT");
            }
            break;
        }

        case COLUMN_PROP_MODE:
        {
            QString sat_mode = QSqlTableModel::data(this->index(index.row(), COLUMN_SAT_MODE), Qt::DisplayRole).toString();
            QString sat_name = QSqlTableModel::data(this->index(index.row(), COLUMN_SAT_NAME), Qt::DisplayRole).toString();

            /* If sat name or mode is not empty then do not allow to change propmode from SAT to any */
            if ( !sat_name.isEmpty() || !sat_mode.isEmpty() )
            {
                depend_update_result = false;
            }

            break;
        }

        case COLUMN_ID: /* it is the primary key, do not update */
        case COLUMN_COUNTRY:  /* it is a computed value, do not update */
        case COLUMN_DISTANCE: /* it is a computed value, do not update */
        {
            /* Do not allow to edit them */
            depend_update_result = false;
            break;
        }

        case COLUMN_ADDRESS_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ADDRESS), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_COMMENT_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COMMENT), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_COUNTRY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_COUNTRY), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_ANTENNA_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ANTENNA), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_CITY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_CITY), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_COUNTRY_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_NAME_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_NAME), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_POSTAL_CODE_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_POSTAL_CODE), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_RIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_RIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_SIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_SIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_SIG_INFO_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_SIG_INFO), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_MY_STREET_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_STREET), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_NAME_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_NAME), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_NOTES_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_NOTES), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_QSLMSG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_QSLMSG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_QTH_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_QTH), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_RIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_RIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_SIG_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_SIG), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_SIG_INFO_INTL:
        {
            depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_SIG_INFO), Data::removeAccents(value.toString()),role);
            break;
        }

        case COLUMN_SOTA_REF:
        {
            SOTAEntity sotaInfo = Data::instance()->lookupSOTA(value.toString());
            if ( sotaInfo.summitCode.toUpper() == value.toString().toUpper()
                 && !sotaInfo.summitName.isEmpty() )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ALTITUDE), sotaInfo.altm, role); // clazy:exclude=skipped-base-method
            }
            else
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_ALTITUDE), QVariant(), role); // clazy:exclude=skipped-base-method
            }
            break;
        }

        case COLUMN_MY_SOTA_REF:
        {
            SOTAEntity sotaInfo = Data::instance()->lookupSOTA(value.toString());
            if ( sotaInfo.summitCode.toUpper() == value.toString().toUpper()
                 && !sotaInfo.summitName.isEmpty() )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ALTITUDE), sotaInfo.altm, role); // clazy:exclude=skipped-base-method
            }
            else
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ALTITUDE), QVariant(), role); // clazy:exclude=skipped-base-method
            }
            break;
        }
        case COLUMN_STATION_CALLSIGN:
        {
            DxccEntity dxccEntity = Data::instance()->lookupDxcc(value.toString().toUpper());

            if ( dxccEntity.dxcc )
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_DXCC), dxccEntity.dxcc, role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ITU_ZONE), dxccEntity.ituz, role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_CQ_ZONE), dxccEntity.cqz, role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY_INTL), dxccEntity.country, role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY), Data::removeAccents(dxccEntity.country), role); // clazy:exclude=skipped-base-method
            }
            else
            {
                depend_update_result = QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY), QVariant(QString()),role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_CQ_ZONE), QVariant(QString()),role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_ITU_ZONE), QVariant(QString()),role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_DXCC), QVariant(QString()),role); // clazy:exclude=skipped-base-method
                depend_update_result = depend_update_result && QSqlTableModel::setData(this->index(index.row(), COLUMN_MY_COUNTRY_INTL), QVariant(QString()),role); // clazy:exclude=skipped-base-method
            }
            break;
        }

        }

        //updateExternalServicesUploadStatus(index, role, depend_update_result);

        if ( depend_update_result )
        {
            switch ( index.column() )
            {
            case COLUMN_FREQUENCY:
            case COLUMN_FREQ_RX:
            case COLUMN_TX_POWER:
                /* store NULL when 0.0MHz */
                main_update_result = QSqlTableModel::setData(index, ( value.toDouble() == 0.0 ) ? QVariant()
                                                                                                : value, role); // clazy:exclude=skipped-base-method
                break;

            case COLUMN_SOTA_REF:
            case COLUMN_MY_SOTA_REF:
            case COLUMN_IOTA:
            case COLUMN_MY_IOTA:
            case COLUMN_MY_GRIDSQUARE:
            case COLUMN_CALL:
            case COLUMN_GRID:
            case COLUMN_VUCC_GRIDS:
            case COLUMN_MY_VUCC_GRIDS:
            case COLUMN_MY_ARRL_SECT:
            case COLUMN_MY_WWFF_REF:
            case COLUMN_WWFF_REF:
            case COLUMN_MY_POTA_REF:
            case COLUMN_POTA_REF:
            case COLUMN_MY_GRIDSQUARE_EXT:
            case COLUMN_GRID_EXT:
            case COLUMN_STATION_CALLSIGN:
                main_update_result = QSqlTableModel::setData(index, ( !value.toString().isEmpty() ) ? value.toString().toUpper()
                                                                                                    : QVariant(), role);
                break;

            case COLUMN_ADDRESS_INTL:
            case COLUMN_COMMENT_INTL:
            case COLUMN_COUNTRY_INTL:
            case COLUMN_MY_ANTENNA_INTL:
            case COLUMN_MY_CITY_INTL:
            case COLUMN_MY_COUNTRY_INTL:
            case COLUMN_MY_NAME_INTL:
            case COLUMN_MY_POSTAL_CODE_INTL:
            case COLUMN_MY_RIG_INTL:
            case COLUMN_MY_SIG_INTL:
            case COLUMN_MY_SIG_INFO_INTL:
            case COLUMN_MY_STREET_INTL:
            case COLUMN_NAME_INTL:
            case COLUMN_NOTES_INTL:
            case COLUMN_QSLMSG_INTL:
            case COLUMN_QTH_INTL:
            case COLUMN_RIG_INTL:
            case COLUMN_SIG_INTL:
            case COLUMN_SIG_INFO_INTL:
                main_update_result = QSqlTableModel::setData(index, ( !value.toString().isEmpty() ) ? value
                                                                                                    : QVariant(), role);
                break;

            default:
                main_update_result = QSqlTableModel::setData(index, ( !value.toString().isEmpty() ) ? Data::removeAccents(value.toString())
                                                                                                    : QVariant(), role);
            }
        }
    }

    return main_update_result && depend_update_result;
}

#if 0
void LogbookModel::updateExternalServicesUploadStatus(const QModelIndex &index, int role, bool &updateResult)
{
    switch (index.column() )
    {
    case COLUMN_TIME_ON:
    case COLUMN_CALL:
    case COLUMN_FREQUENCY:
    case COLUMN_BAND:
    case COLUMN_PROP_MODE:
    case COLUMN_SAT_MODE:
    case COLUMN_SAT_NAME:
    case COLUMN_MODE:
    case COLUMN_SUBMODE:
    case COLUMN_STATION_CALLSIGN:
    case COLUMN_RST_RCVD:
    case COLUMN_RST_SENT:
    case COLUMN_QSL_RCVD:
    case COLUMN_QSL_SENT:
    case COLUMN_QSL_RCVD_DATE:
    case COLUMN_QSL_SENT_DATE:
    case COLUMN_DXCC:
    case COLUMN_CREDIT_GRANTED:
    case COLUMN_VUCC_GRIDS:
    case COLUMN_OPERATOR:
    case COLUMN_GRID:
    case COLUMN_NOTES:
        updateUploadToModified(index, role, COLUMN_CLUBLOG_QSO_UPLOAD_STATUS, updateResult);
        //updateUploadToModified(index, role, COLUMN_HRDLOG_QSO_UPLOAD_STATUS, updateResult);
        break;
    }

    /* QRZ consumes all ADIF Fields */
    updateUploadToModified(index, role, COLUMN_QRZCOM_QSO_UPLOAD_STATUS, updateResult);

    /* HRDLOG consumes all ADIF Fields */
    updateUploadToModified(index, role, COLUMN_HRDLOG_QSO_UPLOAD_STATUS, updateResult);
}

void LogbookModel::updateUploadToModified(const QModelIndex &index, int role, int column, bool &updateResult)
{
    QString status    = QSqlTableModel::data(this->index(index.row(), column), Qt::DisplayRole).toString();

    if ( status == "Y" )
    {
        updateResult = updateResult && QSqlTableModel::setData(this->index(index.row(), column), QVariant("M"), role);
    }
}
#endif
