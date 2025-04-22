#ifndef QLOG_SERVICE_HRDLOG_HRDLOG_H
#define QLOG_SERVICE_HRDLOG_HRDLOG_H

#include <QObject>
#include <QSqlRecord>

class QNetworkReply;
class QNetworkAccessManager;


class HRDLog : public QObject
{
    Q_OBJECT
public:
    explicit HRDLog(QObject *parent = nullptr);
    ~HRDLog();

    static const QString getRegisteredCallsign();
    static const QString getUploadCode();
    static bool getOnAirEnabled();

    static void saveUploadCode(const QString &newUsername, const QString &newPassword);
    static void saveOnAirEnabled(bool state);

    void uploadAdif(const QByteArray &data,
                    const QVariant &contactID,
                    bool update = false);
    void uploadContact(QSqlRecord record);
    void uploadContacts(const QList<QSqlRecord>&);
    void sendOnAir(double freq, const QString &mode);

public slots:
    void abortRequest();

signals:
    void uploadFinished(bool result);
    void uploadedQSO(int);
    void uploadError(QString);

private slots:
    void processReply(QNetworkReply* reply);


private:
    QNetworkAccessManager* nam;
    QNetworkReply *currentReply;
    QList<QSqlRecord> queuedContacts4Upload;
    bool cancelUpload;

    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_CALLSIGN_KEY;
    const static QString CONFIG_ONAIR_ENABLED_KEY;
};

#endif // QLOG_SERVICE_HRDLOG_HRDLOG_H
