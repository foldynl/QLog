#include <QEventLoop>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <qt5keychain/keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif
#include <QApplication>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <openssl/rand.h>
#include "CredentialStore.h"
#include "core/debug.h"
#include "core/PasswordCipher.h"
#include "core/LogParam.h"
#include "core/LogDatabase.h"

MODULE_IDENTIFICATION("qlog.core.credentialstore");

const QString CredentialStore::IMPORT_PASSPHRASE_STORAGE_KEY = QStringLiteral("ImportPassphrase");
const QString CredentialStore::IMPORT_PASSPHRASE_USERNAME = QStringLiteral("import");

using namespace QKeychain;

CredentialRegistry &CredentialRegistry::instance()
{
    static CredentialRegistry r;
    return r;
}

void CredentialRegistry::add(const QString &, const std::function<QList<CredentialDescriptor>()> &fn)
{
    callbacks.append(fn);
}

const QList<CredentialDescriptor> CredentialRegistry::allDescriptors() const
{
    FCT_IDENTIFICATION;

    QList<CredentialDescriptor> result;

    for ( const std::function<QList<CredentialDescriptor>()> &fn : callbacks ) result.append(fn());
    return result;
}

CredentialStore::CredentialStore(QObject *parent) : QObject(parent)
{
    FCT_IDENTIFICATION;
}

int CredentialStore::savePassword(const QString &storage_key, const QString &user, const QString &pass)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << storage_key << " " << user;

    if ( user.isEmpty() || storage_key.isEmpty() || pass.isEmpty() )
       return 1;

    QString locStorageKey = qApp->applicationName() + ":" + storage_key;
    QString locUser = user;
    QString locPass = pass;
    QEventLoop loop;

    // write a password to Credential Storage
    WritePasswordJob job(QLatin1String(locStorageKey.toStdString().c_str()));
    job.setAutoDelete(false);
#ifdef Q_OS_WIN
    // see more qtkeychain issue #105
    locUser.prepend(locStorageKey + ":");
#endif
    job.setKey(locUser);
    job.setTextData(locPass);

    job.connect(&job, &WritePasswordJob::finished, &loop, &QEventLoop::quit);

    job.start();
    loop.exec();

    if ( job.error() )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Critical"),
                              QMessageBox::tr("Cannot save a password for %1 to the Credential Store").arg(storage_key)
                              + "<p>"
                              + job.errorString());
        qCWarning(runtime) << "Cannot save a password. Error " << job.errorString();
        return 1;
    }

    return 0;
}

QString CredentialStore::getPassword(const QString &storage_key, const QString &user, bool *success)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << storage_key << " " << user;

    if ( success )
        *success = true;

    if ( user.isEmpty() || storage_key.isEmpty() )
        return QString();

    QString locStorageKey = qApp->applicationName() + ":" + storage_key;
    QString locUser = user;
    QString pass;
    QEventLoop loop;

    // get a password from Credential Storage
    ReadPasswordJob job(QLatin1String(locStorageKey.toStdString().c_str()));
    job.setAutoDelete(false);
#ifdef Q_OS_WIN
    // see more qtkeychain issue #105
    locUser.prepend(locStorageKey + ":");
#endif
    job.setKey(locUser);

    job.connect(&job, &ReadPasswordJob::finished, &loop, &QEventLoop::quit);

    job.start();
    loop.exec();

    pass = job.textData();

    if ( job.error() && job.error() != EntryNotFound )
    {
        if ( success )
            *success = false;
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Critical"),
                              QMessageBox::tr("Cannot get a password for %1 from the Credential Store").arg(storage_key)
                              + "<p>"
                              + job.errorString());
        qCWarning(runtime) << "Cannot get a password. Error " << job.errorString();
        return QString();
    }

    return pass;
}

