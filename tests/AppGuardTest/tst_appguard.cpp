#include <QtTest>
#include <QElapsedTimer>
#include <QProcess>
#include <QTextStream>
#include <QUuid>
#include "core/AppGuard.h"

#ifdef GUARD_CREATION_WRAPPER
int pauseOnAttach = 0;
extern "C" bool __real__ZN13QSharedMemory6attachENS_10AccessModeE(QSharedMemory *, QSharedMemory::AccessMode);
extern "C" bool __wrap__ZN13QSharedMemory6attachENS_10AccessModeE(QSharedMemory *memory, QSharedMemory::AccessMode mode)
{
    if ( pauseOnAttach > 0 && --pauseOnAttach == 0 )
    {
        QTextStream output(stdout);
        output << "creating:" << memory->key() << '\n';
        output.flush();
        QTextStream input(stdin);
        input.readLine();
    }
    return __real__ZN13QSharedMemory6attachENS_10AccessModeE(memory, mode);
}
#endif

class AppGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void restartGuardWaitsForRelease();
    void restartGuardTimeout();
    void ordinaryStartupRefusesSecondInstance();
    void restartGuardRetriesConcurrentAcquisition();

private:
    bool startWaitingChild(QProcess &process, const QString &key, int timeout);
};

void AppGuardTest::restartGuardRetriesConcurrentAcquisition()
{
#ifdef GUARD_CREATION_WRAPPER
    const QString key = QUuid::createUuid().toString();
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {"--guard", key, "2000", "race"});
    QVERIFY(child.waitForStarted(5000));
    QVERIFY(child.waitForReadyRead(5000));
    QCOMPARE(child.readLine().trimmed(), QByteArray("ready"));
    child.write("go\n");
    QByteArray output;
    while ( !output.contains("creating:") || !output.endsWith("\n") )
    {
        QVERIFY(child.waitForReadyRead(5000));
        output += child.readAllStandardOutput();
    }
    const QString nativeKey = QString::fromUtf8(output.mid(output.indexOf("creating:") + 9).trimmed());
    QSharedMemory competitor(nativeKey);
    QVERIFY(competitor.create(sizeof(quint64)));
    child.write("continue\n");
    QTest::qWait(100);
    QVERIFY(competitor.detach());
    QVERIFY(child.state() == QProcess::NotRunning || child.waitForFinished(5000));
    QCOMPARE(child.exitCode(), 0);
#else
    QSKIP("Requires a Qt5 linker wrapper for a deterministic native IPC acquisition race.");
#endif
}

bool AppGuardTest::startWaitingChild(QProcess &process, const QString &key, int timeout)
{
    process.start(QCoreApplication::applicationFilePath(), {"--guard", key, QString::number(timeout)});
    if ( !process.waitForStarted(5000) || !process.waitForReadyRead(5000)
         || process.readLine().trimmed() != "ready" )
        return false;
    process.write("go\n");
    return process.waitForReadyRead(5000) && process.readLine().trimmed() == "attempting";
}

void AppGuardTest::restartGuardWaitsForRelease()
{
    const QString key = QUuid::createUuid().toString();
    AppGuard guard(key);
    QVERIFY(guard.tryToRun());
    QProcess child;
    QVERIFY(startWaitingChild(child, key, 2000));
    // The child has begun acquiring while the parent still owns the guard.
    QTest::qWait(100);
    guard.release();
    QVERIFY(child.state() == QProcess::NotRunning || child.waitForFinished(5000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
}

void AppGuardTest::restartGuardTimeout()
{
    const QString key = QUuid::createUuid().toString();
    AppGuard guard(key);
    QVERIFY(guard.tryToRun());
    QProcess child;
    QVERIFY(startWaitingChild(child, key, 200));
    QVERIFY(child.waitForFinished(5000));
    QCOMPARE(child.exitCode(), 2);
    const qint64 elapsed = child.readAllStandardOutput().trimmed().toLongLong();
    QVERIFY(elapsed >= 200);
    QVERIFY(elapsed < 5000);
}

void AppGuardTest::ordinaryStartupRefusesSecondInstance()
{
    const QString key = QUuid::createUuid().toString();
    AppGuard guard(key);
    QVERIFY(guard.tryToRun());
    QProcess child;
    QVERIFY(startWaitingChild(child, key, 0));
    QVERIFY(child.waitForFinished(5000));
    QCOMPARE(child.exitCode(), 2);
    guard.release();
    QVERIFY(guard.tryToRun());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if ( arguments.value(1) == "--guard" )
    {
#ifdef GUARD_CREATION_WRAPPER
        pauseOnAttach = 0;
#endif
        AppGuard guard(arguments.value(2));
        QTextStream output(stdout);
        output << "ready\n";
        output.flush();
        QTextStream input(stdin);
        if ( input.readLine() != "go" )
            return 3;
        output << "attempting\n";
        output.flush();
        QElapsedTimer timer;
        timer.start();
#ifdef GUARD_CREATION_WRAPPER
        // Skip the initial presence check, then inject a winner before the create workaround.
        pauseOnAttach = arguments.value(4) == "race" ? 2 : 0;
#endif
        const bool acquired = guard.tryToRun(arguments.value(3).toInt());
        output << timer.elapsed() << '\n';
        output.flush();
        return acquired ? 0 : 2;
    }
    AppGuardTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_appguard.moc"
