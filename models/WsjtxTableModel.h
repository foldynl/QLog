#ifndef QLOG_MODELS_WSJTXTABLEMODEL_H
#define QLOG_MODELS_WSJTXTABLEMODEL_H

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include "data/WsjtxEntry.h"
#include "models/WsjtxActivity.h"

class WsjtxTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum column_id
    {
        COLUMN_CALLSIGN = 0,
        COLUMN_GRID = 1,
        COLUMN_DISTANCE = 2,
        COLUMN_SNR = 3,
        COLUMN_ACTIVITY = 4,
        COLUMN_LAST_MESSAGE = 5,
        COLUMN_MEMBER = 6,
        COLUMN_FREQUENCY = 7,
    };

    enum data_role
    {
        ACTIVITY_HISTORY_ROLE = Qt::UserRole + 1
    };

    WsjtxTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void addOrReplaceEntry(const WsjtxEntry &entry);
    void spotAging();
    bool callsignExists(const WsjtxEntry &);
    const WsjtxEntry getEntry(const QString &id, const QString &callsign) const;
    const WsjtxEntry getEntry(QModelIndex idx) const;
    void clear();
    void removeSource(const QString &id);
    void removeSpot(const QString &callsign);
    void refreshStatusColors();
    void recalculateDxccStatus();
    bool updateSpotsDxccStatus(const QSet<uint> &entities);
    QList<WsjtxEntry> entries() const;

private:
    struct Row
    {
        WsjtxEntry entry;
        WsjtxActivity activity;
    };

    struct SourceActivity
    {
        SourceActivity() : periodMilliseconds(0) {}

        QDateTime referencePeriod;
        int periodMilliseconds;
    };

    int findRow(const WsjtxEntry &entry) const;
    static int periodMilliseconds(const WsjtxEntry &entry);
    QDateTime referencePeriod(const Row &row) const;
    bool updateSourceActivity(const WsjtxEntry &entry,
                              const QDateTime &decodedAt);
    void emitSourceActivityChanged(const QString &id);

    QList<Row> wsjtxData;
    QHash<QString, SourceActivity> sourceActivity;
};

#endif // QLOG_MODELS_WSJTXTABLEMODEL_H
