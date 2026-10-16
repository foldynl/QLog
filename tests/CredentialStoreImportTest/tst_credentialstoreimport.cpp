#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "fakes/KeychainJobs.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"
#include "core/PasswordCipher.h"

QMap<QPair<QString, QString>, QString> QKeychain::Job::passwords;
QKeychain::Job::Operation QKeychain::Job::failedOperation = QKeychain::Job::READ;
QString QKeychain::Job::failedKey;

QPair<QString, QString> credentialIdentity(const QString &storageKey, const QString &user)
{
    const QString service = "CredentialStoreImportTest:" + storageKey;
#ifdef Q_OS_WIN
    return QKeychain::Job::credentialIdentity(service, service + ":" + user);
#else
    return QKeychain::Job::credentialIdentity(service, user);
#endif
}

class CredentialStoreImportTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();
    void exportReportsReadFailure();
    void exportImportPreservesCredentials();
    void exportMetadataFailureKeepsPreviousPayload();
    void passphrasePersistenceFailureIsReported();
    void credentialDeletionFailureIsReported_data();
    void credentialDeletionFailureIsReported();
    void importReportsPartialWriteFailure_data();
    void importReportsPartialWriteFailure();
    void importRejectsMalformedBatchBeforeWriting_data();
    void importRejectsMalformedBatchBeforeWriting();
};

void CredentialStoreImportTest::initTestCase()
{
    QCoreApplication::setApplicationName("CredentialStoreImportTest");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
    QSqlQuery query(db);
    QVERIFY(query.exec("CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)"));
    CredentialRegistry::instance().add("TestService", []()
    {
        return QList<CredentialDescriptor>{
            {"TestService", []() { return QString("first"); }},
            {"TestService", []() { return QString("second"); }},
            {"TestService", []() { return QString("third"); }}
        };
    });
}

void CredentialStoreImportTest::init()
{
    QKeychain::Job::passwords.clear();
    QKeychain::Job::failedKey.clear();
    LogParam::removeEncryptedPasswords();
    LogParam::removeSourcePlatform();
}

void CredentialStoreImportTest::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void CredentialStoreImportTest::exportReportsReadFailure()
{
    const QByteArray previousPayload("previous recovery payload");
    LogParam::setEncryptedPasswords(previousPayload);
    QKeychain::Job::failedOperation = QKeychain::Job::READ;
    QKeychain::Job::failedKey = credentialIdentity("TestService", "first").second;
    QVERIFY(!CredentialStore::instance()->exportPasswords("passphrase"));
    QCOMPARE(LogParam::getEncryptedPasswords(), previousPayload);
    QKeychain::Job::failedKey.clear();
    QVERIFY(CredentialStore::instance()->exportPasswords("passphrase"));
}

void CredentialStoreImportTest::exportMetadataFailureKeepsPreviousPayload()
{
    const QByteArray previousPayload("previous recovery payload");
    LogParam::setEncryptedPasswords(previousPayload);
    LogParam::setSourcePlatform("previous platform");
    QSqlQuery query;
    QVERIFY(query.exec("CREATE TRIGGER reject_export_platform BEFORE INSERT ON log_param "
                       "WHEN NEW.name = 'sourceplatform' BEGIN SELECT RAISE(FAIL, 'test failure'); END"));
    const bool result = CredentialStore::instance()->exportPasswords("passphrase");
    QVERIFY(query.exec("DROP TRIGGER reject_export_platform"));
    QVERIFY(!result);
    QCOMPARE(LogParam::getEncryptedPasswords(), previousPayload);
    QCOMPARE(LogParam::getSourcePlatform(), QString("previous platform"));
}

void CredentialStoreImportTest::exportImportPreservesCredentials()
{
    const QPair<QString, QString> identity = credentialIdentity("TestService", "first");
    QKeychain::Job::passwords[identity] = "secret";
    QVERIFY(CredentialStore::instance()->exportPasswords("passphrase"));
    QKeychain::Job::passwords.clear();
    QVERIFY(CredentialStore::instance()->importPasswords("passphrase"));
    QCOMPARE(QKeychain::Job::passwords.value(identity), QString("secret"));
}

void CredentialStoreImportTest::importReportsPartialWriteFailure_data()
{
    QTest::addColumn<QString>("failedUser");
    QTest::newRow("first") << QString("first");
    QTest::newRow("middle") << QString("second");
}

