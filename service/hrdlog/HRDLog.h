#ifndef QLOG_SERVICE_HRDLOG_HRDLOG_H
#define QLOG_SERVICE_HRDLOG_HRDLOG_H

#include <QObject>
#include <QSqlRecord>
#include "service/GenericQSOUploader.h"
#include "core/CredentialStore.h"

class QNetworkReply;
class QNetworkAccessManager;


class HRDLogBase : public SecureServiceBase<HRDLogBase>
{
protected:
    const static QString SECURE_STORAGE_KEY;

public:
    explicit HRDLogBase() {};
    virtual ~HRDLogBase() {};

    DECLARE_SECURE_SERVICE(HRDLogBase);

    static const QString getRegisteredCallsign();
    static const QString getUploadCode();
    static bool getOnAirEnabled();
    static void saveUploadCode(const QString &newUsername, const QString &newPassword);
    static void saveOnAirEnabled(bool state);

    // Callsign-specific accounts - HRDLog has an account (upload code) per callsign. The account
    // above is the default one, used for its callsign and for QSOs without station callsign.
    static QStringList getAddlCallsigns();
    static void setAddlCallsigns(const QStringList &list);
    static QString getCallsignUploadCode(const QString &callsign);
    static void saveCallsignUploadCode(const QString &newCode, const QString &callsign);
    static QString resolveAccountCallsign(const QString &stationCallsign);
    static bool isConfigured();
};

class HRDLogUploader : public GenericQSOUploader, private HRDLogBase
{
    Q_OBJECT

public:
    static QStringList uploadedFields;

    explicit HRDLogUploader(QObject *parent = nullptr);
    virtual ~HRDLogUploader();

    void uploadAdif(const QByteArray &data,
                    const QVariant &contactID,
                    const QString &accountCallsign,
                    bool update = false);
    void uploadContact(const QSqlRecord &record);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;
    void sendOnAir(double freq, const QString &mode, const QString &stationCallsign);

public slots:
    virtual void abortRequest() override;

protected:
    virtual void processReply(QNetworkReply* reply) override;

private:
    QNetworkReply *currentReply;
    QList<QSqlRecord> queuedContacts4Upload;
    static QString recordStationCallsign(const QSqlRecord &record);
    bool cancelUpload;

    const QString API_LOG_UPLOAD_URL = "https://robot.hrdlog.net/NewEntry.aspx";
    const QString API_ONAIR_URL = "https://robot.hrdlog.net/OnAir.aspx";
};

#endif // QLOG_SERVICE_HRDLOG_HRDLOG_H
