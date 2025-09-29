#ifndef QLOG_SERVICE_CLUBLOG_CLUBLOG_H
#define QLOG_SERVICE_CLUBLOG_CLUBLOG_H

#include <QObject>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QHash>
#include <QRegularExpression>
#include "service/GenericQSOUploader.h"

class QNetworkReply;
class QNetworkAccessManager;

class ClubLogBase
{
public:
    explicit ClubLogBase() {};
    virtual ~ClubLogBase() {} ;

    static const QString getEmail();
    static bool isUploadImmediatelyEnabled();
    static const QString getPassword();
    static void saveUsernamePassword(const QString &, const QString &);
    static void saveUploadImmediatelyConfig(bool value);
    static const QString getCTYUrl()  {return QString("https://cdn.clublog.org/cty.php?api=%1").arg(API_KEY);};
protected:
    const static QString SECURE_STORAGE_KEY;
    const static QString API_KEY ;
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
