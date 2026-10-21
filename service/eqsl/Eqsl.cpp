#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QHttpMultiPart>
#include <QSqlRecord>
#include <QDir>
#include <QTemporaryDir>
#include <QHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>

#include "Eqsl.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "logformat/AdiFormat.h"
#include "core/LogParam.h"
#include "data/Data.h"
#include "data/Callsign.h"

MODULE_IDENTIFICATION("qlog.service.eqsl.eqsl");

extern QTemporaryDir tempDir;

/* http://www.eqsl.cc/qslcard/ADIFContentSpecs.cfm */
QStringList EQSLUploader::uploadedFields =
{
    "start_time",
    "callsign",
    "mode",
    "freq",
    "band",
    "prop_mode",
    "rst_sent",
    "submode",
    "sat_mode",
    "sat_name",
    "my_cnty",
    "my_gridsquare",
    "qslmsg",
    "comment"
};

const QString EQSLBase::SECURE_STORAGE_KEY = "eQSL";
REGISTRATION_SECURE_SERVICE(EQSLBase);

const QString EQSLBase::getUsername()
{
    FCT_IDENTIFICATION;

    return LogParam::getEQSLLogbookUsername();
}

const QString EQSLBase::getPasswd()
{
    FCT_IDENTIFICATION;

    return getPassword(EQSLBase::SECURE_STORAGE_KEY, getUsername());
}


void EQSLBase::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    const QString &oldUsername = getUsername();

    if ( oldUsername != newUsername )
    {
        deletePassword(EQSLBase::SECURE_STORAGE_KEY, oldUsername);
    }
    LogParam::setEQSLLogbookUsername(newUsername);

    savePassword(EQSLBase::SECURE_STORAGE_KEY,
                 newUsername, newPassword);
}

QStringList EQSLBase::getAddlCallsigns()
{
    FCT_IDENTIFICATION;

    return LogParam::getEQSLCallsignsList();
}

void EQSLBase::setAddlCallsigns(const QStringList &list)
{
    FCT_IDENTIFICATION;

    LogParam::setEQSLCallsignsList(list);
}

QString EQSLBase::getCallsignPasswd(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    return getPassword(EQSLBase::SECURE_STORAGE_KEY, callsign);
}

void EQSLBase::saveCallsignPasswd(const QString &newPassword, const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    // the default account can have the same username - its password must stay
    if ( callsign.compare(getUsername(), Qt::CaseInsensitive) == 0 )
        return;

    deletePassword(EQSLBase::SECURE_STORAGE_KEY, callsign);

    if ( !newPassword.isEmpty() )
        savePassword(EQSLBase::SECURE_STORAGE_KEY, callsign, newPassword);
}

QString EQSLBase::resolveUsername(const QString &stationCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << stationCallsign;

    const QString normalizedCallsign = stationCallsign.trimmed().toUpper();

    if ( normalizedCallsign.isEmpty() )
        return getUsername();

    const QStringList &addlCallsigns = getAddlCallsigns();

    if ( addlCallsigns.contains(normalizedCallsign) )
        return normalizedCallsign;

    // portable and prefixed callsigns (SQ8BWA/P, SP/SQ8BWA) use the account of the base
    // callsign unless they have an account of their own
    const Callsign callsign(normalizedCallsign);

    if ( callsign.isValid() && addlCallsigns.contains(callsign.getBase()) )
        return callsign.getBase();

    return getUsername();
}

QString EQSLBase::getPasswdForUsername(const QString &username)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << username;

    return getPassword(EQSLBase::SECURE_STORAGE_KEY, username);
}

QStringList EQSLBase::getAccountUsernames()
{
    FCT_IDENTIFICATION;

    QStringList ret;

    if ( !getUsername().isEmpty() )
        ret << getUsername();

    const QStringList &addlCallsigns = getAddlCallsigns();

    for ( const QString &callsign : addlCallsigns )
        if ( !ret.contains(callsign, Qt::CaseInsensitive) )
            ret << callsign;

    return ret;
}

bool EQSLBase::isConfigured()
{
    FCT_IDENTIFICATION;

    return !getUsername().isEmpty() || !getAddlCallsigns().isEmpty();
}

