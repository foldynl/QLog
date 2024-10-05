#include <QEventLoop>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <qt5keychain/keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif
#include <QApplication>
#include <QMessageBox>
#include "CredentialStore.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.credentialstore");

CredentialStore::CredentialStore(QObject *parent) : QObject(parent)
{
    FCT_IDENTIFICATION;
}

int CredentialStore::savePassword(const QString &storage_key, const QString &user, const QString &pass)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << storage_key << " " << user;

    if ( user.isEmpty()
         || storage_key.isEmpty()
         || pass.isEmpty() )
    {
       return 1;
    }

    QString locStorageKey = storage_key;
    locStorageKey.prepend(qApp->applicationName() + ":");
    QString locUser = user;
    QString locPass = pass;

    using namespace QKeychain;

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

    if ( job.error() && job.error() != EntryNotFound )
    {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Critical"),
                              QMessageBox::tr("Cannot save a password for %1 to the Credential Store").arg(storage_key)
                              + "<p>"
                              + job.errorString());
        qWarning() << "Cannot save a password. Error " << job.errorString();
        return 1;
    }

    return 0;
}

QString CredentialStore::getPassword(const QString &storage_key, const QString &user)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << storage_key << " " << user;

    if ( user.isEmpty()
         || storage_key.isEmpty() )
    {
        return QString();
    }

    QString locStorageKey = storage_key;
    locStorageKey.prepend(qApp->applicationName() + ":");
    QString locUser = user;
    QString pass;

    using namespace QKeychain;

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
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Critical"),
                              QMessageBox::tr("Cannot get a password for %1 from the Credential Store").arg(storage_key)
                              + "<p>"
                              + job.errorString());
        qCDebug(runtime) << "Cannot get a password. Error " << job.errorString();
        return QString();
    }

    return pass;
}

void CredentialStore::deletePassword(const QString &storage_key, const QString &user)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << storage_key << " " << user;

    if ( user.isEmpty()
         || storage_key.isEmpty() )
    {
        return;
    }

    QString locStorageKey = storage_key;
    locStorageKey.prepend(qApp->applicationName() + ":");
    QString locUser = user;

    using namespace QKeychain;

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

    return;
}
