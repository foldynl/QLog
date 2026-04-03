#ifndef QLOG_CORE_ADIFFILEMONITOR_H
#define QLOG_CORE_ADIFFILEMONITOR_H

#include <QObject>
#include <QSqlRecord>
#include "logformat/LogFormat.h"

class ADIFFileMonitor : public QObject
{
    Q_OBJECT

public:
    static const int MAX_SLOTS = 4;

    enum ImportFrequency {
        DISABLED = 0,
        STARTUP  = 1,
        SHUTDOWN = 2
    };

    explicit ADIFFileMonitor(QObject *parent = nullptr);
    ~ADIFFileMonitor();

    void runStartupImports();
    void runShutdownImports();

private:
    struct SlotConfig {
        bool enabled = false;
        QString path;
        ImportFrequency frequency = DISABLED;
        QString profile;
    };

    QList<SlotConfig> getEnabledSlots(ImportFrequency freq) const;
    void runImportForSlot(const SlotConfig &slot);
    static LogFormat::duplicateQSOBehaviour skipAllDuplicates(QSqlRecord *, QSqlRecord *);
};

#endif // QLOG_CORE_ADIFFILEMONITOR_H
