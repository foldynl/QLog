#include <QtTest>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QTimer>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/CredentialStore.h"
#include "core/PasswordCipher.h"
#include "core/LogParam.h"

namespace {
QtMessageHandler previousHandler = nullptr;

void credentialStoreTestMessageHandler(QtMsgType type, const QMessageLogContext &context,
                                       const QString &message)
{
    if (type == QtWarningMsg)
    {
        if (message.contains(QStringLiteral("This plugin does not support propagateSizeHints()")))
            return;
        if (message.contains(QStringLiteral("Cannot save a password. Error")))
            return;
        if (message.contains(QStringLiteral("Cannot get a password")))
            return;
    }

    if (previousHandler)
        previousHandler(type, context, message);
}

struct ScopedMessageHandler
{
    ScopedMessageHandler()
    {
        previousHandler = qInstallMessageHandler(credentialStoreTestMessageHandler);
    }

    ~ScopedMessageHandler()
    {
        qInstallMessageHandler(previousHandler);
        previousHandler = nullptr;
    }
};

}  // anonymous namespace

// Helper class to access protected CredentialStore methods via SecureServiceBase
// Must be outside anonymous namespace for friend declaration to work
class TestCredentialService : public SecureServiceBase<TestCredentialService>
{
public:
    DECLARE_SECURE_SERVICE(TestCredentialService)

    // Expose protected methods from SecureServiceBase for testing
    static QString testGetPassword(const QString &key, const QString &user)
    {
        return getPassword(key, user);
    }

    static void testSavePassword(const QString &key, const QString &user, const QString &pass)
    {
        savePassword(key, user, pass);
    }

    static void testDeletePassword(const QString &key, const QString &user)
    {
        deletePassword(key, user);
    }

    // Test if save succeeded by trying to get the password back
    static bool testSaveAndVerify(const QString &key, const QString &user, const QString &pass)
    {
        savePassword(key, user, pass);
        return getPassword(key, user) == pass;
    }
};

// Required registration for TestCredentialService
void TestCredentialService::registerCredentials()
{
    CredentialRegistry::instance().add("TestCredentialService", []() {
        return QList<CredentialDescriptor>{};
    });
}

REGISTRATION_SECURE_SERVICE(TestCredentialService);

class CredentialStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void instance_isStable();
    void savePassword_emptyInputs_noEffect_data();
    void savePassword_emptyInputs_noEffect();
    void getPassword_emptyKeyOrUser_returnsEmpty();
    void deletePassword_emptyKeyOrUser_noCrash();
    void saveGetDelete_worksAndCleansUp();
    void getPassword_benchmark_keychain();

    // New tests for export/import functionality
    void exportImportPasswords_roundTrip();
    void importPasswords_wrongPassword_fails();
    void importPassphrase_saveGetDelete();

private:
    void setupMessageBoxCloser();
    QTimer messageBoxCloser;
    QTemporaryDir *tempDir = nullptr;
};

void CredentialStoreTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    setupMessageBoxCloser();

    // Create temporary database for LogParam tests
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());

    QString dbPath = tempDir->filePath("test.db");

    // Create a minimal database with log_param table
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "testdb");
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());

        QSqlQuery query(db);
        QVERIFY(query.exec("CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)"));
        db.close();
    }
    QSqlDatabase::removeDatabase("testdb");

    // Set as default database connection for LogParam
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
}

void CredentialStoreTest::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);

    delete tempDir;
    tempDir = nullptr;
}

void CredentialStoreTest::setupMessageBoxCloser()
{
    messageBoxCloser.setInterval(25);
    messageBoxCloser.setSingleShot(false);
    connect(&messageBoxCloser, &QTimer::timeout, this, []() {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets)
        {
            if (auto *box = qobject_cast<QMessageBox *>(widget))
                box->reject();
        }
    });
    messageBoxCloser.start();
}

void CredentialStoreTest::instance_isStable()
{
    CredentialStore *first = CredentialStore::instance();
    CredentialStore *second = CredentialStore::instance();
    QVERIFY(first != nullptr);
    QCOMPARE(first, second);
}

