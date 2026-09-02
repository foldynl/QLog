#ifndef QLOG_MODELS_ALERTTABLEMODEL_H
#define QLOG_MODELS_ALERTTABLEMODEL_H

#include <QAbstractTableModel>
#include <data/SpotAlert.h>
#include <QMutex>
#include "core/LogLocale.h"

class AlertTableModel : public QAbstractTableModel
{
    Q_OBJECT


public:

    enum column_id
    {
        COLUMN_RULENAME = 0,
        COLUMN_CALLSIGN = 1,
        COLUMN_FREQ = 2,
        COLUMN_MODE = 3,
        COLUMN_UPDATED = 4,
        COLUMN_LAST_UPDATE = 5,
        COLUMN_LAST_COMMENT = 6,
        COLUMN_MEMBER = 7,
    };

    struct AlertTableRecord
    {
        QStringList ruleName;
        long long counter;
        SpotAlert alert;

        AlertTableRecord() : counter(0){};

        bool operator==(const AlertTableRecord &) const;
        explicit AlertTableRecord(const SpotAlert&);
    };

    AlertTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent){};
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void addAlert(const SpotAlert &entry);
    bool containsAlert(const SpotAlert &alert);
    void clear();
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    const AlertTableRecord getTableRecord(const QModelIndex& index);
    void aging(const int clear_interval_sec);
    void resetDupe();
    void recalculateDupe();
    void updateSpotsStatusWhenQSOAdded(const QSqlRecord &record);
    void updateSpotsStatusWhenQSOUpdated(const QSqlRecord &);
    void updateSpotsStatusWhenQSODeleted(const QSqlRecord &record);
    void updateSpotsDxccStatus(const QSet<uint> &entities);
    void recalculateDxccStatus();

private:
    LogLocale locale;
    QList<AlertTableRecord> alertList;
    QMutex alertListMutex;
};

#endif // QLOG_MODELS_ALERTTABLEMODEL_H
