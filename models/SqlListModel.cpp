#include <QDebug>
#include <QFile>
#include <QSqlQuery>
#include "SqlListModel.h"

SqlListModel::SqlListModel(const QString &query, const QString &placeholder, QObject *parent)
    : QSqlQueryModel(parent),
      placeholder(placeholder),
      stmt(query)
{

    this->setQuery(stmt);
}

QVariant SqlListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    Q_UNUSED(role)
    return QVariant();
}

int SqlListModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;

    return QSqlQueryModel::rowCount(parent) + (!placeholder.isEmpty() ? 1 : 0);
}

QVariant SqlListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    const int col = index.column();
    const int row = index.row();
    const bool hasPlaceholder = !placeholder.isEmpty();
    const bool isTargetColumn = (col >= 0 && col <= 2);
    const int realRow = row - (hasPlaceholder ? 1 : 0);

    if (!isTargetColumn) return QVariant();

    if ( role == Qt::DisplayRole )
    {
        if ( hasPlaceholder && row == 0 ) return placeholder;

        const QModelIndex &sqlIndex = QSqlQueryModel::index(realRow, col);
        return QSqlQueryModel::data(sqlIndex, role);
    }

    // get row index;
    if (role == Qt::UserRole) return row - (hasPlaceholder ? 1 : 0);

    // get (role - UserRole) Column
    if (role >= Qt::UserRole + 1)
    {
        if (hasPlaceholder && row == 0) return QVariant();

        const QModelIndex &sqlIndex = QSqlQueryModel::index(realRow, role - 1 - Qt::UserRole);
        return QSqlQueryModel::data(sqlIndex, Qt::DisplayRole);
    }

    return QVariant();
}

void SqlListModel::refresh()
{
    setQuery(stmt);
}
