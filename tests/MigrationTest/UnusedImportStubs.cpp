#include <cstdlib>
#include "core/CredentialStore.h"
#include "core/LOVDatabase.h"
#include "core/PlatformParameterManager.h"

// Database backup and migration tests must not run the import workflow or
// access the system credential store. Keep unexpected calls fatal.
// Explicit aborts supply the noreturn guarantee omitted by Qt for MSVC.
bool PlatformParameterManager::applyParameters(const QList<PlatformParameter> &)
{
    qFatal("Database backup and migration tests must not apply import parameters");
    std::abort();
}

bool PlatformParameterManager::applyFlatpakFixedPaths()
{
    qFatal("Database backup and migration tests must not apply import parameters");
    std::abort();
}

bool PlatformParameterManager::applyProfileParameters(const QList<ProfileParameter> &)
{
    qFatal("Database backup and migration tests must not apply import parameters");
    std::abort();
}

CredentialRegistry &CredentialRegistry::instance()
{
    qFatal("Database backup and migration tests must not access the credential registry");
    std::abort();
}

const QList<CredentialDescriptor> CredentialRegistry::allDescriptors() const
{
    qFatal("Database backup and migration tests must not access the credential registry");
    std::abort();
}

CredentialStore::CredentialStore(QObject *parent) : QObject(parent)
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

bool CredentialStore::saveImportPassphrase(const QString &)
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

QString CredentialStore::getImportPassphrase()
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

bool CredentialStore::deleteImportPassphrase()
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

bool CredentialStore::deletePassword(const QString &, const QString &)
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

bool CredentialStore::decodePasswords(const QString &, QJsonArray &)
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

bool CredentialStore::restorePasswords(const QJsonArray &)
{
    qFatal("Database backup and migration tests must not access the credential store");
    std::abort();
}

bool LOVDatabase::discardPrepared(const QString &)
{
    qFatal("Database backup and migration tests must not discard prepared reference lists");
    std::abort();
}
