#ifndef QLOG_SERVICE_HRDLOG_HRDLOG_H
#define QLOG_SERVICE_HRDLOG_HRDLOG_H

#include <QObject>
#include <QSqlRecord>
#include "service/GenericQSOUploader.h"

class QNetworkReply;
class QNetworkAccessManager;


class HRDLogBase
{
public:
    explicit HRDLogBase() {};
    virtual ~HRDLogBase() {};

    static const QString getRegisteredCallsign();
    static const QString getUploadCode();
    static bool getOnAirEnabled();
    static void saveUploadCode(const QString &newUsername, const QString &newPassword);
    static void saveOnAirEnabled(bool state);

protected:
    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_CALLSIGN_KEY;
    const static QString CONFIG_ONAIR_ENABLED_KEY;
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
                    bool update = false);
    void uploadContact(const QSqlRecord &record);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;
    void sendOnAir(double freq, const QString &mode);

public slots:
    virtual void abortRequest() override;

protected:
    virtual void processReply(QNetworkReply* reply) override;

private:
    QNetworkReply *currentReply;
    QList<QSqlRecord> queuedContacts4Upload;
    bool cancelUpload;

    const QString API_LOG_UPLOAD_URL = "https://robot.hrdlog.net/NewEntry.aspx";
    const QString API_ONAIR_URL = "https://robot.hrdlog.net/OnAir.aspx";
};

#endif // QLOG_SERVICE_HRDLOG_HRDLOG_H
