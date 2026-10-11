#ifndef QLOG_CORE_CREDENTIALSTORE_H
#define QLOG_CORE_CREDENTIALSTORE_H

#include <QObject>
#include <QString>
#include <QJsonArray>

// - Central registry of all services that use the CredentialStore.
// - Each service registers its credential descriptor statically at load time
//   using the DECLARE_SECURE_SERVICE macro.
// why:
// - QtKeychain does not allow listing all stored passwords.
// - To migrate data or validate access, we must know which services exist.
// - This registry provides that authoritative list.

struct CredentialDescriptor
{
    QString storageKey;                     // unique key (e.g. "eQSL", "HRDLog")
    std::function<QString()> usernameFn;    // callback returning the username
};

class CredentialRegistry
{
private:

    QList<std::function<QList<CredentialDescriptor>()>> callbacks;

public:
    static CredentialRegistry &instance();

    // Register a service descriptor callback.
    // Each service provides a lambda returning one or more CredentialDescriptors.
    void add(const QString &serviceName,
             const std::function<QList<CredentialDescriptor>()> &fn);

    // Returns all known credential descriptors (collected from all services).
    const QList<CredentialDescriptor> allDescriptors() const;
};

// Macro used in each service to force the presence of registerCredentials()
// and to auto-execute it during static initialization.
// If the developer forgets to implement registerCredentials(),
// linking will fail with "undefined reference".
#define DECLARE_SECURE_SERVICE(cls) \
    static void registerCredentials(); \
    static int cls##ForceRegistration() { \
        cls::registerCredentials(); \
        return 0; \
    } \
    static int cls##RegistrationDummy;

#define REGISTRATION_SECURE_SERVICE(cls) \
    int cls::cls##RegistrationDummy = cls::cls##ForceRegistration()

template <typename Derived>
class SecureServiceBase;  // forward declaration

// - Provides a single point of access to platform credential storage (QtKeychain).
// - Ensures that only registered services can access credentials.
// - Protected API: only subclasses (service classes) can call get/save/delete.
// - Validation layer: prevents ad-hoc access from unregistered services.

class CredentialStore : public QObject
{
    Q_OBJECT

    template <typename Derived>
    friend class SecureServiceBase;
    friend class LogDatabase;

public:

    static CredentialStore* instance()
    {
        static CredentialStore instance;
        return &instance;
    };

    bool exportPasswords(const QString &passphrase);
    bool importPasswords(const QString &passphrase);
    bool deleteAllPasswords();

    // Methods for storing import passphrase in SecureStore during application restart
    bool saveImportPassphrase(const QString &passphrase);
    QString getImportPassphrase();
    bool deleteImportPassphrase();

private:
    explicit CredentialStore(QObject *parent = nullptr);

    // Protected methods — accessible only from derived service classes.
    // Direct use elsewhere is discouraged by design.
    int savePassword(const QString &storage_key, const QString &user, const QString &pass);
    QString getPassword(const QString &storage_key, const QString &user, bool *success = nullptr);
    bool deletePassword(const QString &storage_key, const QString &user);
    bool decodePasswords(const QString &passphrase, QJsonArray &credentials);
    bool restorePasswords(const QJsonArray &credentials);

private:
    // Verifies that the given service (storage_key) is registered.
    // Prevents code from querying or saving passwords.
    //bool isRegisteredService(const QString &storage_key) const;
};


// - Base class for all components that manage user credentials.
// - Enforces per-service registration and provides a safe API wrapper
//   for accessing the CredentialStore.
// - Prevents ad-hoc direct usage of CredentialStore.
//

template <typename Derived>
class SecureServiceBase {

public:
    virtual ~SecureServiceBase() {};

protected:
    static QString getPassword(const QString &storageKey, const QString &user)
    {
        return CredentialStore::instance()->getPassword(storageKey, user);
    }

    static void savePassword(const QString &storageKey, const QString &user, const QString &pass)
    {
        CredentialStore::instance()->savePassword(storageKey, user, pass);
    }

    static void deletePassword(const QString &storageKey, const QString &user)
    {
        CredentialStore::instance()->deletePassword(storageKey, user);
    }

    SecureServiceBase() {
        // --- Compile-time enforcement of registerCredentials() existence ---
        typedef void (*RegFn)();
        static_assert(
            std::is_same<RegFn, decltype(&Derived::registerCredentials)>::value,
            "Derived class must implement: DECLARE_SECURE_SERVICE macro;"
        );
    }
};

#endif // QLOG_CORE_CREDENTIALSTORE_H
