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

    const QString &start = start_date.toString("yyyy-MM-dd");

    if (qso_since)
    {
        params.append(qMakePair(QString("qso_qsl"), QString("no")));
        params.append(qMakePair(QString("qso_qsorxsince"), start));
    }
    else
    {
        params.append(qMakePair(QString("qso_qsl"), QString("yes")));
        params.append(qMakePair(QString("qso_qslsince"), start));
    }

    get(params);
}

void Lotw::uploadAdif(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    file.open();
    file.write(data);
    file.flush();

    QStringList args;
    args << "-d" << "-q" << "-u" << file.fileName();

    QProcess *tqslProcess = new QProcess();

    connect(tqslProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, tqslProcess](int exitCode, QProcess::ExitStatus exitStatus)
    {
        qCDebug(runtime) << "Process finished with exit code" << exitCode << "and exit status" << exitStatus;

        /* list of Error Codes: http://www.arrl.org/command-1 */
        switch ( exitCode )
        {
        case 0: // Success
            emit uploadFinished();
            break;

        case 1: // Cancelled by user
            emit uploadError(tr("Upload cancelled by user"));
            break;

        case 2: // Rejected by LoTW
            emit uploadError(tr("Upload rejected by LoTW"));
            break;

        case 3: // Unexpected response from TQSL server
            emit uploadError(tr("Unexpected response from TQSL server"));
            break;

        case 4: // TQSL error
            emit uploadError(tr("TQSL utility error"));
            break;

        case 5: // TQSLlib error
            emit uploadError(tr("TQSLlib error"));
            break;

        case 6: // Unable to open input file
            emit uploadError(tr("Unable to open input file"));
            break;

        case 7: // Unable to open output file
            emit uploadError(tr("Unable to open output file"));
            break;

        case 8: // All QSOs were duplicates or out of date range
            emit uploadError(tr("All QSOs were duplicates or out of date range"));
            break;

        case 9: // Some QSOs were duplicates or out of date range
            emit uploadError(tr("Some QSOs were duplicates or out of date range"));
            break;

        case 10: // Command syntax error
            emit uploadError(tr("Command syntax error"));
            break;

        case 11: // LoTW Connection error (no network or LoTW is unreachable)
            emit uploadError(tr("LoTW Connection error (no network or LoTW is unreachable)"));
            break;

        default:
            emit uploadError(tr("Unexpected Error from TQSL"));
        }

        tqslProcess->deleteLater();
    });

    connect(tqslProcess, &QProcess::errorOccurred, this, [this, tqslProcess](QProcess::ProcessError error)
    {
        qDebug() << "Process error:" << error;

        switch ( error )
        {
        case QProcess::FailedToStart: // Error code of QProcess::execute - Process cannot start
            emit uploadError(tr("TQSL not found"));
            break;

        case QProcess::Crashed: // Error code of QProcess::execute - Process crashed
            emit uploadError(tr("TQSL crashed"));
            break;
        default:
            emit uploadError(tr("Unexpected Error from TQSL"));
        }
        tqslProcess->deleteLater();
    });

    connect(tqslProcess, &QProcess::readyReadStandardOutput, this, [tqslProcess]()
    {
        qCDebug(runtime)<< "TQSL output: " << qPrintable(tqslProcess->readAllStandardOutput());
    });

    tqslProcess->setProcessChannelMode(QProcess::MergedChannels);
    tqslProcess->setReadChannel(QProcess::StandardOutput);

    qCDebug(runtime) << getTQSLPath("tqsl") << args;
    tqslProcess->start(getTQSLPath("tqsl"),args);
}

const QString Lotw::getUsername()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    return settings.value(Lotw::CONFIG_USERNAME_KEY).toString().trimmed();

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

#ifdef QLOG_FLATPAK
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

    const QString &oldUsername = getUsername();
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

#ifdef QLOG_FLATPAK
    // do not save path for Flatpak version - an internal tqsl instance is present in the package
    Q_UNUSED(newPath);
#else
    settings.setValue("lotw/tqsl", newPath);
#endif
}

void Lotw::get(QList<QPair<QString, QString>> params)
{
    FCT_IDENTIFICATION;

    const QString &username = getUsername();
    const QString &password = getPassword();

    QUrlQuery query;
    query.setQueryItems(params);
    query.addQueryItem("login", username.toUtf8().toPercentEncoding());
    query.addQueryItem("password", password.toUtf8().toPercentEncoding());

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

    const QByteArray &data = reply->readAll();

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
