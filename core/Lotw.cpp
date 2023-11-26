#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QMessageBox>
#include "Lotw.h"
#include "logformat/AdiFormat.h"
#include "debug.h"
#include "core/CredentialStore.h"

#define ADIF_API "https://lotw.arrl.org/lotwuser/lotwreport.adi"

MODULE_IDENTIFICATION("qlog.core.lotw");

Lotw::Lotw(QObject *parent) :
    QObject(parent),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;

    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished,
            this, &Lotw::processReply);
}

Lotw::~Lotw()
{
    FCT_IDENTIFICATION;

    nam->deleteLater();

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

void Lotw::update(const QDate &start_date, bool qso_since, const QString &station_callsign)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << start_date << " " << qso_since;

    QList<QPair<QString, QString>> params;
    params.append(qMakePair(QString("qso_query"), QString("1")));
    params.append(qMakePair(QString("qso_qsldetail"), QString("yes")));
    params.append(qMakePair(QString("qso_owncall"), station_callsign));

    QString start = start_date.toString("yyyy-MM-dd");
    if (qso_since) {
        params.append(qMakePair(QString("qso_qsl"), QString("no")));
        params.append(qMakePair(QString("qso_qsorxsince"), start));
    }
    else {
        params.append(qMakePair(QString("qso_qsl"), QString("yes")));
        params.append(qMakePair(QString("qso_qslsince"), start));
    }

    get(params);
}

int Lotw::uploadAdif(const QByteArray &data, QString &ErrorString)
{
    FCT_IDENTIFICATION;

    QTemporaryFile file;
    file.open();
    file.write(data);
    file.flush();

    ErrorString = "";
    QStringList args;
    args << "-d" << "-q" << "-u" << file.fileName();
    int ErrorCode = QProcess::execute(getTQSLPath("tqsl"),args);

    /* list of Error Codes: http://www.arrl.org/command-1 */
    switch ( ErrorCode )
    {
    case -2: // Error code of QProcess::execute - Process cannot start
        ErrorString = tr("TQSL not found");
        break;

    case -1: // Error code of QProcess::execute - Process crashed
        ErrorString = tr("TQSL crashed");
        break;

    case 0: // Success
        break;

    case 1: // Cancelled by user
        ErrorString = tr("Upload cancelled by user");
        break;

    case 2: // Rejected by LoTW
        ErrorString = tr("Upload rejected by LoTW");
        break;

    case 3: // Unexpected response from TQSL server
        ErrorString = tr("Unexpected response from TQSL server");
        break;

    case 4: // TQSL error
        ErrorString = tr("TQSL utility error");
        break;

    case 5: // TQSLlib error
        ErrorString = tr("TQSLlib error");
        break;

    case 6: // Unable to open input file
        ErrorString = tr("Unable to open input file");
        break;

    case 7: // Unable to open output file
        ErrorString = tr("Unable to open output file");
        break;

    case 8: // All QSOs were duplicates or out of date range
        ErrorString = tr("All QSOs were duplicates or out of date range");
        break;

    case 9: // Some QSOs were duplicates or out of date range
        ErrorString = tr("Some QSOs were duplicates or out of date range");
        break;

    case 10: // Command syntax error
        ErrorString = tr("Command syntax error");
        break;

    case 11: // LoTW Connection error (no network or LoTW is unreachable)
        ErrorString = tr("LoTW Connection error (no network or LoTW is unreachable)");
        break;

    default:
        ErrorString = tr("Unexpected Error from TQSL");
    }

    return ErrorCode;
}

const QString Lotw::getUsername()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(Lotw::CONFIG_USERNAME_KEY).toString();

}

const QString Lotw::getPassword()
{
    FCT_IDENTIFICATION;

    return CredentialStore::instance()->getPassword(Lotw::SECURE_STORAGE_KEY,
                                                    getUsername());

}