void CredentialStoreTest::savePassword_emptyInputs_noEffect_data()
{
    QTest::addColumn<QString>("storageKey");
    QTest::addColumn<QString>("user");
    QTest::addColumn<QString>("pass");

    QTest::newRow("empty_all") << QString() << QString() << QString();
    QTest::newRow("empty_key") << QString() << QStringLiteral("u") << QStringLiteral("p");
    QTest::newRow("empty_user") << QStringLiteral("k") << QString() << QStringLiteral("p");
    QTest::newRow("empty_pass") << QStringLiteral("k") << QStringLiteral("u") << QString();
}

void CredentialStoreTest::savePassword_emptyInputs_noEffect()
{
    QFETCH(QString, storageKey);
    QFETCH(QString, user);
    QFETCH(QString, pass);

    // Empty inputs should have no effect (password should remain empty)
    TestCredentialService::testSavePassword(storageKey, user, pass);

    // If key or user is empty, getPassword should return empty
    if (storageKey.isEmpty() || user.isEmpty())
    {
        QCOMPARE(TestCredentialService::testGetPassword(storageKey, user), QString());
    }
}

void CredentialStoreTest::getPassword_emptyKeyOrUser_returnsEmpty()
{
    QCOMPARE(TestCredentialService::testGetPassword(QString(), QStringLiteral("u")), QString());
    QCOMPARE(TestCredentialService::testGetPassword(QStringLiteral("k"), QString()), QString());
    QCOMPARE(TestCredentialService::testGetPassword(QString(), QString()), QString());
}

void CredentialStoreTest::deletePassword_emptyKeyOrUser_noCrash()
{
    TestCredentialService::testDeletePassword(QString(), QStringLiteral("u"));
    TestCredentialService::testDeletePassword(QStringLiteral("k"), QString());
    TestCredentialService::testDeletePassword(QString(), QString());
    QVERIFY(true);
}

void CredentialStoreTest::saveGetDelete_worksAndCleansUp()
{
    const QString unique = QStringLiteral("%1-%2")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    const QString testKey = QStringLiteral("CredentialStoreTest-%1").arg(unique);
    const QString testUser = QStringLiteral("user-%1").arg(unique);
    const QString testPwd = QStringLiteral("pass-%1").arg(unique);

    struct Cleanup
    {
        QString key;
        QString user;
        ~Cleanup()
        {
            TestCredentialService::testDeletePassword(key, user);
        }
    } cleanup{testKey, testUser};

    // Ensure clean state
    TestCredentialService::testDeletePassword(testKey, testUser);
    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), QString());

    // Save and verify
    if (!TestCredentialService::testSaveAndVerify(testKey, testUser, testPwd))
        QSKIP("Credential store backend not available (save failed).");

    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), testPwd);

    // Delete and verify
    TestCredentialService::testDeletePassword(testKey, testUser);
    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), QString());
}

void CredentialStoreTest::getPassword_benchmark_keychain()
{
    const ScopedMessageHandler handlerScope;

    const QString unique = QStringLiteral("%1-%2-bench")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    const QString testKey = QStringLiteral("CredentialStoreBench-%1").arg(unique);
    const QString testUser = QStringLiteral("user-%1").arg(unique);
    const QString testPwd = QStringLiteral("pass-%1").arg(unique);

    struct Cleanup
    {
        QString key;
        QString user;
        ~Cleanup()
        {
            TestCredentialService::testDeletePassword(key, user);
        }
    } cleanup{testKey, testUser};

    TestCredentialService::testDeletePassword(testKey, testUser);
    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), QString());

    if (!TestCredentialService::testSaveAndVerify(testKey, testUser, testPwd))
        QSKIP("Credential store backend not available (save failed).");

    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), testPwd);

    QString out;
    QBENCHMARK { out = TestCredentialService::testGetPassword(testKey, testUser); }
    QCOMPARE(out, testPwd);
}

