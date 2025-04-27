#ifndef QLOG_SERVICE_EQSL_EQSL_H
#define QLOG_SERVICE_EQSL_EQSL_H

#include <QObject>
#include <logformat/LogFormat.h>

#include "core/QSLStorage.h"
#include "service/GenericQSOUploader.h"

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

    void update(const QDate&, bool, const QString&);
    void uploadAdif(const QByteArray&);
    void getQSLImage(const QSqlRecord&);
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) override;

signals:
    void updateProgress(int value);
    void updateStarted();
    void updateComplete(QSLMergeStat);
    void updateFailed(QString);
    void QSLImageFound(QString);
    void QSLImageError(QString);

public slots:
    virtual void abortRequest() override;

private:
    QSLStorage *qslStorage;
    QNetworkReply *currentReply;

    const QString DOWNLOAD_1ST_PAGE = "https://www.eQSL.cc/qslcard/DownloadInBox.cfm";
    const QString DOWNLOAD_2ND_PAGE = "https://www.eQSL.cc/downloadedfiles/";
    const QString UPLOAD_ADIF_PAGE = "https://www.eQSL.cc/qslcard/ImportADIF.cfm";
    const QString QSL_IMAGE_FILENAME_PAGE = "https://www.eQSL.cc/qslcard/GeteQSL.cfm";
    const QString QSL_IMAGE_DOWNLOAD_PAGE = "https://www.eQSL.cc";

    void get(const QList<QPair<QString, QString> > &);
    void downloadADIF(const QString &);
    void downloadImage(const QString &, const QString &, const qulonglong);
    QString QSLImageFilename(const QSqlRecord &);
    bool isQSLImageInCache(const QSqlRecord &, QString &);
    virtual void processReply(QNetworkReply*) override;
};

#endif // QLOG_SERVICE_EQSL_EQSL_H
