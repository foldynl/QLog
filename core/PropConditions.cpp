#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDomDocument>
#include "PropConditions.h"
#include "debug.h"

//#define FLUX_URL "https://services.swpc.noaa.gov/products/summary/10cm-flux.json"
#define K_INDEX_URL "https://www.hamqsl.com/solarxml.php"
#define SOLAR_SUMMARY_IMG "https://www.hamqsl.com/solar101vhf.php"
#define AURORA_MAP "https://services.swpc.noaa.gov/json/ovation_aurora_latest.json"
#define MUF_POINTS "https://prop.kc2g.com/api/stations.json?maxage=2700"

// the resend mechanism was implemented only because of a issue with prop.kc2g.com
// This site has IPv4 and IPv6 DNS record, and if the notebook is IPv4 only, QT uses an IPv6
// address for the first attempt and an IPv4 address for the second attempt.
// This resulted in a long interval before information was obtained from this server.
// Resend mechanism accelerates all this.
#define RESEND_ATTEMPTS 3
//intervals are defined in seconds
#define RESEND_BASE_INTERVAL 5
#define BASE_UPDATE_INTERVAL (15 * 60)

MODULE_IDENTIFICATION("qlog.core.conditions");

PropConditions::PropConditions(QObject *parent) : QObject(parent)
{
    FCT_IDENTIFICATION;

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, &PropConditions::processReply);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PropConditions::update);
    update();
    timer->start(BASE_UPDATE_INTERVAL * 1000);
}

void PropConditions::update()
{
    FCT_IDENTIFICATION;

    nam->get(QNetworkRequest(QUrl(SOLAR_SUMMARY_IMG)));
    nam->get(QNetworkRequest(QUrl(K_INDEX_URL)));
    nam->get(QNetworkRequest(QUrl(AURORA_MAP)));
    nam->get(QNetworkRequest(QUrl(MUF_POINTS)));
}

void PropConditions::processReply(QNetworkReply* reply)
{
    FCT_IDENTIFICATION;

    QByteArray data = reply->readAll();

    qCDebug(runtime) << data;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qCDebug(runtime) << reply->error()
                     << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                     << reply->url();

    if ( reply->error() == QNetworkReply::NoError
         && replyStatusCode >= 200 && replyStatusCode < 300 )
    {
        failedRequests[reply->url()] = 0;

        if (reply->url() == QUrl(SOLAR_SUMMARY_IMG))
        {
            QFile file(solarSummaryFile());

            if ( file.open(QIODevice::WriteOnly))
            {
                file.write(data);
                file.flush();
                file.close();
            }
        }
        else if (reply->url() == QUrl(K_INDEX_URL))
        {
            QDomDocument doc;

            if ( !doc.setContent(data) )
            {
                qWarning() << "Cannot parse response from " << K_INDEX_URL;
                return;
            }

            QDomNodeList solarData = doc.elementsByTagName("solardata");
            QDomNode n = solarData.item(0);

            if ( n.isNull() )
            {
                qWarning() << "Cannot find solardata in " << K_INDEX_URL;
                return;
            }

            QDomElement aindex = n.firstChildElement("aindex");
            QDomElement kindex = n.firstChildElement("kindex");
            QDomElement solarflux = n.firstChildElement("solarflux");

            if ( !aindex.isNull() )
            {
                a_index = aindex.text().toInt();
                qCDebug(runtime) << "A-Index: " << a_index;
                a_index_last_update = QDateTime::currentDateTime();
                emit AIndexUpdated();
            }

            if ( !kindex.isNull() )
            {
                k_index = kindex.text().toDouble();
                qCDebug(runtime) << "K-Index: " << k_index;
                k_index_last_update = QDateTime::currentDateTime();
                emit KIndexUpdated();
            }

            if ( !solarflux.isNull() )
            {
                flux = solarflux.text().toInt();
                qCDebug(runtime) << "Flux: " << flux;
                flux_last_update = QDateTime::currentDateTime();
                emit fluxUpdated();
            }
        }
        else if (reply->url() == QUrl(AURORA_MAP))
        {
            auroraMap.clear();

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if ( ! doc.isNull() )
            {
                double skipElement = 0.0;

                qCDebug(runtime) << "Aurora forecast Time:" << doc["Forecast Time"].toString();
                const QJsonArray &jsonArray = doc["coordinates"].toArray();
                for (const QJsonValue &value : jsonArray)
                {
                    QJsonArray obj = value.toArray();
                    if ( obj.size() == 3 )
                    {
                        double longitute = obj[0].toDouble();
                        double latitude = obj[1].toDouble();
                        double prob = obj[2].toDouble();
                        auroraMap.addPoint(longitute, latitude, prob, &skipElement);
                    }
                }
                auroraMap_last_update = QDateTime::currentDateTime();
                emit auroraMapUpdated();
            }
        }
        else if (reply->url() == QUrl(MUF_POINTS))
        {
            mufMap.clear();

            QJsonDocument doc = QJsonDocument::fromJson(data);

            if ( ! doc.isNull() )
            {
                double skipElement = 0.0;

                const QJsonArray &jsonArray = doc.array();
                for (const QJsonValue &value : jsonArray)
                {
                    QJsonObject obj = value.toObject();
                    QJsonObject station = obj["station"].toObject();
                    double longitute = station["longitude"].toString().toDouble();
                    double latitude = station["latitude"].toString().toDouble();
                    double muf = obj["mufd"].toDouble();
                    mufMap.addPoint(longitute, latitude, muf, &skipElement);
                }
                mufMap_last_update = QDateTime::currentDateTime();
                emit mufMapUpdated();
            }
        }
        reply->deleteLater();
        emit conditionsUpdated();
    }
    else
    {
        repeateRequest(reply->url());
        reply->deleteLater();
    }
}