void CredentialStoreImportTest::passphrasePersistenceFailureIsReported()
{
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("ImportPassphrase", "import").second;
    QVERIFY(!CredentialStore::instance()->saveImportPassphrase("passphrase"));
    QKeychain::Job::failedKey.clear();
    QVERIFY(CredentialStore::instance()->saveImportPassphrase("passphrase"));
    QCOMPARE(CredentialStore::instance()->getImportPassphrase(), QString("passphrase"));
}

void CredentialStoreImportTest::credentialDeletionFailureIsReported_data()
{
    QTest::addColumn<QString>("failedUser");
    QTest::newRow("first") << QString("first");
    QTest::newRow("middle") << QString("second");
}

void CredentialStoreImportTest::credentialDeletionFailureIsReported()
{
    QFETCH(QString, failedUser);
    const QPair<QString, QString> identity = credentialIdentity("TestService", failedUser);
    for ( const QString &user : {QString("first"), QString("second"), QString("third")} )
        QKeychain::Job::passwords[credentialIdentity("TestService", user)] = "secret";
    QKeychain::Job::failedOperation = QKeychain::Job::DELETE;
    QKeychain::Job::failedKey = identity.second;
    QVERIFY(!CredentialStore::instance()->deleteAllPasswords());
    QCOMPARE(QKeychain::Job::passwords.value(identity), QString("secret"));
    QCOMPARE(QKeychain::Job::passwords.size(), 1);
    QKeychain::Job::failedKey.clear();
    QVERIFY(CredentialStore::instance()->deleteAllPasswords());
    QVERIFY(QKeychain::Job::passwords.isEmpty());
    QVERIFY(CredentialStore::instance()->deleteAllPasswords());
}

void CredentialStoreImportTest::importReportsPartialWriteFailure()
{
    QFETCH(QString, failedUser);
    const QByteArray json(R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"one"},
        {"storagekey":"TestService","username":"second","password":"two"},
        {"storagekey":"TestService","username":"third","password":"three"}]})");
    QByteArray encrypted;
    QVERIFY(PasswordCipher::encrypt("passphrase", json, encrypted));
    LogParam::setEncryptedPasswords(encrypted);
    QKeychain::Job::failedOperation = QKeychain::Job::WRITE;
    QKeychain::Job::failedKey = credentialIdentity("TestService", failedUser).second;
    QVERIFY(!CredentialStore::instance()->importPasswords("passphrase"));
    QCOMPARE(LogParam::getEncryptedPasswords(), encrypted);
    QKeychain::Job::failedKey.clear();
    QVERIFY(CredentialStore::instance()->importPasswords("passphrase"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "first")), QString("one"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "second")), QString("two"));
    QCOMPARE(QKeychain::Job::passwords.value(credentialIdentity("TestService", "third")), QString("three"));
}

void CredentialStoreImportTest::importRejectsMalformedBatchBeforeWriting_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::newRow("missing-array") << QByteArray("{}");
    QTest::newRow("wrong-root") << QByteArray("[]");
    QTest::newRow("invalid-entry") << QByteArray(R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"one"},42]})");
    QTest::newRow("missing-password") << QByteArray(R"({"credentials":[
        {"storagekey":"TestService","username":"first"}]})");
    QTest::newRow("reserved-import-passphrase") << QByteArray(R"({"credentials":[
        {"storagekey":"TestService","username":"first","password":"one"},
        {"storagekey":"ImportPassphrase","username":"import","password":"replacement"}]})");
    QTest::newRow("reserved-storage-case") << QByteArray(R"({"credentials":[
        {"storagekey":"IMPORTPASSPHRASE","username":"import","password":"replacement"}]})");
    QTest::newRow("reserved-user-case") << QByteArray(R"({"credentials":[
        {"storagekey":"ImportPassphrase","username":"IMPORT","password":"replacement"}]})");
}

void CredentialStoreImportTest::importRejectsMalformedBatchBeforeWriting()
{
    QFETCH(QByteArray, json);
    QByteArray encrypted;
    QVERIFY(PasswordCipher::encrypt("passphrase", json, encrypted));
    LogParam::setEncryptedPasswords(encrypted);
    QVERIFY(CredentialStore::instance()->saveImportPassphrase("passphrase"));
    const auto previousPasswords = QKeychain::Job::passwords;
    QVERIFY(!CredentialStore::instance()->importPasswords("passphrase"));
    QVERIFY(QKeychain::Job::passwords == previousPasswords);
    QCOMPARE(CredentialStore::instance()->getImportPassphrase(), QString("passphrase"));
}

QTEST_GUILESS_MAIN(CredentialStoreImportTest)
#include "tst_credentialstoreimport.moc"
