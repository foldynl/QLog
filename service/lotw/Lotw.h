#ifndef QLOG_SERVISE_LOTW_LOTW_H
#define QLOG_SERVISE_LOTW_LOTW_H

#include <QObject>
#include <QNetworkReply>
#include <logformat/LogFormat.h>
#include "service/GenericQSOUploader.h"

class QNetworkAccessManager;

class LotwBase
{
public:
    explicit LotwBase() {};
    virtual ~LotwBase() {};

    static const QString getUsername();
    static const QString getPassword();
    static const QString getTQSLPath(const QString &defaultPath = QDir::rootPath());

    static void saveUsernamePassword(const QString&, const QString&);
    static void saveTQSLPath(const QString&);

protected:
    static const QString SECURE_STORAGE_KEY;
    static const QString CONFIG_USERNAME_KEY;
};

class LotwUploader : public GenericQSOUploader, private LotwBase
{
    Q_OBJECT

public:
    static QStringList uploadedFields;

    explicit LotwUploader(QObject *parent = nullptr);
    virtual ~LotwUploader();

    void update(const QDate &, bool, const QString &);
    void uploadAdif(const QByteArray &);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

signals:
    void updateProgress(int value);
    void updateStarted();
    void updateComplete(QSLMergeStat update);
    void updateFailed(QString);

public slots:
    virtual void abortRequest() override;

private:
    QNetworkReply *currentReply;
    QTemporaryFile file;
    const QString ADIF_API = "https://lotw.arrl.org/lotwuser/lotwreport.adi";

    virtual void processReply(QNetworkReply* reply) override;
    void get(QList<QPair<QString, QString>> params);
};

#endif // QLOG_SERVISE_LOTW_LOTW_H