void EQSLBase::registerCredentials()
{
    // both storage keys belong to the same logical service
    CredentialRegistry::instance().add(SECURE_STORAGE_KEY, []()
    {
        QList<CredentialDescriptor> ret;

        ret.append({ SECURE_STORAGE_KEY, [](){ return getUsername(); } });

        const QStringList &addlCallsigns = getAddlCallsigns();

        for ( QString callsign : addlCallsigns ) // do not use reference - the lambda keeps a copy
            ret.append({ SECURE_STORAGE_KEY, [callsign]() { return callsign; } });

        return ret;
    });
}

EQSLUploader::EQSLUploader( QObject *parent ):
   GenericQSOUploader(uploadedFields, parent),
   currentReply(nullptr),
   commentAsQSLMSG(false),
   disableqslmsg(true)
{
    FCT_IDENTIFICATION;
}

EQSLUploader::~EQSLUploader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void EQSLUploader::uploadAdif(const QByteArray &data, const QString &username)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << username;

    const QString &password = getPasswdForUsername(username);

    /* http://www.eqsl.cc/qslcard/ImportADIF.txt */
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);

    /* UserName */
    QHttpPart userPart;
    userPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"eqsl_user\""));
    userPart.setBody(username.toUtf8());

    /* Password */
    QHttpPart passPart;
    passPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"eqsl_pswd\""));
    passPart.setBody(password.toUtf8());

    /* File */
    QTemporaryFile file;
    file.open();
    file.write(data);
    file.flush();

    QHttpPart filePart;
    QString aux = QString("form-data; name=\"Filename\"; filename=\"%1\"").arg(file.fileName());
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(aux));
    filePart.setBody(data);

    multiPart->append(userPart);
    multiPart->append(passPart);
    multiPart->append(filePart);

    QUrl url(UPLOAD_ADIF_PAGE);
    QNetworkRequest request(url);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->post(request, multiPart);
    currentReply->setProperty("messageType", QVariant("uploadADIFFile"));
}

const QSqlRecord EQSLUploader::stripRecord(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    QSqlRecord newRecord(GenericQSOUploader::stripRecord(record));

    if ( disableqslmsg )
        newRecord.remove(newRecord.indexOf("qslmsg"));
    else if ( commentAsQSLMSG )
        newRecord.setValue("qslmsg", record.value("comment"));

    int commentIndex = newRecord.indexOf("comment");
    if (commentIndex != -1) newRecord.remove(commentIndex);

    return newRecord;
}

void EQSLUploader::uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &addlParams)
{
    FCT_IDENTIFICATION;

    QString qthProfile = addlParams["qthprofile"].toString();
    commentAsQSLMSG = addlParams["commentasqslmsg"].toBool();
    disableqslmsg = addlParams["disableqslmsg"].toBool();

    QMap<QString, QString> *applTags = nullptr;
    if ( !qthProfile.isEmpty() )
    {
        applTags = new QMap<QString, QString>;
        applTags->insert("app_eqsl_qth_nickname", qthProfile);
    }

    // eQSL takes the station callsign from the account, therefore QSOs are uploaded
    // separately for every account (callsign-specific accounts, the default account)
    QStringList usernames;
    QStringList callsignsWithoutAccount;
    QHash<QString, QList<QSqlRecord>> qsosPerAccount;
    QHash<QString, QString> usernamePerCallsign; // the callsign is parsed only once; empty = no account

    for ( const QSqlRecord &qso : qsos )
    {
        // based on LoTW source code and ADIF spec - priority STATION_CALLSIGN, OPERATOR
        QString stationCallsign = qso.value("station_callsign").toString();
        if ( stationCallsign.isEmpty() )
            stationCallsign = qso.value("operator").toString();

        auto usernameIt = usernamePerCallsign.constFind(stationCallsign);
        if ( usernameIt == usernamePerCallsign.constEnd() )
        {
            QString username = resolveUsername(stationCallsign);

            if ( username.compare(getUsername(), Qt::CaseInsensitive) == 0
                 && !isDefaultAccountCallsign(stationCallsign) )
            {
                callsignsWithoutAccount << stationCallsign.trimmed().toUpper();
                username.clear();
            }

            usernameIt = usernamePerCallsign.insert(stationCallsign, username);
        }
        const QString &username = usernameIt.value();

        if ( username.isEmpty() )
            continue;

        if ( !qsosPerAccount.contains(username) )
            usernames << username;

        qsosPerAccount[username].append(qso);
    }

    queuedUploads.clear();

    // eQSL would take these QSOs as QSOs of the default account callsign
    if ( !callsignsWithoutAccount.isEmpty() )
    {
        if ( applTags )
            delete applTags;

        const QString error = tr("No eQSL account is configured for %1").arg(callsignsWithoutAccount.join(", "));
        // report it asynchronously like any other upload error
        QTimer::singleShot(0, this, [this, error]() { emit uploadError(error); });
        return;
    }

    for ( const QString &username : static_cast<const QStringList&>(usernames) )
        queuedUploads.append({username, generateADIF(qsosPerAccount.value(username), applTags)});

    if ( applTags )
        delete applTags;

    uploadNextAccount();
}

