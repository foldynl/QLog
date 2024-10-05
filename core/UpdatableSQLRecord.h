#ifndef QLOG_CORE_UPDATABLESQLRECORD_H
#define QLOG_CORE_UPDATABLESQLRECORD_H

#include <QObject>
#include <QSqlRecord>
#include <QTimer>
#include <QHash>

class UpdatableSQLRecord : public QObject
{
    Q_OBJECT

public:
    explicit UpdatableSQLRecord(int interval = 500,
                                QObject *parent = nullptr);

    ~UpdatableSQLRecord();
    void updateRecord(const QSqlRecord &record);

signals:
    void recordReady( QSqlRecord );

private slots:
    void emitStoreRecord();

private:

    enum MatchingType{
        QSOMatchingType
    };

    QHash<MatchingType, QStringList> matchingFields
    {
        {QSOMatchingType, {"callsign", "mode", "submode"}}
    };

    bool matchQSO(const MatchingType,
                  const QSqlRecord &);

    QSqlRecord internalRecord;
    QTimer timer;
    int interval;
};

#endif // QLOG_CORE_UPDATABLESQLRECORD_H