const QString Lotw::getTQSLPath(const QString &defaultPath)
{
    FCT_IDENTIFICATION;

    QSettings settings;

#ifdef QLOG_FLATPAK_PACKAGE
    // flatpak package contain an internal tqsl that is always on the same path
    Q_UNUSED(defaultPath);
    return QString("/app/bin/tqsl");
#else
    return settings.value("lotw/tqsl", defaultPath).toString();
#endif
}

void Lotw::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QString oldUsername = getUsername();
    if ( oldUsername != newUsername )
    {
        CredentialStore::instance()->deletePassword(Lotw::SECURE_STORAGE_KEY,
                                                    oldUsername);
    }
    settings.setValue(Lotw::CONFIG_USERNAME_KEY, newUsername);
    CredentialStore::instance()->savePassword(Lotw::SECURE_STORAGE_KEY,
                                              newUsername,
                                              newPassword);

}

void Lotw::saveTQSLPath(const QString &newPath)
{
    FCT_IDENTIFICATION;

    QSettings settings;

#ifdef QLOG_FLATPAK_PACKAGE
    // do not save path for Flatpak - internal tqsl instance in the package
    Q_UNUSED(newPath);
#else
    settings.setValue("lotw/tqsl", newPath);
#endif
}

void Lotw::get(QList<QPair<QString, QString>> params)
{
    FCT_IDENTIFICATION;

    QString username = getUsername();
    QString password = getPassword();

    QUrlQuery query;
    query.setQueryItems(params);
    query.addQueryItem("login", username);
    query.addQueryItem("password", password);

    QUrl url(ADIF_API);
    url.setQuery(query);

    qCDebug(runtime) << url.toString();

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";
    }

    currentReply = nam->get(QNetworkRequest(url));
}

void Lotw::processReply(QNetworkReply* reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError
        || replyStatusCode < 200
        || replyStatusCode >= 300)
    {
        qCInfo(runtime) << "LotW error" << reply->errorString();
        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
           emit updateFailed(reply->errorString());
           reply->deleteLater();
        }
        return;
    }

    qint64 size = reply->size();
    qCDebug(runtime) << "Reply received, size: " << size;

    /* Currently, QT returns an incorrect stream position value in Network stream
     * when the stream is used in QTextStream. Therefore
     * QLog downloads a response, saves it to a temp file and opens
     * the file as a stream */
    QTemporaryFile tempFile;

    if ( ! tempFile.open() )
    {
        qCDebug(runtime) << "Cannot open temp file";
        emit updateFailed(tr("Cannot open temporary file"));
        return;
    }

    QByteArray data = reply->readAll();

    qCDebug(runtime) << data;

    /* verify the Username/password incorrect only in case when message is short (10k).
     * otherwise, it is a long ADIF and it is not necessary to verify login status */
    if ( size < 10000 && data.contains("Username/password incorrect") )
    {
        emit updateFailed(tr("Incorrect Loging or password"));
        return;
    }

    tempFile.write(data);
    tempFile.flush();
    tempFile.seek(0);

    emit updateStarted();

    /* see above why QLog uses a temp file */
    QTextStream stream(&tempFile);
    AdiFormat adi(stream);

    connect(&adi, &AdiFormat::importPosition, this, [this, size](qint64 position)
    {
        if ( size > 0 )
        {
            double progress = position * 100.0 / size;
            emit updateProgress(static_cast<int>(progress));
        }
    });

    connect(&adi, &AdiFormat::QSLMergeFinished, this, [this](QSLMergeStat stats)
    {
        emit updateComplete(stats);
    });

    adi.runQSLImport(adi.LOTW);

    tempFile.close();

    reply->deleteLater();
}

void Lotw::abortRequest()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        //currentReply->deleteLater(); // pointer is deleted later in processReply
        currentReply = nullptr;
    }
}

const QString Lotw::SECURE_STORAGE_KEY = "LoTW";
const QString Lotw::CONFIG_USERNAME_KEY = "lotw/username";
