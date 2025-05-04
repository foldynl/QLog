#ifndef QLOG_SERVICE_EQSL_EQSL_H
#define QLOG_SERVICE_EQSL_EQSL_H

#include <QObject>
#include <logformat/LogFormat.h>

#include "core/QSLStorage.h"
#include "service/GenericQSOUploader.h"
#include "service/GenericQSLDownloader.h"

class QNetworkAccessManager;
class QNetworkReply;

class EQSLBase
{
public:
    explicit EQSLBase() {};
    virtual ~EQSLBase() {};

    static const QString getUsername();
    static const QString getPassword();
    static void saveUsernamePassword(const QString&, const QString&);

protected:
    static const QString SECURE_STORAGE_KEY;
    static const QString CONFIG_USERNAME_KEY;
};

class EQSLUploader : public GenericQSOUploader, private EQSLBase
{
    Q_OBJECT
public:
    static QStringList uploadedFields;

    explicit EQSLUploader(QObject *parent = nullptr);
    virtual ~EQSLUploader();

    void uploadAdif(const QByteArray&);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

public slots:
    virtual void abortRequest() override;

private:
    const QString UPLOAD_ADIF_PAGE = "https://www.eQSL.cc/qslcard/ImportADIF.cfm";
    QNetworkReply *currentReply;

    virtual void processReply(QNetworkReply*) override;
};

class EQSLQSLDownloader : public GenericQSLDownloader, private EQSLBase
{
    Q_OBJECT

public:
    explicit EQSLQSLDownloader(QObject *parent = nullptr);
    virtual ~EQSLQSLDownloader();

    virtual void receiveQSL(const QDate &, bool, const QString &) override;
    void getQSLImage(const QSqlRecord&);

signals:
    void QSLImageFound(QString);
    void QSLImageError(QString);

public slots:
    virtual void abortDownload() override;

private:
    QNetworkReply *currentReply;
    QSLStorage *qslStorage;
    const QString DOWNLOAD_1ST_PAGE = "https://www.eQSL.cc/qslcard/DownloadInBox.cfm";
    const QString DOWNLOAD_2ND_PAGE = "https://www.eQSL.cc/downloadedfiles/";
    const QString QSL_IMAGE_FILENAME_PAGE = "https://www.eQSL.cc/qslcard/GeteQSL.cfm";
    const QString QSL_IMAGE_DOWNLOAD_PAGE = "https://www.eQSL.cc";

    virtual void processReply(QNetworkReply* reply) override;
    void get(const QList<QPair<QString, QString> > &);
    void downloadADIF(const QString &);
    void downloadImage(const QString &, const QString &, const qulonglong);
    QString QSLImageFilename(const QSqlRecord &);
    bool isQSLImageInCache(const QSqlRecord &, QString &);
};

#endif // QLOG_SERVICE_EQSL_EQSL_H
