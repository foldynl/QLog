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

class GuardTestProcess : public QProcess
{
public:
    ~GuardTestProcess() override
    {
        if ( state() != QProcess::NotRunning )
        {
            kill();
            waitForFinished(5000);
        }
    }

    bool waitForLine(QByteArray &line, int timeoutMs = 5000)
    {
        line.clear();
        QElapsedTimer timer;
        timer.start();
        // A pipe wakeup can precede a full stdout line, including after stderr output on Windows.
        while ( !canReadLine() )
        {
            const qint64 remaining = timeoutMs - timer.elapsed();
            if ( remaining <= 0 || state() == QProcess::NotRunning )
                return false;
            waitForReadyRead(static_cast<int>(remaining));
        }
        line = readLine().trimmed();
        return true;
    }

    QString diagnostics()
    {
        // QVERIFY2 may evaluate diagnostics on success; preserve unread protocol output.
        return QString("state=%1, error=%2, stdout=%3, stderr=%4")
                .arg(state()).arg(errorString(), QString::fromUtf8(peek(bytesAvailable())),
                                  QString::fromUtf8(readAllStandardError()));
    }
};

class AppGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void restartGuardWaitsForRelease();
    void restartGuardTimeout();
    void ordinaryStartupRefusesSecondInstance();
    void restartGuardRetriesConcurrentAcquisition();

private:
    bool startWaitingChild(GuardTestProcess &process, const QString &key, int timeout, bool race = false);
};

void AppGuardTest::restartGuardRetriesConcurrentAcquisition()
{
#ifdef GUARD_CREATION_WRAPPER
    const QString key = QUuid::createUuid().toString();
    GuardTestProcess child;
    QVERIFY(startWaitingChild(child, key, 2000, true));
    QByteArray output;
    QVERIFY2(child.waitForLine(output), qPrintable(child.diagnostics()));
    QVERIFY2(output.startsWith("creating:"), output.constData());
    const QString nativeKey = QString::fromUtf8(output.mid(9));
    QSharedMemory competitor(nativeKey);
    QVERIFY(competitor.create(sizeof(quint64)));
    child.write("continue\n");
    QVERIFY2(!child.waitForFinished(100) && child.state() == QProcess::Running,
             qPrintable(child.diagnostics()));
    QVERIFY(competitor.detach());
    QVERIFY2(child.state() == QProcess::NotRunning || child.waitForFinished(5000),
             qPrintable(child.diagnostics()));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
#else
    QSKIP("Requires a Qt5 linker wrapper for a deterministic native IPC acquisition race.");
#endif
}

bool AppGuardTest::startWaitingChild(GuardTestProcess &process, const QString &key, int timeout, bool race)
{
    QStringList arguments = {"--guard", key, QString::number(timeout)};
    if ( race )
        arguments.append("race");
    QElapsedTimer timer;
    timer.start();
    process.start(QCoreApplication::applicationFilePath(), arguments);
    QByteArray line;
    if ( !process.waitForStarted(5000)
         || !process.waitForLine(line, 5000 - static_cast<int>(timer.elapsed())) || line != "ready" )
    {
        qWarning() << "Expected child message ready, received:" << line << process.diagnostics();
        return false;
    }
    process.write("go\n");
    if ( !process.waitForLine(line, 5000 - static_cast<int>(timer.elapsed())) || line != "attempting" )
    {
        qWarning() << "Expected child message attempting, received:" << line << process.diagnostics();
        return false;
    }
    return true;
}

void AppGuardTest::restartGuardWaitsForRelease()
{
    const QString key = QUuid::createUuid().toString();
    AppGuard guard(key);
    QVERIFY(guard.tryToRun());
    GuardTestProcess child;
    QVERIFY(startWaitingChild(child, key, 2000));
    QVERIFY2(!child.waitForFinished(100) && child.state() == QProcess::Running,
             qPrintable(child.diagnostics()));
    guard.release();
    QVERIFY2(child.state() == QProcess::NotRunning || child.waitForFinished(5000),
             qPrintable(child.diagnostics()));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
}

void AppGuardTest::restartGuardTimeout()
{
    const QString key = QUuid::createUuid().toString();
    AppGuard guard(key);
    QVERIFY(guard.tryToRun());
    GuardTestProcess child;
    QVERIFY(startWaitingChild(child, key, 200));
    QVERIFY2(child.state() == QProcess::NotRunning || child.waitForFinished(5000),
             qPrintable(child.diagnostics()));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 2);
    const QByteArray output = child.readAllStandardOutput().trimmed();
    bool validElapsed = false;
    const qint64 elapsed = output.toLongLong(&validElapsed);
    QVERIFY2(validElapsed, output.constData());
    QVERIFY(elapsed >= 200);
    QVERIFY(elapsed < 5000);
}

void AppGuardTest::ordinaryStartupRefusesSecondInstance()
{
    const QString key = QUuid::createUuid().toString();
    AppGuard guard(key);
    QVERIFY(guard.tryToRun());
    GuardTestProcess child;
    QVERIFY(startWaitingChild(child, key, 0));
    QVERIFY2(child.state() == QProcess::NotRunning || child.waitForFinished(5000),
             qPrintable(child.diagnostics()));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
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