bool EQSLUploader::isDefaultAccountCallsign(const QString &stationCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << stationCallsign;

    const QString &defaultUsername = getUsername();

    if ( defaultUsername.isEmpty() )
        return false;

    // QSOs without the station callsign keep using the default account
    if ( stationCallsign.trimmed().isEmpty()
         || stationCallsign.trimmed().compare(defaultUsername, Qt::CaseInsensitive) == 0 )
        return true;

    // portable and prefixed variants of the default account callsign
    const Callsign callsign(stationCallsign.trimmed());
    const Callsign defaultCallsign(defaultUsername);

    return callsign.isValid()
           && defaultCallsign.isValid()
           && callsign.getBase() == defaultCallsign.getBase();
}

void EQSLUploader::uploadNextAccount()
{
    FCT_IDENTIFICATION;

    if ( queuedUploads.isEmpty() )
    {
        emit uploadFinished();
        return;
    }

    const QPair<QString, QByteArray> next = queuedUploads.takeFirst();

    qCDebug(runtime) << "Uploading to eQSL account" << next.first;

    uploadAdif(next.second, next.first);
}

void EQSLUploader::processReply(QNetworkReply* reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "eQSL error URL " << reply->request().url().toString();
        qCDebug(runtime) << "eQSL error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        queuedUploads.clear();

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit uploadError(reply->errorString());
            reply->deleteLater();
        }
        return;
    }

    const QString &messageType = reply->property("messageType").toString();

    qCDebug(runtime) << "Received Message Type: " << messageType;

    /******************/
    /* uploadADIFFile */
    /******************/
    if ( messageType == "uploadADIFFile" )
    {
        const QString replyString(reply->readAll());
        qCDebug(runtime) << replyString;

        static QRegularExpression rOK("Result: (.*)");
        QRegularExpressionMatch matchOK = rOK.match(replyString);
        static QRegularExpression rError("Error: (.*)");
        QRegularExpressionMatch matchError = rError.match(replyString);
        static QRegularExpression rWarning("Warning: (.*)");
        QRegularExpressionMatch matchWarning = rWarning.match(replyString);
        static QRegularExpression rCaution("Caution: (.*)");
        QRegularExpressionMatch matchCaution = rCaution.match(replyString);
        QString msg;

        if ( matchOK.hasMatch() || matchWarning.hasMatch() )
        {
            // continue with the next account; uploadFinished is emitted after the last one
            uploadNextAccount();
        }
        else if (matchError.hasMatch() )
        {
            queuedUploads.clear();
            msg = matchError.captured(1);
            emit uploadError(msg);
        }
        else if (matchCaution.hasMatch() )
        {
            queuedUploads.clear();
            msg = matchCaution.captured(1);
            emit uploadError(msg);
        }
        else
        {
            queuedUploads.clear();
            qCInfo(runtime) << "Unknown Reply ";
            qCInfo(runtime) << replyString;
            emit uploadError(tr("Unknown Reply from eQSL"));
        }
    }

    reply->deleteLater();
}

void EQSLUploader::abortRequest()
{
    FCT_IDENTIFICATION;

    queuedUploads.clear();

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

EQSLQSLDownloader::EQSLQSLDownloader(QObject *parent) :
    GenericQSLDownloader(parent),
    EQSLBase(),
    currentReply(nullptr),
    qslStorage(new QSLStorage(this)),
    accountUsername(getUsername())
{
    FCT_IDENTIFICATION;
}

void EQSLQSLDownloader::setAccountUsername(const QString &username)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << username;

    accountUsername = username;
}

EQSLQSLDownloader::~EQSLQSLDownloader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
    qslStorage->deleteLater();
}

