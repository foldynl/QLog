#include <QCryptographicHash>
#include <QLoggingCategory>
#include <QElapsedTimer>
#include <QThread>
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
#include <QNativeIpcKey>
#endif
#include "AppGuard.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.appguard");
namespace
{

    QString generateKeyHash( const QString& key, const QString& salt )
    {
        QByteArray data;

        data.append( key.toUtf8() );
        data.append( salt.toUtf8() );
        data = QCryptographicHash::hash( data, QCryptographicHash::Sha1 ).toHex();

        return data;
    }

}

AppGuard::AppGuard( const QString& key )
    : key( key )
    , memLockKey( generateKeyHash( key, "_memLockKey" ) )
    , sharedmemKey( generateKeyHash( key, "_sharedmemKey" ) )
    , sharedMem(
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
          QSharedMemory::legacyNativeKey(sharedmemKey)
#else
          sharedmemKey
#endif
 )
    , memLock(
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
          QSystemSemaphore::legacyNativeKey(memLockKey),
#else
          memLockKey,
#endif
     1 )
{
    FCT_IDENTIFICATION;
}

AppGuard::~AppGuard()
{
    FCT_IDENTIFICATION;

    release();
}

bool AppGuard::tryToRun(int timeoutMs)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << timeoutMs;

    // An IPC failure requires cleanup before another attempt, even if still attached.
    if ( !lastError.isEmpty() )
        return false;
    if ( sharedMem.isAttached() )
        return true;

    QElapsedTimer timer;
    timer.start();
    do
    {
        if ( !memLock.acquire() )
        {
            setError(QObject::tr("Cannot acquire the QLog instance semaphore: %1").arg(memLock.errorString()));
            return false;
        }
        semaphoreAcquired = true;

        // Removes stale Unix memory; attach() before create() is also required on macOS (#257).
        if ( sharedMem.attach() )
        {
            if ( !sharedMem.detach() )
                setError(QObject::tr("Cannot detach QLog instance shared memory: %1").arg(sharedMem.errorString()));
        }
        // Qt6 can retain AlreadyExists while cleaning a segment removed since the previous create().
        else if ( sharedMem.error() != QSharedMemory::NotFound
                  && sharedMem.error() != QSharedMemory::AlreadyExists )
            setError(QObject::tr("Cannot attach QLog instance shared memory: %1").arg(sharedMem.errorString()));

        bool acquired = false;
        if ( lastError.isEmpty() )
        {
            acquired = sharedMem.create(sizeof(quint64));
            if ( !acquired && sharedMem.error() != QSharedMemory::AlreadyExists )
                setError(QObject::tr("Cannot create QLog instance shared memory: %1").arg(sharedMem.errorString()));
        }

        semaphoreAcquired = !memLock.release();
        if ( semaphoreAcquired )
            setError(QObject::tr("Cannot release the QLog instance semaphore: %1").arg(memLock.errorString()));
        if ( !lastError.isEmpty() )
            return false;
        if ( acquired )
            return true;

        // The timeout bounds busy-instance retries, not blocking Qt IPC operations.
        const qint64 remaining = timeoutMs - timer.elapsed();
        if ( remaining > 0 )
            QThread::msleep(static_cast<unsigned long>(qMin<qint64>(BUSY_WAIT_TIME, remaining)));
    }
    while ( timer.elapsed() < timeoutMs );

    return false;
}

void AppGuard::release(void)
{
    FCT_IDENTIFICATION;

    if ( !sharedMem.isAttached() && !semaphoreAcquired )
    {
        lastError.clear();
        return;
    }
    // A failed release leaves the semaphore held; cleanup must not acquire it again.
    if ( !semaphoreAcquired && !memLock.acquire() )
    {
        setError(QObject::tr("Cannot acquire the QLog instance semaphore: %1").arg(memLock.errorString()));
        return;
    }
    semaphoreAcquired = true;

    const bool detached = !sharedMem.isAttached() || sharedMem.detach();
    if ( !detached )
        setError(QObject::tr("Cannot detach QLog instance shared memory: %1").arg(sharedMem.errorString()));

    semaphoreAcquired = !memLock.release();
    if ( semaphoreAcquired )
        setError(QObject::tr("Cannot release the QLog instance semaphore: %1").arg(memLock.errorString()));
    if ( detached && !semaphoreAcquired )
        lastError.clear();
}

QString AppGuard::errorString() const
{
    FCT_IDENTIFICATION;

    return lastError;
}

void AppGuard::setError(const QString &message)
{
    FCT_IDENTIFICATION;

    if ( lastError.isEmpty() )
        lastError = message;
    qCWarning(runtime) << message;
}
