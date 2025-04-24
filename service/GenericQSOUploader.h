#ifndef QLOG_SERVICE_GENERICQSOUPLOADER_H
#define QLOG_SERVICE_GENERICQSOUPLOADER_H

#include <QObject>
#include <QSqlRecord>
#include <QNetworkAccessManager>

class GenericQSOUploader : public QObject
{
    Q_OBJECT
public:
    explicit GenericQSOUploader(const QStringList &uploadedFields, QObject *parent = nullptr);
    virtual ~GenericQSOUploader() {nam->deleteLater();};
    virtual void uploadQSOList(const QList<QSqlRecord>& qsos, const QVariantMap &addlParams) = 0;

signals:
    void uploadFinished();
    void uploadError(QString);
    void uploadedQSO(qulonglong);

public slots:
    virtual void abortRequest() = 0;

protected:
    virtual const QByteArray generateADIF(const QList<QSqlRecord> qsos);
    virtual const QSqlRecord stripRecord(const QSqlRecord &record);
    virtual void processReply(QNetworkReply *reply) = 0;
    QNetworkAccessManager* getNetworkAccessManager() {return nam;};

private:
    QNetworkAccessManager* nam;
    QStringList uploadedFields;

private slots:
    void onNetworkReply(QNetworkReply *reply);
};

#endif // QLOG_SERVICE_GENERICQSOUPLOADER_H
