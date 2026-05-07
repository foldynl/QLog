#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QXmlStreamReader>
#include "Lotw.h"
#include "logformat/AdiFormat.h"
#include "core/debug.h"
#include "core/CredentialStore.h"
#include "core/LogParam.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.core.lotw");

QStringList LotwUploader::uploadedFields =
{
    "callsign",
    "freq",
    "band",
    "freq_rx",
    "mode",
    "submode",
    "start_time",
    "prop_mode",
    "sat_name",
    "station_callsign",
    "operator",
    "rst_sent",
    "rst_rcvd",
    "my_state",
    "my_cnty",
    "my_vucc_grids"
};

const QString LotwBase::SECURE_STORAGE_KEY = "LoTW";
REGISTRATION_SECURE_SERVICE(LotwBase);

const QString LotwBase::getUsername()
{
    FCT_IDENTIFICATION;

    return LogParam::getLoTWCallbookUsername();
}

const QString LotwBase::getPasswd()
{
    FCT_IDENTIFICATION;

    return getPassword(LotwBase::SECURE_STORAGE_KEY, getUsername());
}

void LotwBase::saveUsernamePassword(const QString &newUsername, const QString &newPassword)
{
    FCT_IDENTIFICATION;

    const QString &oldUsername = getUsername();
    if ( oldUsername != newUsername )
    {
        deletePassword(LotwBase::SECURE_STORAGE_KEY, oldUsername);
    }

    LogParam::setLoTWCallbookUsername(newUsername);
    savePassword(LotwBase::SECURE_STORAGE_KEY,
                 newUsername, newPassword);
}

const QString LotwBase::getTQSLPath(const QString &defaultPath)
{
    FCT_IDENTIFICATION;

#ifdef QLOG_FLATPAK
    // flatpak package contain an internal tqsl that is always on the same path
    Q_UNUSED(defaultPath);
    return QString("/app/bin/tqsl");
#else
    return LogParam::getLoTWTQSLPath(defaultPath);
#endif
}

void LotwBase::saveTQSLPath(const QString &newPath)
{
    FCT_IDENTIFICATION;

#ifdef QLOG_FLATPAK
    // do not save path for Flatpak version - an internal tqsl instance is present in the package
    Q_UNUSED(newPath);
#else
    LogParam::setLoTWTQSLPath(newPath);
#endif
}

QString LotwBase::findTQSLPath()
{
    FCT_IDENTIFICATION;

    // Platform-specific well-known paths
    const QStringList platformPaths =
    {
#ifdef Q_OS_WIN
        "C:/Program Files/ARRL/TQSL/tqsl.exe",
        "C:/Program Files (x86)/ARRL/TQSL/tqsl.exe",
        QDir::homePath() + "/AppData/Local/Programs/TQSL/tqsl.exe"
#elif defined(Q_OS_MACOS)
        "/Applications/tqsl.app/Contents/MacOS/tqsl",
        "/Applications/TQSL.app/Contents/MacOS/tqsl"
#else
        "/usr/bin/tqsl",
        "/usr/local/bin/tqsl",
        "/opt/tqsl/bin/tqsl"
#endif
    };

    for ( const QString &p : platformPaths )
    {
        if ( QFile::exists(p) )
        {
            qCDebug(runtime) << "Found TQSL at:" << p;
            return p;
        }
    }

    // Last resort: search in $PATH
    const QString path = QStandardPaths::findExecutable("tqsl");
    if ( !path.isEmpty() )
    {
        qCDebug(runtime) << "Found TQSL in PATH:" << path;
        return path;
    }

    qCWarning(runtime) << "TQSL not found";
    return QString();
}

TQSLVersion LotwBase::getTQSLVersion(const QString &tqslPath)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << tqslPath;

    TQSLVersion version;

    const QString path = tqslPath.trimmed().isEmpty() ? findTQSLPath() : tqslPath.trimmed();

    if ( path.isEmpty() )
    {
        qCDebug(runtime) << "TQSL not found";
        return version;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(path, QStringList("--version"));

    if ( !process.waitForFinished(2000) )
    {
        qCDebug(runtime) << "tqsl --version timed out";
        return version;
    }

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    qCDebug(runtime) << "tqsl version output:" << output;

    // "TQSL Version 2.7.2 [unknown]"
    QRegularExpression re("TQSL\\s+Version\\s+(\\d+)\\.(\\d+)\\.(\\d+)");
    QRegularExpressionMatch match = re.match(output); // clazy:exclude=use-static-qregularexpression

    if ( match.hasMatch() )
    {
        version.major = match.captured(1).toInt();
        version.minor = match.captured(2).toInt();
        version.patch = match.captured(3).toInt();
        qCDebug(runtime) << "Parsed TQSL version:" << version.major << version.minor << version.patch;
    }
    else
    {
        qCDebug(runtime) << "Failed to parse TQSL version from output:" << output;
    }

    return version;
}

void LotwBase::registerCredentials()
{
    // both storage keys belong to the same logical service
    CredentialRegistry::instance().add(SECURE_STORAGE_KEY, []()
    {
        return QList<CredentialDescriptor>
        {
            { SECURE_STORAGE_KEY, [](){ return getUsername(); } }
        };
    });
}