bool CredentialStore::deletePassword(const QString &storage_key, const QString &user)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << storage_key << " " << user;

    if ( user.isEmpty() || storage_key.isEmpty() )
        return true;

    QString locStorageKey = qApp->applicationName() + ":" + storage_key;
    QString locUser = user;
    QEventLoop loop;

    // delete password from Secure Storage
    DeletePasswordJob job(QLatin1String(locStorageKey.toStdString().c_str()));
    job.setAutoDelete(false);
#ifdef Q_OS_WIN
    // see more qtkeychain issue #105
    locUser.prepend(locStorageKey + ":");
#endif
    job.setKey(locUser);

    job.connect(&job, &DeletePasswordJob::finished, &loop, &QEventLoop::quit);
    job.start();

    loop.exec();

    if ( job.error() && job.error() != EntryNotFound )
    {
        qCWarning(runtime) << "Cannot delete a password:" << job.errorString();
        return false;
    }

    return true;
}

bool CredentialStore::exportPasswords(const QString &passphrase)
{
    FCT_IDENTIFICATION;

    if ( LogDatabase::hasPendingImport() )
    {
        qCWarning(runtime) << "Cannot overwrite unfinished import credentials";
        return false;
    }

    /*
     * Password storage format:
     * 1. Build JSON object:
     *    {
     *      "credentials": [
     *        {"storagekey": "...", "username": "...", "password": "..."},
     *        ...
     *      ],
     *      "padding": "<base64 random 128-512 bytes>"
     *    }
     * 2. Encrypt JSON with AES-256-GCM using passphrase (via PasswordCipher)
     * 3. Store base64-encoded ciphertext in LogParam ("security/encryptedpasswords")
     */

    const QList<CredentialDescriptor> list = CredentialRegistry::instance().allDescriptors();

    QJsonArray credArray;
    for ( const CredentialDescriptor &desc : list )
    {
        QString user = desc.usernameFn();
        bool readSucceeded = false;
        QString pass = getPassword(desc.storageKey, user, &readSucceeded);
        if ( !readSucceeded )
            return false;

        if ( !pass.isEmpty() )
        {
            QJsonObject entry;
            entry["storagekey"] = desc.storageKey;
            entry["username"] = user;
            entry["password"] = pass;
            credArray.append(entry);
        }
    }

    // generate random padding (128-512 bytes)
    // add this padding to make it more difficult to estimate the length of passwords.
    unsigned char randLenBuf[2];
    if ( RAND_bytes(randLenBuf, 2) != 1 )
    {
        qCWarning(runtime) << "RAND_bytes failed for padding length";
        return false;
    }
    int paddingLen = 128 + (static_cast<int>((randLenBuf[0] << 8) | randLenBuf[1]) % (512 - 128 + 1));

    QByteArray paddingRaw(paddingLen, '\0');
    if ( RAND_bytes(reinterpret_cast<unsigned char*>(paddingRaw.data()), paddingLen) != 1 )
    {
        qCWarning(runtime) << "RAND_bytes failed for padding data";
        return false;
    }
    QString paddingB64 = QString::fromLatin1(paddingRaw.toBase64());

    QJsonObject root;
    root["credentials"] = credArray;
    root["padding"] = paddingB64;

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);

    QByteArray blobB64;
    if ( !PasswordCipher::encrypt(passphrase, json, blobB64) )
    {
        qCWarning(runtime) << "Password encryption failed";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if ( !db.transaction() )
    {
        qCWarning(runtime) << "Cannot start export metadata transaction:" << db.lastError();
        return false;
    }

    if ( !LogParam::setEncryptedPasswords(blobB64)
         || !LogParam::setSourcePlatform(LogDatabase::currentPlatformId()) || !db.commit() )
    {
        qCWarning(runtime) << "Cannot save export metadata:" << db.lastError();
        if ( !db.rollback() )
            qCWarning(runtime) << "Cannot roll back export metadata:" << db.lastError();
        return false;
    }

    return true;
}

bool CredentialStore::importPasswords(const QString &passphrase)
{
    FCT_IDENTIFICATION;

    QJsonArray credentials;
    return decodePasswords(passphrase, credentials) && restorePasswords(credentials);
}