void EQSLQSLDownloader::receiveQSL(const QDate &start_date, bool qso_since, const QString &qthNick)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << start_date << qso_since << qthNick;

    QList<QPair<QString, QString>> params;

    if ( !qthNick.isEmpty() )
        params.append(qMakePair(QString("QTHNickname"), qthNick));

    if ( start_date.isValid() )
    {
        if ( qso_since )
        {
            const QString &start = start_date.toString("MM/dd/yyyy");
            const QString &stop = QDate::currentDate().addDays(1).toString("MM/dd/yyyy");
            params.append(qMakePair(QString("LimitDateLo"), start));
            params.append(qMakePair(QString("LimitDateHi"), stop));
        }
        else
        {
            //qsl_since
            const QString &start = start_date.toString("yyyyMMdd");
            params.append(qMakePair(QString("RcvdSince"), start));
        }
    }

    get(params);
}

void EQSLQSLDownloader::getQSLImage(const QSqlRecord &qso)
{
    FCT_IDENTIFICATION;

    QString inCacheFilename;

    if ( isQSLImageInCache(qso, inCacheFilename) )
    {
        emit QSLImageFound(inCacheFilename);
        return;
    }

    /* QSL image is not in Cache */
    qCDebug(runtime) << "QSL is not in Cache";

    // the card belongs to the account of the QSO station callsign
    QString stationCallsign = qso.value("station_callsign").toString();
    if ( stationCallsign.isEmpty() )
        stationCallsign = qso.value("operator").toString();

    const QString &username = resolveUsername(stationCallsign);
    const QString &password = getPasswdForUsername(username);
    const QDateTime &time_start = qso.value("start_time").toDateTime().toTimeZone(QTimeZone::utc());

    QUrlQuery query;

    query.addQueryItem("UserName", username.toUtf8().toPercentEncoding());
    query.addQueryItem("Password", password.toUtf8().toPercentEncoding());
    query.addQueryItem("CallsignFrom", qso.value("callsign").toString());
    query.addQueryItem("QSOYear", time_start.toString("yyyy"));
    query.addQueryItem("QSOMonth", time_start.toString("MM"));
    query.addQueryItem("QSODay", time_start.toString("dd"));
    query.addQueryItem("QSOHour", time_start.toString("hh"));
    query.addQueryItem("QSOMinute", time_start.toString("mm"));
    query.addQueryItem("QSOBand", qso.value("band").toString());
    query.addQueryItem("QSOMode", qso.value("mode").toString());

    QUrl url(QSL_IMAGE_FILENAME_PAGE);
    url.setQuery(query);

    qCDebug(runtime) << Data::safeQueryString(query);

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
    currentReply->setProperty("messageType", QVariant("getQSLImageFileName"));
    currentReply->setProperty("onDiskFilename", QVariant(inCacheFilename));
    currentReply->setProperty("QSORecordID", QVariant(qso.value("id")));
}

