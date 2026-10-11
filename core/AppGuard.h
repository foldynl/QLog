#ifndef QLOG_CORE_APPGUARD_H
#define QLOG_CORE_APPGUARD_H

#include <QObject>
#include <QSharedMemory>
#include <QSystemSemaphore>

class AppGuard
{

public:
    explicit AppGuard( const QString& key );
    ~AppGuard();

    bool isAnotherRunning(void);
    bool tryToRun(int timeoutMs = 0);
    void release(void);

private:
    const int BUSY_WAIT_TIME = 50;
    const QString key;
    const QString memLockKey;
    const QString sharedmemKey;

    QSharedMemory sharedMem;
    QSystemSemaphore memLock;

    Q_DISABLE_COPY( AppGuard )
};

#endif // QLOG_CORE_APPGUARD_H
