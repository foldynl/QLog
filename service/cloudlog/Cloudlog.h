#ifndef QLOG_SERVICE_CLOUDLOG_CLOUDLOG_H
#define QLOG_SERVICE_CLOUDLOG_CLOUDLOG_H

#include <QObject>
#include <QSqlRecord>
#include "service/GenericQSOUploader.h"
#include "core/CredentialStore.h"

class QNetworkReply;
class QNetworkAccessManager;

class CloudlogBase : public SecureServiceBase<CloudlogBase>
{
protected:
    const static QString SECURE_STORAGE_API_KEY;
    const static QString CONFIG_USERNAME_API_CONST;

public:
    explicit CloudlogBase() {};
    virtual ~CloudlogBase() {};

    DECLARE_SECURE_SERVICE(CloudlogBase);

    static QString getUsername() {return CONFIG_USERNAME_API_CONST;}
    static QString getLogbookAPIKey();
    static void saveLogbookAPIKey(const QString& newKey);

    // Callsign-specific API keys (the same concept as QRZ.com). The key above is the default
    // one, used for callsigns without a key of their own.
    static QString getCallsignLogbookAPIKey(const QString &callsign);
    static void saveCallsignLogbookAPIKey(const QString &newKey, const QString &callsign);
    static QStringList getLogbookAPIAddlCallsigns();
    static void setLogbookAPIAddlCallsigns(const QStringList &list);
    // the callsign of the table whose key is used for the callsign (the callsign itself or its
    // base callsign), empty for the default key
    static QString resolveAPIKeyCallsign(const QString &callsign);
    static QString resolveLogbookAPIKey(const QString &callsign);
    static bool isLogbookAPIKeyConfigured();

    static QString getAPIEndpoint();
    static void setAPIEndpoint(const QString &endpoint);
};

class CloudlogUploader : public GenericQSOUploader, private CloudlogBase
{
    Q_OBJECT

public:
    class StationProfile
    {
    public:
        int station_id;
        QString station_profile_name;
        QString station_gridsquare;
        QString station_callsign;
        bool station_active = false;

        static StationProfile fromJson(const QJsonObject &obj);
    };

    explicit CloudlogUploader(QObject *parent = nullptr);
    virtual ~CloudlogUploader();
    static QVariantMap generateUploadConfigMap(uint stationID);
    void uploadAdif(const QByteArray &data, uint stationID, const QString &apiKey);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;
    const QMap<uint, StationProfile>& getAvailableStationIDs() const;
    void sendStationInfoReq(const QString &callsign = QString());

public slots:
    virtual void abortRequest() override;
    void uploadContact(const QSqlRecord &record, uint stationID);

signals:
    void stationIDsUpdated();

protected:
    virtual void processReply(QNetworkReply* reply) override;
    virtual const QByteArray generateADIF(const QList<QSqlRecord> &qsos, QMap<QString,
                                          QString> *applTags = nullptr) override;
private:

    QMap<uint, StationProfile> availableStationIDs;
    QString stationInfoCallsign;
    QNetworkReply *currentReply;
    QList<QSqlRecord> queuedContacts4Upload;
    bool cancelUpload;
    uint stationID;

    QVariantMap parseResponse(const QByteArray &data);
};

#endif // QLOG_SERVICE_CLOUDLOG_CLOUDLOG_H
