#ifndef QLOG_SERVICE_EMAILQSLSERVICE_H
#define QLOG_SERVICE_EMAILQSLSERVICE_H

#include <QObject>
#include <QString>
#include <QSqlRecord>
#include <QList>
#include <QJsonObject>
#include <QPixmap>
#include <QDateTime>
#include <QSslSocket>
#include <QThread>

#include "core/CredentialStore.h"

// Describes a single overlay drawn on the QSL card image.
// type == "TEXT"  → renders a merge-field value as text
// type == "BOX"   → renders a filled rounded rectangle (label is optional caption)
struct EmailQSLFieldOverlay
{
    // Common
    QString type       = QStringLiteral("TEXT"); // "TEXT" or "BOX"
    QString fieldName;                           // TEXT: merge key; BOX: optional caption label
    int     x          = 0;
    int     y          = 0;

    // TEXT-only
    QString fontFamily = QStringLiteral("Arial");
    int     fontSize   = 14;
    QString color      = QStringLiteral("#000000"); // text color / BOX border color
    bool    bold       = false;
    bool    italic     = false;

    // BOX-only
    int     width        = 120;
    int     height       = 60;
    QString fillColor    = QStringLiteral("#FFFF99"); // box fill (no alpha — use opacity)
    int     opacity      = 80;                        // fill opacity 0–100
    int     cornerRadius = 8;

    QJsonObject toJson() const;
    static EmailQSLFieldOverlay fromJson(const QJsonObject &obj);
};

// Static helpers for reading/writing all Email QSL settings.
// The SMTP password is kept in the secure credential store;
// everything else lives in QSettings under the "emailqsl/" group.
class EmailQSLBase : public SecureServiceBase<EmailQSLBase>
{
public:
    DECLARE_SECURE_SERVICE(EmailQSLBase)
    static const QString SECURE_STORAGE_KEY;

    enum EncryptionType
    {
        ENCRYPTION_NONE      = 0,
        ENCRYPTION_SSL_TLS   = 1,
        ENCRYPTION_STARTTLS  = 2
    };

    // --- SMTP connection ---
    static QString getSmtpHost();
    static void    setSmtpHost(const QString &host);
    static int     getSmtpPort();
    static void    setSmtpPort(int port);
    static int     getSmtpEncryption();
    static void    setSmtpEncryption(int enc);
    static QString getSmtpUsername();
    static void    setSmtpUsername(const QString &username);
    static QString getSmtpPassword();
    static void    saveSmtpCredentials(const QString &username, const QString &password);

    // --- Envelope / headers ---
    static QString getFromAddress();
    static void    setFromAddress(const QString &addr);
    static QString getFromName();
    static void    setFromName(const QString &name);
    static QString getSubjectTemplate();
    static void    setSubjectTemplate(const QString &tmpl);
    static QString getBodyTemplate();
    static void    setBodyTemplate(const QString &tmpl);

    // --- QSL card image & overlays ---
    static QString                      getCardImagePath();
    static void                         setCardImagePath(const QString &path);
    static QList<EmailQSLFieldOverlay>  getCardFieldOverlays();
    static void                         setCardFieldOverlays(const QList<EmailQSLFieldOverlay> &overlays);

    // --- Sent-tracking (stored in contacts.fields JSON) ---
    static QDateTime getEmailSentDateTime(const QSqlRecord &record);
    static bool      hasEmailBeenSentToCallsign(const QString &callsign, int excludeId = -1);
    static void      recordEmailSent(int contactId, const QSqlRecord &currentRecord);

    // --- Rendering helpers ---
    // Full render using saved QSettings (used when sending).
    static QPixmap  renderCard(const QSqlRecord &record);
    // Full render using an explicit image path and overlay list
    // (used by settings preview so unsaved changes are shown).
    static QPixmap  renderCard(const QString &imagePath,
                               const QSqlRecord &record,
                               const QList<EmailQSLFieldOverlay> &overlays);
    static QString  applyMergeFields(const QString &tmpl, const QSqlRecord &record);

    // Available merge keys (for display in settings UI)
    struct MergeField { QString key; QString description; };
    static QList<MergeField> availableMergeFields();

private:
    static QString fieldValue(const QString &key, const QSqlRecord &record);
};

// Worker object that runs the SMTP protocol on a background thread.
class SmtpWorker : public QObject
{
    Q_OBJECT
public:
    explicit SmtpWorker(const QString &host, int port, int encryption,
                        const QString &username, const QString &password,
                        const QString &fromAddress, const QString &fromName,
                        const QString &toAddress,
                        const QString &subject, const QString &body,
                        const QByteArray &imageData, const QString &imageName,
                        QObject *parent = nullptr);

public slots:
    void run();

signals:
    void finished(bool success, const QString &errorMessage);

private:
    bool     waitForResponse(QSslSocket *socket, QByteArray &out, int timeoutMs = 15000);
    int      responseCode(const QByteArray &response);
    bool     sendCommand(QSslSocket *socket, const QByteArray &cmd, int expectedCode,
                         QByteArray &response);
    QByteArray buildMimeMessage();

    QString    m_host;
    int        m_port;
    int        m_encryption;
    QString    m_username;
    QString    m_password;
    QString    m_fromAddress;
    QString    m_fromName;
    QString    m_toAddress;
    QString    m_subject;
    QString    m_body;
    QByteArray m_imageData;
    QString    m_imageName;
};

// High-level service used by the UI.  Call sendEmailQSL() and connect to
// sendFinished() for result notification.
class EmailQSLService : public QObject
{
    Q_OBJECT
public:
    explicit EmailQSLService(QObject *parent = nullptr);

    void sendEmailQSL(const QSqlRecord &record);
    void testConnection(const QString &host, int port, int encryption,
                        const QString &username, const QString &password);

signals:
    void sendFinished(bool success, const QString &message);
    void testFinished(bool success, const QString &message);
};

#endif // QLOG_SERVICE_EMAILQSLSERVICE_H
