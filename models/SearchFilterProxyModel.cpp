#include "SearchFilterProxyModel.h"

SearchFilterProxyModel::SearchFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{}

void SearchFilterProxyModel::setSearchString(const QString &searchString)
{
    this->searchString = searchString;
    invalidateFilter();
}

bool SearchFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    // full-text search
    for ( int col = 0; col < sourceModel()->columnCount(); ++col )
    {
        QModelIndex index = sourceModel()->index(source_row, col, source_parent);
        QString data = index.data(Qt::DisplayRole).toString();

        if ( data.contains(searchString, Qt::CaseInsensitive) )
            return true;
    }
    return false;
}