void CredentialStoreTest::exportImportPasswords_roundTrip()
{
    const ScopedMessageHandler handlerScope;

    const QString unique = QStringLiteral("%1-%2-export")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    const QString testKey = QStringLiteral("ExportTest-%1").arg(unique);
    const QString testUser = QStringLiteral("user-%1").arg(unique);
    const QString testPwd = QStringLiteral("secret-%1").arg(unique);
    const QString passphrase = QStringLiteral("test-passphrase-12345");

    struct Cleanup
    {
        QString key;
        QString user;
        ~Cleanup()
        {
            TestCredentialService::testDeletePassword(key, user);
            LogParam::removeEncryptedPasswords();
            LogParam::removeSourcePlatform();
        }
    } cleanup{testKey, testUser};

    // Save a test password
    if (!TestCredentialService::testSaveAndVerify(testKey, testUser, testPwd))
        QSKIP("Credential store backend not available (save failed).");

    // Register test credential for export
    CredentialRegistry::instance().add("ExportTest", [testKey, testUser]() {
        return QList<CredentialDescriptor>{{testKey, [testUser]() { return testUser; }}};
    });

    // Export
    QVERIFY(CredentialStore::instance()->exportPasswords(passphrase));

    // Verify encrypted blob was saved
    QByteArray blob = LogParam::getEncryptedPasswords();
    QVERIFY(!blob.isEmpty());

    // Delete the original password
    TestCredentialService::testDeletePassword(testKey, testUser);
    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), QString());

    // Import
    QVERIFY(CredentialStore::instance()->importPasswords(passphrase));

    // Verify password was restored
    QCOMPARE(TestCredentialService::testGetPassword(testKey, testUser), testPwd);
}

void CredentialStoreTest::importPasswords_wrongPassword_fails()
{
    const ScopedMessageHandler handlerScope;

    const QString unique = QStringLiteral("%1-%2-wrongpwd")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    const QString testKey = QStringLiteral("WrongPwdTest-%1").arg(unique);
    const QString testUser = QStringLiteral("user-%1").arg(unique);
    const QString testPwd = QStringLiteral("secret-%1").arg(unique);
    const QString correctPassphrase = QStringLiteral("correct-passphrase");
    const QString wrongPassphrase = QStringLiteral("wrong-passphrase");

    struct Cleanup
    {
        QString key;
        QString user;
        ~Cleanup()
        {
            TestCredentialService::testDeletePassword(key, user);
            LogParam::removeEncryptedPasswords();
            LogParam::removeSourcePlatform();
        }
    } cleanup{testKey, testUser};

    // Save and export with correct passphrase
    if (!TestCredentialService::testSaveAndVerify(testKey, testUser, testPwd))
        QSKIP("Credential store backend not available (save failed).");

    CredentialRegistry::instance().add("WrongPwdTest", [testKey, testUser]() {
        return QList<CredentialDescriptor>{{testKey, [testUser]() { return testUser; }}};
    });

    QVERIFY(CredentialStore::instance()->exportPasswords(correctPassphrase));

    // Try to import with wrong passphrase
    QVERIFY(!CredentialStore::instance()->importPasswords(wrongPassphrase));
}

void CredentialStoreTest::importPassphrase_saveGetDelete()
{
    const ScopedMessageHandler handlerScope;

    const QString testPassphrase = QStringLiteral("import-test-passphrase-67890");

    struct Cleanup
    {
        ~Cleanup()
        {
            CredentialStore::instance()->deleteImportPassphrase();
        }
    } cleanup;

    // Initially should be empty
    CredentialStore::instance()->deleteImportPassphrase();
    QCOMPARE(CredentialStore::instance()->getImportPassphrase(), QString());

    // Save
    CredentialStore::instance()->saveImportPassphrase(testPassphrase);

    // Get
    QString retrieved = CredentialStore::instance()->getImportPassphrase();
    if (retrieved.isEmpty())
        QSKIP("Credential store backend not available (save failed).");

    QCOMPARE(retrieved, testPassphrase);

    // Delete
    CredentialStore::instance()->deleteImportPassphrase();
    QCOMPARE(CredentialStore::instance()->getImportPassphrase(), QString());
}

int main(int argc, char **argv)
{
    // The macOS keychain backend requires native event dispatching.
#ifndef Q_OS_MACOS
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
#endif

    QApplication app(argc, argv);
    CredentialStoreTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_credentialstore.moc"