bool CredentialStore::decodePasswords(const QString &passphrase, QJsonArray &credentials)
{
    FCT_IDENTIFICATION;

    credentials = QJsonArray();

    /*
     * Import process (reverse of exportPasswords):
     * 1. Read base64-encoded ciphertext from LogParam ("security/encryptedpasswords")
     * 2. Decrypt with AES-256-GCM using passphrase (via PasswordCipher)
     * 3. Validate the complete batch before any database publication or keychain write
     *    (padding field is ignored)
     */

    QByteArray blobB64 = LogParam::getEncryptedPasswords();
    if ( blobB64.isEmpty() )
    {
        qCWarning(runtime) << "No encrypted passwords found";
        return false;
    }

    QByteArray json;
    if ( !PasswordCipher::decrypt(passphrase, blobB64, json) )
    {
        qCWarning(runtime) << "Password decryption failed";
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if ( parseError.error != QJsonParseError::NoError )
    {
        qCWarning(runtime) << "JSON parse error:" << parseError.errorString();
        return false;
    }

    if ( !doc.isObject() || !doc.object().value("credentials").isArray() )
    {
        qCWarning(runtime) << "Invalid credentials array";
        return false;
    }

    QJsonObject root = doc.object();
    const QJsonArray credArray = root["credentials"].toArray();

    // Validate the whole batch before the first keychain write.
    for ( const QJsonValue &val : credArray )
    {
        const QJsonObject entry = val.toObject();
        // Windows credential targets are case-insensitive.
        const bool isImportPassphrase = entry.value("storagekey").toString().compare(IMPORT_PASSPHRASE_STORAGE_KEY, Qt::CaseInsensitive) == 0
                                       && entry.value("username").toString().compare(IMPORT_PASSPHRASE_USERNAME, Qt::CaseInsensitive) == 0;
        if ( !val.isObject() || entry.value("storagekey").toString().isEmpty()
             || entry.value("username").toString().isEmpty()
             || entry.value("password").toString().isEmpty()
             || isImportPassphrase )
        {
            qCWarning(runtime) << "Invalid credential entry";
            return false;
        }
    }

    credentials = credArray;
    return true;
}

bool CredentialStore::restorePasswords(const QJsonArray &credentials)
{
    FCT_IDENTIFICATION;

    for ( const QJsonValue &val : credentials )
    {
        QJsonObject entry = val.toObject();
        QString storageKey = entry["storagekey"].toString();
        QString username = entry["username"].toString();
        QString password = entry["password"].toString();

        if ( savePassword(storageKey, username, password) != 0 )
            return false;
    }

    return true;
}

bool CredentialStore::deleteAllPasswords()
{
    FCT_IDENTIFICATION;

    const QList<CredentialDescriptor> list = CredentialRegistry::instance().allDescriptors();
    bool success = true;

    for (const CredentialDescriptor &desc : list)
    {
        QString user = desc.usernameFn();
        if ( !user.isEmpty() && !deletePassword(desc.storageKey, user) )
            success = false;
    }
    return success;
}

bool CredentialStore::saveImportPassphrase(const QString &passphrase)
{
    FCT_IDENTIFICATION;

    // special one-time password when QLog import external DB
    return savePassword(IMPORT_PASSPHRASE_STORAGE_KEY, IMPORT_PASSPHRASE_USERNAME, passphrase) == 0;
}

QString CredentialStore::getImportPassphrase()
{
    FCT_IDENTIFICATION;

    // special one-time password when QLog import external DB
    return getPassword(IMPORT_PASSPHRASE_STORAGE_KEY, IMPORT_PASSPHRASE_USERNAME);
}

bool CredentialStore::deleteImportPassphrase()
{
    FCT_IDENTIFICATION;

    // special one-time password when QLog import external DB
    return deletePassword(IMPORT_PASSPHRASE_STORAGE_KEY, IMPORT_PASSPHRASE_USERNAME);
}