void EQSLQSLDownloader::abortDownload()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void EQSLQSLDownloader::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "eQSL error URL " << reply->request().url().toString();
        qCDebug(runtime) << "eQSL error" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            emit receiveQSLFailed(reply->errorString());
            emit QSLImageError(reply->errorString());
            reply->deleteLater();
        }
        return;
    }

    const QString &messageType = reply->property("messageType").toString();

    qCDebug(runtime) << "Received Message Type: " << messageType;

    /*******************/
    /* getADIFFileName */
    /*******************/
    if ( messageType == "getADIFFileName" )
    {
        //getting the first page where a ADIF filename is present
        QString replyString(reply->readAll());

        qCDebug(runtime) << replyString;

        if ( replyString.contains("No such Username/Password found")
             || replyString.contains("No such Callsign found") )
        {
            qCDebug(runtime) << "Incorrect Password or QTHProfile Id";
            emit receiveQSLFailed(tr("Incorrect Password or QTHProfile Id"));
        }
        else
        {
            static QRegularExpression re("/downloadedfiles/(.*.adi)\">.ADI file");
            QRegularExpressionMatch match = re.match(replyString);

            if ( match.hasMatch() )
            {
                const QString &filename = match.captured(1);
                downloadADIF(filename);
            }
            else if ( replyString.contains("You have no log entries"))
            {
                emit receiveQSLComplete(QSLMergeStat());
            }
            else
            {
                qCInfo(runtime) << "File not found in HTTP reply ";
                emit receiveQSLFailed(tr("ADIF file not found in eQSL response"));
            }
        }
    }
    /***********************/
    /* getQSLImageFileName */
    /***********************/
    else if ( messageType == "getQSLImageFileName" )
    {
        //getting the first page where an Image filename is present

        QString replyString(reply->readAll());

        qCDebug(runtime) << replyString;

        if ( replyString.contains("No such Username/Password found") )
            emit QSLImageError(tr("Incorrect Username or password"));
        else
        {
            static QRegularExpression re("<img src=\"(.*)\" alt");
            QRegularExpressionMatch match = re.match(replyString);

            if ( match.hasMatch() )
            {
                QString filename = match.captured(1);
                QString onDiskFilename = reply->property("onDiskFilename").toString();
                qulonglong qsoid = reply->property("QSORecordID").toULongLong();
                downloadImage(filename, onDiskFilename, qsoid);
            }
            else
            {
                static QRegularExpression rError("Error: (.*)");
                QRegularExpressionMatch matchError = rError.match(replyString);

                if (matchError.hasMatch() )
                {
                    QString msg = matchError.captured(1);
                    emit QSLImageError(msg);
                }
                else
                {
                    static QRegularExpression rWarning("Warning: (.*) --");
                    QRegularExpressionMatch matchWarning = rWarning.match(replyString);

                    if ( matchWarning.hasMatch() )
                    {
                        QString msg = matchWarning.captured(1);
                        emit QSLImageError(msg);
                    }
                    else
                    {
                        qCInfo(runtime) << replyString;
                        emit QSLImageError(tr("Unknown Error"));
                    }

                }
            }
        }
    }
    /***********/
    /* getADIF */
    /***********/
    else if ( messageType == "getADIF")
    {
        qint64 size = reply->size();
        qCDebug(runtime) << "Reply size: " << size;

        /* Currently, QT returns an incorrect stream position value in Network stream
         * when the stream is used in QTextStream. Therefore
         * QLog downloads a response, saves it to a temp file and opens
         * the file as a stream */
        QTemporaryFile tempFile;

        if ( ! tempFile.open() )
        {
            qCDebug(runtime) << "Cannot open temp file";
            emit receiveQSLFailed(tr("Cannot open temporary file"));
            reply->deleteLater();
            return;
        }

        const QByteArray &data = reply->readAll();

        tempFile.write(data);
        tempFile.flush();
        tempFile.seek(0);

        emit receiveQSLStarted();

        /* see above why QLog uses a temp file */
        QTextStream stream(&tempFile);
        AdiFormat adi(stream);

        connect(&adi, &AdiFormat::importPosition, this, [this, size](qint64 position)
        {
            if ( size > 0 )
            {
                double progress = position * 100.0 / size;
                emit receiveQSLProgress(static_cast<qulonglong>(progress));
            }
        });

        connect(&adi, &AdiFormat::QSLMergeFinished, this, [this](QSLMergeStat stats)
        {
            emit receiveQSLComplete(stats);
        });

        connect(&adi, &AdiFormat::QSLMergeFailed, this, [this](const QString &error)
        {
            emit receiveQSLFailed(error);
        });

        // eQSL confirmations belong to the callsign of the account - they must not match
        // QSOs of callsigns that are uploaded with another account
        adi.setQSLImportExcludedStationCallsigns(otherAccountsStationCallsigns());

        adi.runQSLImport(adi.EQSL);

        tempFile.close();
    }
    /********************/
    /* downloadQSLImage */
    /********************/
    else if ( messageType == "downloadQSLImage")
    {
        qint64 size = reply->size();
        qCDebug(runtime) << "Reply size: " << size;

        const QByteArray &data = reply->readAll();

        const QString &onDiskFilename = reply->property("onDiskFilename").toString();
        qulonglong qsoID = reply->property("QSORecordID").toULongLong();

        QFile file(onDiskFilename);
        if ( !file.open(QIODevice::WriteOnly))
        {
            emit QSLImageError(tr("Cannot save the image to file") + " " + onDiskFilename);

            return;
        }
        file.write(data);
        file.flush();
        file.close();
        if ( !qslStorage->add(QSLObject(qsoID, QSLObject::EQSL,
                                        QFileInfo(onDiskFilename).fileName(), data,
                                        QSLObject::RAWBYTES)) )
        {
            qWarning() << "Cannot save the eQSL image to database cache";
            // ??? database is only a cache for images. not needed to inform operator about this ????
        }
        emit QSLImageFound(onDiskFilename);
    }

    reply->deleteLater();
}

