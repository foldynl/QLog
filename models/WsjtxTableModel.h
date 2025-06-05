#ifndef QLOG_MODELS_WSJTXTABLEMODEL_H
#define QLOG_MODELS_WSJTXTABLEMODEL_H

#include <QAbstractTableModel>
#include "data/WsjtxEntry.h"

class WsjtxTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum column_id
    {
        COLUMN_CALLSIGN = 0,
        COLUMN_GRID = 1,
        COLUMN_DISTANCE = 2,
        COLUMN_SNR = 3,
        COLUMN_LAST_ACTIVITY = 4,
        COLUMN_LAST_MESSAGE = 5,
        COLUMN_MEMBER = 6,
    };

    WsjtxTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {spotPeriod = 120;}
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void addOrReplaceEntry(const WsjtxEntry &entry);
    void spotAging();
    bool callsignExists(const WsjtxEntry &);
    const WsjtxEntry getEntry(const QString &callsign) const;
    const WsjtxEntry getEntry(QModelIndex idx) const;
    void setCurrentSpotPeriod(float);
    void clear();
    void removeSpot(const QString &callsign);

private:
    QList<WsjtxEntry> wsjtxData;
    float spotPeriod;
};

#endif // QLOG_MODELS_WSJTXTABLEMODEL_H
