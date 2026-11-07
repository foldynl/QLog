#ifndef QLOG_SERVICE_CLUBLOG_CLUBLOG_H
#define QLOG_SERVICE_CLUBLOG_CLUBLOG_H

#include <QObject>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QHash>
#include <QRegularExpression>
#include "service/GenericQSOUploader.h"
#include "core/CredentialStore.h"

class QNetworkReply;
class QNetworkAccessManager;

class ClubLogBase : public SecureServiceBase<ClubLogBase>
{
protected:
    const static QString SECURE_STORAGE_KEY;
    const static QString API_KEY ;

public:
    explicit ClubLogBase() {};
    virtual ~ClubLogBase() {} ;

    DECLARE_SECURE_SERVICE(ClubLogBase);

    static const QString getEmail();
    static bool isUploadImmediatelyEnabled();
    static const QString getPasswd();
    static void saveUsernamePassword(const QString &, const QString &);
    static void saveUploadImmediatelyConfig(bool value);

    // Additional accounts - a Club Log account holds the logs of its callsigns. The account
    // above is the default one, used for callsigns of no additional account.
    static QStringList getAddlAccounts();
    static void setAddlAccounts(const QStringList &emails);
    static QStringList getAccountCallsigns(const QString &email);
    static void setAccountCallsigns(const QString &email, const QStringList &callsigns);
    static QString getAccountPasswd(const QString &email);
    static void saveAccountPasswd(const QString &newPassword, const QString &email);
    static QString resolveEmail(const QString &stationCallsign);
    static const QString getCTYUrl()  {return QString("https://cdn.clublog.org/cty.php?api=%1").arg(API_KEY);};
};

class ClubLogUploader : public GenericQSOUploader, private ClubLogBase
{
    Q_OBJECT

public:
    static QStringList uploadedFields;
    static QVariantMap generateUploadConfigMap(const QString uploadCallsign, bool clearFlag)
    {
        return QVariantMap({{"uploadCallsign", uploadCallsign}, {"clearFlag", clearFlag}});
    }

    enum OnlineUploadCommand
    {
      INSERT_QSO,
      UPDATE_QSO,
      DELETE_QSO
    };

    explicit ClubLogUploader(QObject *parent = nullptr);
    virtual ~ClubLogUploader();

    void uploadAdif(const QByteArray &data,
                    const QString &uploadCallsign,
                    bool clearFlag = false);
    void sendRealtimeRequest(const OnlineUploadCommand command,
                             const QSqlRecord &record,
                             const QString &uploadCallsign);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override;
    void insertQSOImmediately(const QSqlRecord &record);
    void updateQSOImmediately(const QSqlRecord &record);
    void deleteQSOImmediately(const QSqlRecord &record);

protected:
    virtual void processReply(QNetworkReply *reply) override;

private:
    const QString API_LIVE_UPLOAD_URL = "https://clublog.org/realtime.php";
    const QString API_LIVE_DELETE_URL = "https://clublog.org/delete.php";
    const QString API_LOG_UPLOAD_URL = "https://clublog.org/putlogs.php";

    QList<QNetworkReply*> activeReplies;
    QSqlQuery query_updateRT;
    QHash<qulonglong, QSqlRecord> RTupdatesInProgress;
    QRegularExpression rx;

    const QString generateUploadCallsign(const QSqlRecord &record) const;
};

#endif // QLOG_SERVICE_CLUBLOG_CLUBLOG_H
