#include "LogbookModel.h"
#include "data/Data.h"
#include "data/Dxcc.h"
#include "data/Gridsquare.h"
#include "data/Callsign.h"
#include "data/BandPlan.h"

#include <QIcon>

LogbookModel::LogbookModel(QObject* parent, QSqlDatabase db)
        : QSqlTableModel(parent, db)
{
    setTable("contacts");
    setEditStrategy(QSqlTableModel::OnFieldChange);
    setSort(COLUMN_TIME_ON, Qt::DescendingOrder);

    for (auto it = fieldNameTranslationMap.begin(); it != fieldNameTranslationMap.end(); ++it)
        setHeaderData(it.key(), Qt::Horizontal, getFieldNameTranslation(it.key()));
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
                                       index.column() == COLUMN_EQSL_QSL_RCVD || index.column() == COLUMN_EQSL_QSL_SENT ||
                                       index.column() == COLUMN_DCL_QSL_RCVD || index.column() == COLUMN_DCL_QSL_SENT ))
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
    else if ( role == Qt::DisplayRole && (index.column() == COLUMN_COUNTRY_INTL
                                          || index.column() == COLUMN_MY_COUNTRY_INTL) )
    {
        return QCoreApplication::translate("DBStrings", QSqlTableModel::data(index, Qt::DisplayRole).toString().toUtf8().constData());
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
        case COLUMN_MY_COUNTRY:
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

QMap<LogbookModel::ColumnID, QString> LogbookModel::fieldNameTranslationMap =
{
    {COLUMN_ID, QT_TR_NOOP("QSO ID")},
    {COLUMN_TIME_ON, QT_TR_NOOP("Time on")},
    {COLUMN_TIME_OFF, QT_TR_NOOP("Time off")},
    {COLUMN_CALL, QT_TR_NOOP("Call")},
    {COLUMN_RST_SENT, QT_TR_NOOP("RSTs")},
    {COLUMN_RST_RCVD, QT_TR_NOOP("RSTr")},
    {COLUMN_FREQUENCY, QT_TR_NOOP("Frequency")},
    {COLUMN_BAND, QT_TR_NOOP("Band")},
    {COLUMN_MODE, QT_TR_NOOP("Mode")},
    {COLUMN_SUBMODE, QT_TR_NOOP("Submode")},
    {COLUMN_NAME, QT_TR_NOOP("Name (ASCII)")},
    {COLUMN_QTH, QT_TR_NOOP("QTH (ASCII)")},
    {COLUMN_GRID, QT_TR_NOOP("Gridsquare")},
    {COLUMN_DXCC, QT_TR_NOOP("DXCC")},
    {COLUMN_COUNTRY, QT_TR_NOOP("Country (ASCII)")},
    {COLUMN_CONTINENT, QT_TR_NOOP("Continent")},
    {COLUMN_CQZ, QT_TR_NOOP("CQZ")},
    {COLUMN_ITUZ, QT_TR_NOOP("ITU")},
    {COLUMN_PREFIX, QT_TR_NOOP("Prefix")},
    {COLUMN_STATE, QT_TR_NOOP("State")},
    {COLUMN_COUNTY, QT_TR_NOOP("County")},
    {COLUMN_CNTY_ALT, QT_TR_NOOP("County Alt")},
    {COLUMN_IOTA, QT_TR_NOOP("IOTA")},
    {COLUMN_QSL_RCVD, QT_TR_NOOP("QSLr")},
    {COLUMN_QSL_RCVD_DATE, QT_TR_NOOP("QSLr Date")},
    {COLUMN_QSL_SENT, QT_TR_NOOP("QSLs")},
    {COLUMN_QSL_SENT_DATE, QT_TR_NOOP("QSLs Date")},
    {COLUMN_LOTW_RCVD, QT_TR_NOOP("LoTWr")},
    {COLUMN_LOTW_RCVD_DATE, QT_TR_NOOP("LoTWr Date")},
    {COLUMN_LOTW_SENT, QT_TR_NOOP("LoTWs")},
    {COLUMN_LOTW_SENT_DATE, QT_TR_NOOP("LoTWs Date")},
    {COLUMN_TX_POWER, QT_TR_NOOP("TX PWR")},
    {COLUMN_FIELDS, QT_TR_NOOP("Additional Fields")},
    {COLUMN_ADDRESS, QT_TR_NOOP("Address (ASCII)")},
    {COLUMN_ADDRESS_INTL, QT_TR_NOOP("Address")},
    {COLUMN_AGE, QT_TR_NOOP("Age")},
    {COLUMN_ALTITUDE, QT_TR_NOOP("Altitude")},
    {COLUMN_A_INDEX, QT_TR_NOOP("A-Index")},
    {COLUMN_ANT_AZ, QT_TR_NOOP("Antenna Az")},
    {COLUMN_ANT_EL, QT_TR_NOOP("Antenna El")},
    {COLUMN_ANT_PATH, QT_TR_NOOP("Signal Path")},
    {COLUMN_ARRL_SECT, QT_TR_NOOP("ARRL Section")},
    {COLUMN_AWARD_SUBMITTED, QT_TR_NOOP("Award Submitted")},
    {COLUMN_AWARD_GRANTED, QT_TR_NOOP("Award Granted")},
    {COLUMN_BAND_RX, QT_TR_NOOP("Band RX")},
    {COLUMN_GRID_EXT, QT_TR_NOOP("Gridsquare Extended")},
    {COLUMN_CHECK, QT_TR_NOOP("Contest Check")},
    {COLUMN_CLASS, QT_TR_NOOP("Class")},
    {COLUMN_CLUBLOG_QSO_UPLOAD_DATE, QT_TR_NOOP("ClubLog Upload Date")},
    {COLUMN_CLUBLOG_QSO_UPLOAD_STATUS, QT_TR_NOOP("ClubLog Upload State")},
    {COLUMN_COMMENT, QT_TR_NOOP("Comment (ASCII)")},
    {COLUMN_COMMENT_INTL, QT_TR_NOOP("Comment")},
    {COLUMN_CONTACTED_OP, QT_TR_NOOP("Contacted Operator")},
    {COLUMN_CONTEST_ID, QT_TR_NOOP("Contest ID")},
    {COLUMN_COUNTRY_INTL, QT_TR_NOOP("Country")},
    {COLUMN_CREDIT_SUBMITTED, QT_TR_NOOP("Credit Submitted")},
    {COLUMN_CREDIT_GRANTED, QT_TR_NOOP("Credit Granted")},
    {COLUMN_DARC_DOK, QT_TR_NOOP("DOK")},
    {COLUMN_DCL_QSLRDATE, QT_TR_NOOP("DCLr Date")},
    {COLUMN_DCL_QSLSDATE, QT_TR_NOOP("DCLs Date")},
    {COLUMN_DCL_QSL_RCVD, QT_TR_NOOP("DCLr")},
    {COLUMN_DCL_QSL_SENT, QT_TR_NOOP("DCLs")},
    {COLUMN_DISTANCE, QT_TR_NOOP("Distance")},
    {COLUMN_EMAIL, QT_TR_NOOP("Email")},
    {COLUMN_EQ_CALL, QT_TR_NOOP("Owner Callsign")},
    {COLUMN_EQSL_AG, QT_TR_NOOP("eQSL AG")},
    {COLUMN_EQSL_QSLRDATE, QT_TR_NOOP("eQSLr Date")},
    {COLUMN_EQSL_QSLSDATE, QT_TR_NOOP("eQSLs Date")},
    {COLUMN_EQSL_QSL_RCVD, QT_TR_NOOP("eQSLr")},
    {COLUMN_EQSL_QSL_SENT, QT_TR_NOOP("eQSLs")},
    {COLUMN_FISTS, QT_TR_NOOP("FISTS Number")},
    {COLUMN_FISTS_CC, QT_TR_NOOP("FISTS CC")},
    {COLUMN_FORCE_INIT, QT_TR_NOOP("EME Init")},
    {COLUMN_FREQ_RX, QT_TR_NOOP("Frequency RX")},
    {COLUMN_GUEST_OP, QT_TR_NOOP("Guest Operator")},
    {COLUMN_HAMLOGEU_QSO_UPLOAD_DATE, QT_TR_NOOP("HamlogEU Upload Date")},
    {COLUMN_HAMLOGEU_QSO_UPLOAD_STATUS, QT_TR_NOOP("HamlogEU Upload Status")},
    {COLUMN_HAMQTH_QSO_UPLOAD_DATE, QT_TR_NOOP("HamQTH Upload Date")},
    {COLUMN_HAMQTH_QSO_UPLOAD_STATUS, QT_TR_NOOP("HamQTH Upload Status")},
    {COLUMN_HRDLOG_QSO_UPLOAD_DATE, QT_TR_NOOP("HRDLog Upload Date")},
    {COLUMN_HRDLOG_QSO_UPLOAD_STATUS, QT_TR_NOOP("HRDLog Upload Status")},
    {COLUMN_IOTA_ISLAND_ID, QT_TR_NOOP("IOTA Island ID")},
    {COLUMN_K_INDEX, QT_TR_NOOP("K-Index")},
    {COLUMN_LAT, QT_TR_NOOP("Latitude")},
    {COLUMN_LON, QT_TR_NOOP("Longitude")},
    {COLUMN_MAX_BURSTS, QT_TR_NOOP("Max Bursts")},
    {COLUMN_MORSE_KEY_INFO, QT_TR_NOOP("CW Key Info")},
    {COLUMN_MORSE_KEY_TYPE, QT_TR_NOOP("CW Key Type")},
    {COLUMN_MS_SHOWER, QT_TR_NOOP("MS Shower Name")},
    {COLUMN_MY_ALTITUDE, QT_TR_NOOP("My Altitude")},
    {COLUMN_MY_ANTENNA, QT_TR_NOOP("My Antenna (ASCII)")},
    {COLUMN_MY_ANTENNA_INTL, QT_TR_NOOP("My Antenna")},
    {COLUMN_MY_CITY, QT_TR_NOOP("My City (ASCII)")},
    {COLUMN_MY_CITY_INTL, QT_TR_NOOP("My City")},
    {COLUMN_MY_CNTY, QT_TR_NOOP("My County")},
    {COLUMN_MY_CNTY_ALT, QT_TR_NOOP("My County Alt")},
    {COLUMN_MY_COUNTRY, QT_TR_NOOP("My Country (ASCII)")},
    {COLUMN_MY_COUNTRY_INTL, QT_TR_NOOP("My Country")},
    {COLUMN_MY_CQ_ZONE, QT_TR_NOOP("My CQZ")},
    {COLUMN_MY_DARC_DOK, QT_TR_NOOP("My DARC DOK")},
    {COLUMN_MY_DXCC, QT_TR_NOOP("My DXCC")},
    {COLUMN_MY_FISTS, QT_TR_NOOP("My FISTS")},
    {COLUMN_MY_GRIDSQUARE, QT_TR_NOOP("My Gridsquare")},
    {COLUMN_MY_GRIDSQUARE_EXT, QT_TR_NOOP("My Gridsquare Extended")},
    {COLUMN_MY_IOTA, QT_TR_NOOP("My IOTA")},
    {COLUMN_MY_IOTA_ISLAND_ID, QT_TR_NOOP("My IOTA Island ID")},
    {COLUMN_MY_ITU_ZONE, QT_TR_NOOP("My ITU")},
    {COLUMN_MY_LAT, QT_TR_NOOP("My Latitude")},
    {COLUMN_MY_LON, QT_TR_NOOP("My Longitude")},
    {COLUMN_MY_MORSE_KEY_INFO, QT_TR_NOOP("My CW Key Info")},
    {COLUMN_MY_MORSE_KEY_TYPE, QT_TR_NOOP("My CW Key Type")},
    {COLUMN_MY_NAME, QT_TR_NOOP("My Name (ASCII)")},
    {COLUMN_MY_NAME_INTL, QT_TR_NOOP("My Name")},
    {COLUMN_MY_POSTAL_CODE, QT_TR_NOOP("My Postal Code (ASCII)")},
    {COLUMN_MY_POSTAL_CODE_INTL, QT_TR_NOOP("My Postal Code")},
    {COLUMN_MY_POTA_REF, QT_TR_NOOP("My POTA Ref")},
    {COLUMN_MY_RIG, QT_TR_NOOP("My Rig (ASCII)")},
    {COLUMN_MY_RIG_INTL, QT_TR_NOOP("My Rig")},
    {COLUMN_MY_SIG, QT_TR_NOOP("My Special Interest Activity (ASCII)")},
    {COLUMN_MY_SIG_INTL, QT_TR_NOOP("My Special Interest Activity")},
    {COLUMN_MY_SIG_INFO, QT_TR_NOOP("My Spec. Interes Activity Info (ASCII)")},
    {COLUMN_MY_SIG_INFO_INTL, QT_TR_NOOP("My Spec. Interest Activity Info")},
    {COLUMN_MY_SOTA_REF, QT_TR_NOOP("My SOTA")},
    {COLUMN_MY_STATE, QT_TR_NOOP("My State")},
    {COLUMN_MY_STREET, QT_TR_NOOP("My Street")},
    {COLUMN_MY_STREET_INTL, QT_TR_NOOP("My Street")},
    {COLUMN_MY_USACA_COUNTIES, QT_TR_NOOP("My USA-CA Counties")},
    {COLUMN_MY_VUCC_GRIDS, QT_TR_NOOP("My VUCC Grids")},
    {COLUMN_NAME_INTL, QT_TR_NOOP("Name")},
    {COLUMN_NOTES, QT_TR_NOOP("Notes (ASCII)")},
    {COLUMN_NOTES_INTL, QT_TR_NOOP("Notes")},
    {COLUMN_NR_BURSTS, QT_TR_NOOP("#MS Bursts")},
    {COLUMN_NR_PINGS, QT_TR_NOOP("#MS Pings")},
    {COLUMN_OPERATOR, QT_TR_NOOP("Operator Callsign")},
    {COLUMN_OWNER_CALLSIGN, QT_TR_NOOP("Owner Callsign")},
    {COLUMN_POTA_REF, QT_TR_NOOP("POTA")},
    {COLUMN_PRECEDENCE, QT_TR_NOOP("Contest Precedence")},
    {COLUMN_PROP_MODE, QT_TR_NOOP("Propagation Mode")},
    {COLUMN_PUBLIC_KEY, QT_TR_NOOP("Public Encryption Key")},
    {COLUMN_QRZCOM_QSO_DOWNLOAD_DATE, QT_TR_NOOP("QRZ Download Date")},
    {COLUMN_QRZCOM_QSO_DOWNLOAD_STATUS, QT_TR_NOOP("QRZ Download Status")},
    {COLUMN_QRZCOM_QSO_UPLOAD_DATE, QT_TR_NOOP("QRZ Upload Date")},
    {COLUMN_QRZCOM_QSO_UPLOAD_STATUS, QT_TR_NOOP("QRZ Upload Status")},
    {COLUMN_QSLMSG, QT_TR_NOOP("QSLs Message (ASCII)")},
    {COLUMN_QSLMSG_INTL, QT_TR_NOOP("QSLs Message")},
    {COLUMN_QSLMSG_RCVD, QT_TR_NOOP("QSLr Message")},
    {COLUMN_QSL_RCVD_VIA, QT_TR_NOOP("QSLr Via")},
    {COLUMN_QSL_SENT_VIA, QT_TR_NOOP("QSLs Via")},
    {COLUMN_QSL_VIA, QT_TR_NOOP("QSL Via")},
    {COLUMN_QSO_COMPLETE, QT_TR_NOOP("QSO Completed")},
    {COLUMN_QSO_RANDOM, QT_TR_NOOP("QSO Random")},
    {COLUMN_QTH_INTL, QT_TR_NOOP("QTH")},
    {COLUMN_REGION, QT_TR_NOOP("Region")},
    {COLUMN_RIG, QT_TR_NOOP("Rig (ASCII)")},
    {COLUMN_RIG_INTL, QT_TR_NOOP("Rig")},
    {COLUMN_RX_PWR, QT_TR_NOOP("RcvPWR")},
    {COLUMN_SAT_MODE, QT_TR_NOOP("SAT Mode")},
    {COLUMN_SAT_NAME, QT_TR_NOOP("SAT Name")},
    {COLUMN_SFI, QT_TR_NOOP("Solar Flux")},
    {COLUMN_SIG, QT_TR_NOOP("SIG (ASCII)")},
    {COLUMN_SIG_INTL, QT_TR_NOOP("SIG")},
    {COLUMN_SIG_INFO, QT_TR_NOOP("SIG Info (ASCII)")},
    {COLUMN_SIG_INFO_INTL, QT_TR_NOOP("SIG Info")},
    {COLUMN_SILENT_KEY, QT_TR_NOOP("Silent Key")},
    {COLUMN_SKCC, QT_TR_NOOP("SKCC Member")},
    {COLUMN_SOTA_REF, QT_TR_NOOP("SOTA")},
    {COLUMN_SRX, QT_TR_NOOP("RcvNr")},
    {COLUMN_SRX_STRING, QT_TR_NOOP("RcvExch")},
    {COLUMN_STATION_CALLSIGN, QT_TR_NOOP("Logging Station Callsign")},
    {COLUMN_STX, QT_TR_NOOP("SentNr")},
    {COLUMN_STX_STRING, QT_TR_NOOP("SentExch")},
    {COLUMN_SWL, QT_TR_NOOP("SWL")},
    {COLUMN_TEN_TEN, QT_TR_NOOP("Ten-Ten Number")},
    {COLUMN_UKSMG, QT_TR_NOOP("UKSMG Member")},
    {COLUMN_USACA_COUNTIES, QT_TR_NOOP("USA-CA Counties")},
    {COLUMN_VE_PROV, QT_TR_NOOP("VE Prov")},
    {COLUMN_VUCC_GRIDS, QT_TR_NOOP("VUCC")},
    {COLUMN_WEB, QT_TR_NOOP("Web")},
    {COLUMN_MY_ARRL_SECT, QT_TR_NOOP("My ARRL Section")},
    {COLUMN_MY_WWFF_REF, QT_TR_NOOP("My WWFF")},
    {COLUMN_WWFF_REF, QT_TR_NOOP("WWFF")}
};


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