QString LotwBase::getTQSLStationDataPath()
{
    FCT_IDENTIFICATION;

    // QStandardPaths::GenericDataLocation is redirected by Flatpak to
    // ~/.var/app/<app-id>/data, where the bundled TQSL stores its data.
    // On a standard install TQSL uses the legacy ~/.tqsl/ directory.
    // On Windows, TQSL stores station_data in %APPDATA%\TrustedQSL\station_data.
    const QStringList candidates = {
#ifdef Q_OS_WIN
        QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/../TrustedQSL/station_data"),
        QDir::homePath() + "/AppData/Roaming/TrustedQSL/station_data"
#else
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + "/tqsl/station_data",
        QDir::homePath() + "/.tqsl/station_data"
#endif
    };

    for ( const QString &path : candidates )
    {
        if ( QFile::exists(path) )
        {
            qCDebug(runtime) << "Found TQSL station_data at:" << path;
            return path;
        }
    }

    qCDebug(runtime) << "TQSL station_data not found";
    return {};
}

QList<TQSLStationLocation> LotwBase::getTQSLStationLocations()
{
    FCT_IDENTIFICATION;

    const QString path = getTQSLStationDataPath();

    if ( path.isEmpty() )
        return {};

    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) )
    {
        qCDebug(runtime) << "Cannot open TQSL station_data:" << path;
        return {};
    }

    QList<TQSLStationLocation> locations;
    QXmlStreamReader xml(&file);
    TQSLStationLocation current;
    bool inStationData = false;

    while ( !xml.atEnd() && !xml.hasError() )
    {
        const QXmlStreamReader::TokenType tokenType = xml.readNext();

        if ( tokenType == QXmlStreamReader::StartElement )
        {
            if ( xml.name().compare(QLatin1String("StationData"), Qt::CaseInsensitive) == 0 )
            {
                current = TQSLStationLocation{};
                current.name = xml.attributes().value("name").toString();
                inStationData = true;
            }
            else if ( inStationData )
            {
                if ( xml.name().compare(QLatin1String("CALL"), Qt::CaseInsensitive) == 0 )
                    current.callsign = xml.readElementText();
                else if ( xml.name().compare(QLatin1String("GRIDSQUARE"), Qt::CaseInsensitive) == 0 )
                    current.grid = xml.readElementText();
            }
        }
        else if ( tokenType == QXmlStreamReader::EndElement
                  && xml.name().compare(QLatin1String("StationData"), Qt::CaseInsensitive) == 0 )
        {
            if ( !current.name.isEmpty() )
                locations << current;
            inStationData = false;
        }
    }

    qCDebug(runtime) << "TQSL locations count:" << locations.size();
    return locations;
}

LotwUploader::LotwUploader(QObject *parent) :
    GenericQSOUploader(uploadedFields, parent),
    LotwBase()
{
    FCT_IDENTIFICATION;
}

LotwUploader::~LotwUploader()
{
    FCT_IDENTIFICATION;
}

void LotwUploader::uploadAdif(const QByteArray &data, const QString &location)
{
    FCT_IDENTIFICATION;

    file.open();
    file.write(data);
    file.flush();

    QStringList args;
    args << "-d" << "-q" << "-u" << file.fileName();

    // Pass -l <location> only when the user explicitly selected a location
    if ( !location.trimmed().isEmpty() )
        args << "-l" << location.trimmed();

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

void LotwUploader::uploadQSOList(const QList<QSqlRecord> &qsos, const QVariantMap &addlParams)
{
    FCT_IDENTIFICATION;

    QByteArray data = generateADIF(qsos);
    const QString location = addlParams["tqsl_location"].toString();
    uploadAdif(data, location);
}

LotwQSLDownloader::LotwQSLDownloader(QObject *parent) :
    GenericQSLDownloader(parent),
    LotwBase(),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;
}

void LotwQSLDownloader::receiveQSL(const QDate &start_date, bool qso_since, const QString &station_callsign)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << start_date << " " << qso_since;

    QList<QPair<QString, QString>> params;

    if ( LotwDXCCCredits )
    {
        params.append(qMakePair(QString("ac_acct"), QString("1")));
    }
    else
    {
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
    }

    get(params);
}

void LotwQSLDownloader::abortDownload()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply = nullptr;
    }
}

void LotwQSLDownloader::processReply(QNetworkReply *reply)
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
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;
        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
           emit receiveQSLFailed(reply->errorString());
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
        emit receiveQSLFailed(tr("Cannot open temporary file"));
        return;
    }

    const QByteArray &data = reply->readAll();

    qCDebug(runtime) << data;

    /* verify the Username/password incorrect only in case when message is short (10k).
     * otherwise, it is a long ADIF and it is not necessary to verify login status */
    if ( size < 10000 && data.contains("Username/password incorrect") )
    {
        emit receiveQSLFailed(tr("Incorrect login or password"));
        return;
    }

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

    if ( LotwDXCCCredits )
        adi.runQSOCreditImport(adi.LOTW_DXCC);
    else
        adi.runQSLImport(adi.LOTW);

    tempFile.close();

    reply->deleteLater();
}

void LotwQSLDownloader::get(QList<QPair<QString, QString>> params)
{
    FCT_IDENTIFICATION;

    const QString &username = getUsername();
    const QString &password = getPasswd();

    QUrlQuery query;
    query.setQueryItems(params);
    query.addQueryItem("login", username.toUtf8().toPercentEncoding());
    query.addQueryItem("password", password.toUtf8().toPercentEncoding());

    QUrl url(LotwDXCCCredits ? DXCC_CREDIT_API : ADIF_API);
    url.setQuery(query);

    qCDebug(runtime) << Data::safeQueryString(query);

    if ( currentReply )
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet !!!";

    currentReply = getNetworkAccessManager()->get(QNetworkRequest(url));
}

LotwQSLDownloader::~LotwQSLDownloader()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        currentReply->abort();
        currentReply->deleteLater();
    }
}
