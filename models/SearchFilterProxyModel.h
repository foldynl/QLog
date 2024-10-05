#ifndef QLOG_MODELS_SEARCHFILTERPROXYMODEL_H
#define QLOG_MODELS_SEARCHFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class SearchFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SearchFilterProxyModel(QObject* parent = nullptr);
    void setSearchString(const QString& searchString);
    void setSearchSkippedCols(const QVector<int> &columns);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    QString searchString;
    QVector<int> searchSkippedCols;
};

#endif // QLOG_MODELS_SEARCHFILTERPROXYMODEL_H
