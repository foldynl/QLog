#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "PotaApp.h"
#include "core/debug.h"
#include "data/Callsign.h"

MODULE_IDENTIFICATION("qlog.service.potaapp.potaapp");

PotaAppActivatorDownloader::PotaAppActivatorDownloader(QObject *parent) :
    QObject(parent),
    nam(new QNetworkAccessManager(this))
{
    FCT_IDENTIFICATION;

    connect(nam, &QNetworkAccessManager::finished, this, &PotaAppActivatorDownloader::processReply);
}

PotaAppActivatorDownloader::~PotaAppActivatorDownloader()
{
    FCT_IDENTIFICATION;

    if ( nam ) nam->deleteLater();
}

void PotaAppActivatorDownloader::swapActivators(ActivatorStorage &a)
{
    FCT_IDENTIFICATION;

    QMutexLocker locker(&activatorLock);
    a.swap(activators);
}

void PotaAppActivatorDownloader::updateActivators()
{
    FCT_IDENTIFICATION;

    if ( currReply )
    {
        qCWarning(runtime) << "request is still running";
        currReply->abort();
        currReply = nullptr;
    }
    const QUrl url(API_URL);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "QLog");
    currReply = nam->get(req);
}


void PotaAppActivatorDownloader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    currReply = nullptr;

    if (!reply) return;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "POTA API error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;
        reply->deleteLater();
        return;
    }

    const QByteArray body = reply->readAll();
    if ( body.isEmpty() ) return;

    // Parse JSON array of spots
    QJsonParseError jerr{};

    const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isArray())
    {
        qCWarning(runtime) << "POTA JSON Error" << jerr.errorString();
        return;
    }

    const QJsonArray arr = doc.array();
    ActivatorStorage newActivators;

    for ( const QJsonValue &val : arr )
    {
        if (!val.isObject()) continue;

        const QJsonObject o = val.toObject();

        //qCDebug(runtime) << "Received Spot" << o;

        if ( o.value("comments").toString().toUpper().contains("QRT") )
        {
            qCDebug(runtime) << "QRT - skipping";
            continue;
        }

        POTASpot s;

        s.spotId = static_cast<quint64>(o.value("spotId").toVariant().toULongLong());
        s.activator = o.value("activator").toString().toUpper();
        Callsign activator(s.activator);
        if (activator.isValid()) s.activatorBaseCallsign = activator.getBase();
        s.frequency = o.value("frequency").toString().toDouble() / 1000;
        s.mode = o.value(("mode")).toString();
        s.reference = o.value(("reference")).toString().toUpper();
        s.parkName = o.value(("parkName")).toString();
        s.spotter = o.value(("spotter")).toString().toUpper();
        s.comments = o.value(("comments")).toString();
        s.source = o.value(("source")).toString();
        s.name = o.value(("name")).toString();
        s.locationDesc = o.value(("locationDesc")).toString();

        const QString st = o.value(("spotTime")).toString();
        s.spotTime = QDateTime::fromString(st, Qt::ISODate);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        s.spotTime.setTimeZone(QTimeZone::UTC);
#else
        s.spotTime.setTimeSpec(Qt::UTC);
#endif

        if ( !s.activator.isEmpty() && s.frequency > 0.0 && !s.mode.isEmpty() )
        {
            qCDebug(runtime) << "Adding"
                             << s.activator << s.frequency << s.mode
                             << s.reference << s.comments;
            newActivators.insert(s.activatorBaseCallsign, s);
        }
    }
    // atomic swap
    activatorLock.lock();
    activators.swap(newActivators);
    activatorLock.unlock();
    emit activatorsUpdated();
    reply->deleteLater();
}
