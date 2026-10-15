#include <QtTest>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QProcess>
#include <QTextStream>
#include <QUuid>
#include "core/AppGuard.h"

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
    void unusedGuardDoesNotWaitForSemaphore();
    void guardCanAcquireAfterOwnerCrash();
    void restartGuardWaitsForRelease();
    void restartGuardTimeout();
    void ordinaryStartupRefusesSecondInstance();

private:
    bool startWaitingChild(GuardTestProcess &process, const QString &key, int timeout, const QString &mode = QString());
};

void AppGuardTest::unusedGuardDoesNotWaitForSemaphore()
{
    const QString key = QUuid::createUuid().toString();
    const QString semaphoreKey = QString::fromLatin1(QCryptographicHash::hash(
                key.toUtf8() + "_memLockKey", QCryptographicHash::Sha1).toHex());
    QSystemSemaphore semaphore(semaphoreKey, 1);
    QVERIFY(semaphore.acquire());

    GuardTestProcess child;
    child.start(QCoreApplication::applicationFilePath(), {"--guard", key, "0", "unused"});
    QVERIFY2(child.waitForStarted(5000), qPrintable(child.diagnostics()));
    QByteArray output;
    QVERIFY2(child.waitForLine(output), qPrintable(child.diagnostics()));
    QCOMPARE(output, QByteArray("ready"));
    child.write("go\n");
    QVERIFY2(child.state() == QProcess::NotRunning || child.waitForFinished(5000),
             qPrintable(child.diagnostics()));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
    QVERIFY(semaphore.release());
}

void AppGuardTest::guardCanAcquireAfterOwnerCrash()
{
    const QString key = QUuid::createUuid().toString();
    GuardTestProcess child;
    QVERIFY(startWaitingChild(child, key, 0, "hold"));
    QByteArray line;
    QVERIFY2(child.waitForLine(line), qPrintable(child.diagnostics()));
    QCOMPARE(line, QByteArray("owned"));
    child.kill();
    QVERIFY(child.waitForFinished(5000));

    AppGuard guard(key);
    QVERIFY2(guard.tryToRun(), qPrintable(guard.errorString()));
}

bool AppGuardTest::startWaitingChild(GuardTestProcess &process, const QString &key, int timeout, const QString &mode)
{
    QStringList arguments = {"--guard", key, QString::number(timeout)};
    if ( !mode.isEmpty() )
        arguments.append(mode);
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
        AppGuard guard(arguments.value(2));
        QTextStream output(stdout);
        output << "ready\n";
        output.flush();
        QTextStream input(stdin);
        if ( input.readLine() != "go" )
            return 3;
        if ( arguments.value(4) == "unused" )
            return 0;
        output << "attempting\n";
        output.flush();
        QElapsedTimer timer;
        timer.start();
        const bool acquired = guard.tryToRun(arguments.value(3).toInt());
        if ( acquired && arguments.value(4) == "hold" )
        {
            output << "owned\n";
            output.flush();
            input.readLine();
        }
        output << timer.elapsed() << '\n';
        output.flush();
        return acquired ? 0 : (guard.errorString().isEmpty() ? 2 : 4);
    }
    AppGuardTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_appguard.moc"
