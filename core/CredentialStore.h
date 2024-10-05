#ifndef QLOG_CORE_CREDENTIALSTORE_H
#define QLOG_CORE_CREDENTIALSTORE_H

#include <QObject>
#include <QString>

class CredentialStore : public QObject
{
    Q_OBJECT
public:
    explicit CredentialStore(QObject *parent = nullptr);
    static CredentialStore* instance()
    {
        static CredentialStore instance;
        return &instance;
    };

    int savePassword(const QString &storage_key, const QString &user, const QString &pass);
    QString getPassword(const QString &storage_key, const QString &user);
    void deletePassword(const QString &storage_key, const QString &user);

};

#endif // QLOG_CORE_CREDENTIALSTORE_H