void EQSLQSLDownloader::get(const QList<QPair<QString, QString>> &params)
{
    FCT_IDENTIFICATION;

    const QString &username = accountUsername;
    const QString &password = getPasswdForUsername(username);

    QUrlQuery query;
    query.setQueryItems(params);
    query.addQueryItem("UserName", username.toUtf8().toPercentEncoding());
    query.addQueryItem("Password", password.toUtf8().toPercentEncoding());

    QUrl url(DOWNLOAD_1ST_PAGE);
    url.setQuery(query);

    qCDebug(runtime) << Data::safeQueryString(query);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
    currentReply->setProperty("messageType", QVariant("getADIFFileName"));
}

void EQSLQSLDownloader::downloadADIF(const QString &filename)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filename;

    QUrlQuery query;
    QUrl url(DOWNLOAD_2ND_PAGE + filename);
    url.setQuery(query);

    qCDebug(runtime) << url.toString();

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
    currentReply->setProperty("messageType", QVariant("getADIF"));
}

void EQSLQSLDownloader::downloadImage(const QString &URLFilename,
                         const QString &onDiskFilename,
                         const qulonglong qsoid)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << URLFilename << onDiskFilename << qsoid;

    QUrlQuery query;
    QUrl url(QSL_IMAGE_DOWNLOAD_PAGE + URLFilename);
    url.setQuery(query);

    qCDebug(runtime) << url.toString();

    currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
    currentReply->setProperty("messageType", QVariant("downloadQSLImage"));
    currentReply->setProperty("onDiskFilename", QVariant(onDiskFilename));
    currentReply->setProperty("QSORecordID", qsoid);
}

QString EQSLQSLDownloader::QSLImageFilename(const QSqlRecord &qso)
{
    FCT_IDENTIFICATION;

    /* QSL Fileformat YYYYMMDD_ID_Call_eqsl.jpg */

    const QDateTime &time_start = qso.value("start_time").toDateTime().toTimeZone(QTimeZone::utc());

    const QString &ret = QString("%1_%2_%3_eqsl.jpg").arg(time_start.toString("yyyyMMdd"),
                                                   qso.value("id").toString(),
                                                   qso.value("callsign").toString().replace(QRegularExpression(QString::fromUtf8("[-`~!@#$%^&*()_—+=|:;<>«»,.?/{}\'\"]")),"_"));
    qCDebug(runtime) << "EQSL Image Filename: " << ret;
    return ret;
}

QStringList EQSLQSLDownloader::otherAccountsStationCallsigns() const
{
    FCT_IDENTIFICATION;

    QStringList ret;
    QSqlQuery query;

    // the same expression as the station callsign filter of the QSL import
    if ( !query.exec("SELECT DISTINCT UPPER(COALESCE(NULLIF(TRIM(station_callsign), ''), TRIM(operator), '')) "
                     "FROM contacts") )
    {
        qCWarning(runtime) << "Cannot get station callsigns" << query.lastError();
        return ret;
    }

    while ( query.next() )
    {
        const QString callsign = query.value(0).toString();

        if ( resolveUsername(callsign).compare(accountUsername, Qt::CaseInsensitive) != 0 )
            ret << callsign;
    }

    qCDebug(runtime) << "Excluded station callsigns:" << ret;
    return ret;
}

bool EQSLQSLDownloader::isQSLImageInCache(const QSqlRecord &qso, QString &fullPath)
{
    FCT_IDENTIFICATION;

    bool isFileExists = false;

    const QString &expectingFilename = QSLImageFilename(qso);
    const QSLObject &eqsl = qslStorage->getQSL(qso, QSLObject::EQSL, expectingFilename);
    QFile f(tempDir.path() + QDir::separator() + eqsl.getQSLName());
    qCDebug(runtime) << "Using temp file" << f.fileName();
    fullPath = f.fileName();

    if ( eqsl.getBLOB() != QByteArray()
         && f.open(QFile::WriteOnly) )
    {
        f.write(eqsl.getBLOB());
        f.flush();
        f.close();
        isFileExists = true;
    }

    qCDebug(runtime) << isFileExists << " " << fullPath;

    return isFileExists;
}
