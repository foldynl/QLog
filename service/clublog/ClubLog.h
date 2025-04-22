#ifndef QLOG_SERVICE_CLUBLOG_CLUBLOG_H
#define QLOG_SERVICE_CLUBLOG_CLUBLOG_H

#include <QObject>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QHash>

class QNetworkReply;
class QNetworkAccessManager;

class ClubLog : public QObject
{
    Q_OBJECT
public:
    enum OnlineCommand
    {
      INSERT_QSO,
      UPDATE_QSO,
      DELETE_QSO
    };
    explicit ClubLog(QObject *parent = nullptr);
    ~ClubLog();

    static const QString getEmail();
    static bool isUploadImmediatelyEnabled();
    static const QString getPassword();

    static void saveUsernamePassword(const QString &, const QString &);
    static void saveUploadImmediatelyConfig(bool value);

    static QStringList supportedDBFields;

    void uploadAdif(QByteArray &data,
                    const QString &uploadCallsign,
                    bool clearFlag = false);
    void sendRealtimeRequest(const OnlineCommand command,
                             const QSqlRecord &record,
                             const QString &uploadCallsign);

signals:
    void uploadFileOK(QString);
    void QSOUploaded();
    void uploadError(QString);

public slots:
    void processReply(QNetworkReply* reply);
    void abortRequest();
    void insertQSOImmediately(const QSqlRecord &record);
    void updateQSOImmediately(const QSqlRecord &record);
    void deleteQSOImmediately(const QSqlRecord &record);

private:
    QNetworkAccessManager* nam;
    QList<QNetworkReply*> activeReplies;
    const QString generateUploadCallsign(const QSqlRecord &record) const;
    QSqlRecord stripRecord(const QSqlRecord&);
    QSqlQuery query_updateRT;
    QHash<unsigned long long, QSqlRecord> RTupdatesInProgress;

    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_EMAIL_KEY;
    const static QString CONFIG_UPLOAD_IMMEDIATELY_KEY;
};

#endif // QLOG_SERVICE_CLUBLOG_CLUBLOG_H
