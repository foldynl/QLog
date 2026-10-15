#include "core/CredentialStore.h"
#include "core/LOVDatabase.h"
#include "core/PlatformParameterManager.h"

// Database backup and migration tests must not run the import workflow or
// access the system credential store. Keep unexpected calls fatal.
bool PlatformParameterManager::applyParameters(const QList<PlatformParameter> &)
{
    qFatal("Database backup and migration tests must not apply import parameters");
}

bool PlatformParameterManager::applyFlatpakFixedPaths()
{
    qFatal("Database backup and migration tests must not apply import parameters");
}

bool PlatformParameterManager::applyProfileParameters(const QList<ProfileParameter> &)
{
    qFatal("Database backup and migration tests must not apply import parameters");
}

CredentialRegistry &CredentialRegistry::instance()
{
    qFatal("Database backup and migration tests must not access the credential registry");
}

const QList<CredentialDescriptor> CredentialRegistry::allDescriptors() const
{
    qFatal("Database backup and migration tests must not access the credential registry");
}

CredentialStore::CredentialStore(QObject *parent) : QObject(parent)
{
    qFatal("Database backup and migration tests must not access the credential store");
}

bool CredentialStore::saveImportPassphrase(const QString &)
{
    qFatal("Database backup and migration tests must not access the credential store");
}

QString CredentialStore::getImportPassphrase()
{
    qFatal("Database backup and migration tests must not access the credential store");
}

bool CredentialStore::deleteImportPassphrase()
{
    qFatal("Database backup and migration tests must not access the credential store");
}

bool CredentialStore::deletePassword(const QString &, const QString &)
{
    qFatal("Database backup and migration tests must not access the credential store");
}

bool CredentialStore::decodePasswords(const QString &, QJsonArray &)
{
    qFatal("Database backup and migration tests must not access the credential store");
}

bool CredentialStore::restorePasswords(const QJsonArray &)
{
    qFatal("Database backup and migration tests must not access the credential store");
}

bool LOVDatabase::discardPrepared(const QString &)
{
    qFatal("Database backup and migration tests must not discard prepared reference lists");
}
