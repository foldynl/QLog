#include "AwardsTableModel.h"
#include <QColor>
#include <QFont>

AwardsTableModel::AwardsTableModel(QObject* parent) :
    QSqlQueryModel(parent)
{

}

QVariant AwardsTableModel::data(const QModelIndex &index, int role) const
{
    /* using hiden column 0 to identify type of information */
    /* 0 - Total Worked/Confirmed row
     * 1 - Confirmed row
     * 2 - Worked row
     * 3 - Per DXCC Entity row
     */
    int originRowType = QSqlQueryModel::data(this->index(index.row(), 0), Qt::DisplayRole).toInt();
    QVariant originCellValue = QSqlQueryModel::data(index, Qt::DisplayRole);
    int cellIntValue = originCellValue.toInt();

    if ( role == Qt::DisplayRole
         && (originRowType == 1 || originRowType == 2)
         && index.column() == 2 )
    {
        unsigned int count = 0;
        for ( int i = 3; i < columnCount(); i++ )
        {
            // EME is a propagation view of a QSO already counted in its band.
            if ( headerData(i, Qt::Horizontal).toString() == QLatin1String("EME") )
                continue;
            count += QSqlQueryModel::data(this->index(index.row(), i),
                                          Qt::DisplayRole).toInt();
        }
        return tr("Slots: ") + QString::number(count) + "  ";
    }

    if ( index.column() >= 3 )
    {
        switch (role)
        {
        case Qt::BackgroundRole:
            if ( originRowType >= 3 )
            {
                if ( cellIntValue > 1 )
                    return QColor(Qt::green);
                else if ( cellIntValue == 1 )
                    return QColor(255, 165, 0);
            }
            break;

        case Qt::ToolTipRole:
            if ( originRowType >= 3 )
            {
                return ( cellIntValue > 1 ) ? tr("Confirmed")
                                            : (cellIntValue == 1) ? tr("Worked")
                                                                  : tr("Still Waiting");
            }
            break;

        case Qt::DisplayRole:
            if ( originRowType >= 3 )
                return QString();
            break;

        case Qt::ForegroundRole:
            if ( originRowType >= 3 )
                return QColor(Qt::transparent);
            break;
        }
    }
    else if ( role == Qt::FontRole && originRowType <= 2 )
    {
        QFont font;
        font.setBold(true);
        return font;
    }

    return QSqlQueryModel::data(index, role);
}
