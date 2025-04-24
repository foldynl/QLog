#include "GenericQSOUploader.h"
#include <QNetworkReply>
#include <QTextStream>
#include "logformat/AdiFormat.h"

GenericQSOUploader::GenericQSOUploader(const QStringList &uploadedFields, QObject *parent)
    : QObject{parent},
      uploadedFields(uploadedFields)
{
    nam = new QNetworkAccessManager(this);

    connect(nam, &QNetworkAccessManager::finished, this, &GenericQSOUploader::onNetworkReply);
}

const QByteArray GenericQSOUploader::generateADIF(const QList<QSqlRecord> qsos)
{
    QByteArray data;
    QTextStream stream(&data, QIODevice::ReadWrite);
    AdiFormat adi(stream);

    adi.exportStart();

    for ( const QSqlRecord &qso : qsos )
        adi.exportContact(stripRecord(qso));

    adi.exportEnd();
    stream.flush();
    return data;
}

const QSqlRecord GenericQSOUploader::stripRecord(const QSqlRecord &inRecord)
{
    if ( uploadedFields.isEmpty() )
        return inRecord;

    QSqlRecord ret;

    for ( int i = 0; i < inRecord.count(); i++ )
    {
        QSqlField curr = inRecord.field(i);
        if ( uploadedFields.contains(curr.name()) )
            ret.append(curr);
    }
    return ret;
}

void GenericQSOUploader::onNetworkReply(QNetworkReply *reply)
{
    processReply(reply);
}