void PropConditions::repeateRequest(const QUrl &url)
{
    FCT_IDENTIFICATION;

    failedRequests[url]++;

    if ( failedRequests[url] <= RESEND_ATTEMPTS )
    {
        int resendInterval = RESEND_BASE_INTERVAL * failedRequests[url];
        qCDebug(runtime) << "Scheduled URL request resend" << resendInterval << "; URL:" << url.toString();

        QTimer::singleShot(1000 * RESEND_BASE_INTERVAL * failedRequests[url], this, [this, url]()
        {
            qCDebug(runtime) << "Resending request" << url.toString();
            nam->get(QNetworkRequest(url));
        });
    }
    else
    {
        qCDebug(runtime) << "Propagation - detected consecutive errors from" << url.toString();
    }
}

PropConditions::~PropConditions() {
    delete nam;
}

bool PropConditions::isFluxValid()
{
    FCT_IDENTIFICATION;
    bool ret = false;

    qCDebug(runtime)<<"Date valid: " << flux_last_update.isValid() << " last_update: " << flux_last_update;

    ret = (flux_last_update.isValid()
           && flux_last_update.secsTo(QDateTime::currentDateTime()) < 20 * 60);

    qCDebug(runtime)<< "Result: " << ret;
    return ret;
}

bool PropConditions::isKIndexValid()
{
    FCT_IDENTIFICATION;
    bool ret = false;

    qCDebug(runtime)<<"Date valid: " << k_index_last_update.isValid() << " last_update: " << k_index_last_update;

    ret = (k_index_last_update.isValid()
           && k_index_last_update.secsTo(QDateTime::currentDateTime()) < 20 * 60);

    qCDebug(runtime)<< "Result: " << ret;

    return ret;
}

bool PropConditions::isAIndexValid()
{
    FCT_IDENTIFICATION;
    bool ret = false;

    qCDebug(runtime)<<"Date valid: " << a_index_last_update.isValid() << " last_update: " << a_index_last_update;

    ret = (a_index_last_update.isValid()
           && a_index_last_update.secsTo(QDateTime::currentDateTime()) < 20 * 60);

    qCDebug(runtime)<< "Result: " << ret;

    return ret;
}

bool PropConditions::isAuroraMapValid()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime)<<"Date valid: " << auroraMap_last_update.isValid()
                    << " last_update: " << auroraMap_last_update
                    << " aurora count: " << auroraMap.count();

    bool ret = (auroraMap_last_update.isValid()
           && auroraMap_last_update.secsTo(QDateTime::currentDateTime()) < 20 * 60
           && auroraMap.count() > 0);

    qCDebug(runtime)<< "Result: " << ret;

    return ret;
}

bool PropConditions::isMufMapValid()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime)<<"Date valid: " << mufMap_last_update.isValid()
                    << " last_update: " << mufMap_last_update
                    << " aurora count: " << mufMap.count();

    bool ret = (mufMap_last_update.isValid()
           && mufMap_last_update.secsTo(QDateTime::currentDateTime()) < 20 * 60
           && mufMap.count() > 0);

    qCDebug(runtime)<< "Result: " << ret;

    return ret;
}

int PropConditions::getFlux()
{
    FCT_IDENTIFICATION;
    qCDebug(runtime)<<"Current Flux: " << flux << " last_update: " << flux_last_update;
    return flux;

}

int PropConditions::getAIndex()
{
    FCT_IDENTIFICATION;
    qCDebug(runtime)<<"Current A-Index: " << a_index << " last_update: " << a_index_last_update;
    return a_index;
}

double PropConditions::getKIndex()
{
    FCT_IDENTIFICATION;
    qCDebug(runtime)<<"Current K-Index: " << k_index << " last_update: " << k_index_last_update;
    return k_index;
}

QList<GenericValueMap<double>::MapPoint> PropConditions::getAuroraPoints() const
{
    FCT_IDENTIFICATION;

    return auroraMap.getMap();
}

QList<GenericValueMap<double>::MapPoint> PropConditions::getMUFPoints() const
{
    return mufMap.getMap();
}

QString PropConditions::solarSummaryFile()
{
    FCT_IDENTIFICATION;

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    return dir.filePath("solar101vhf.gif");
}
