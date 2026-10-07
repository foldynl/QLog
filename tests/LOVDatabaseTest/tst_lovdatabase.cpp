#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QNetworkReply>
#include <cstring>
#include "core/LOVDatabase.h"
#include "core/LogParam.h"
#include "core/FileCompressor.h"
#include "service/clublog/ClubLog.h"

const QString ClubLogBase::API_KEY = QStringLiteral("test");

class FixtureReply : public QNetworkReply
{
public:
    FixtureReply(const QByteArray &body, int status, NetworkError error, QObject *parent) :
        QNetworkReply(parent), body(body)
    {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        setHeader(QNetworkRequest::ContentTypeHeader, body.startsWith("<html>") ? "text/html" : "text/plain");
        if (error != NoError)
            setError(error, "Fixture network error");
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        setFinished(true);
    }
    void abort() override {}
protected:
    qint64 readData(char *destination, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, qint64(body.size()) - position);
        if (count <= 0)
            return -1;
        std::memcpy(destination, body.constData() + position, size_t(count));
        position += count;
        return count;
    }
private:
    const QByteArray body;
    qint64 position = 0;
};

class LOVDatabaseTest : public QObject
{
    Q_OBJECT
private slots:
    void responseHandling_data();
    void responseHandling();
    void init();
    void cleanup();
    void preparesCompleteSource_data();
    void preparesCompleteSource();
    void failedParseDoesNotPublish_data();
    void failedParseDoesNotPublish();
    void interruptedPreparationDoesNotPublish();
    void existingReadyIsNotOverwritten();
    void bundledCtyDoesNotAdvanceDate();
    void fullReplacementAddsChangesAndDeletes();
    void groupFailureRollsBackAllTables();
    void dateWriteFailureRollsBackTables();
    void commitFailureRestoresRowsAndCachedDate();
    void cachedBootstrapUsesWorkerWithoutChangingMain();
    void freshSourcesSkipDownloadsAndStopQueuedUpdates();
    void readyPackageSuppressesAnotherPreparation();
    void shutdownStopsCachedCtyWithoutFallback();
    void failedBundledCtyPreparationTerminates();
    void repeatedAdoptionIsSafe();
    void incompatiblePackageDoesNotModifyMain();
    void olderPackageDoesNotReplaceNewerImportedData();
    void emptyClublogExceptionsAreAccepted();
    void zeroAdifClublogExceptionsAreAccepted();
    void compressedSourceIsAccepted();
    void completeQuotedCSV_data();
    void completeQuotedCSV();
    void discardPreparedKeepsMainAndRawCaches();
    void discardPreparedReportsRemovalFailure();
private:
    void execute(const QString &sql);
    QVariant scalar(const QString &sql);
    QByteArray payload(LOVDownloader::SourceType type) const;
    LOVDatabase::Job job(LOVDownloader::SourceType type, const QByteArray &data = QByteArray()) const;
    void prepare(LOVDownloader::SourceType type, const QByteArray &data = QByteArray());
    LOVDatabase::ApplyResult apply(LOVDownloader::SourceType type);
    QString ready(LOVDownloader::SourceType type) const;
    QScopedPointer<QTemporaryDir> directory;
    QByteArray originalDataHome;
};

void LOVDatabaseTest::execute(const QString &sql)
{
    QSqlQuery query;
    QVERIFY2(query.exec(sql), qPrintable(query.lastError().text() + sql));
}

QVariant LOVDatabaseTest::scalar(const QString &sql)
{
    QSqlQuery query;
    if (!query.exec(sql) || !query.next())
        qFatal("%s", qPrintable(query.lastError().text() + sql));
    return query.value(0);
}

void LOVDatabaseTest::responseHandling_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("status");
    QTest::addColumn<int>("networkError");
    QTest::addColumn<bool>("canceled");
    QTest::addColumn<bool>("success");
    QTest::newRow("successful-json") << int(LOVDownloader::IOTALIST) << payload(LOVDownloader::IOTALIST) << 200 << int(QNetworkReply::NoError) << false << true;
    QTest::newRow("cty-html") << int(LOVDownloader::CTY) << QByteArray("<html>verification required</html>") << 200 << int(QNetworkReply::NoError) << false << true;
    QTest::newRow("cty-http-error") << int(LOVDownloader::CTY) << QByteArray("not found") << 404 << int(QNetworkReply::ContentNotFoundError) << false << true;
    QTest::newRow("cty-canceled") << int(LOVDownloader::CTY) << QByteArray() << 0 << int(QNetworkReply::OperationCanceledError) << true << true;
    QTest::newRow("non-cty-http-error") << int(LOVDownloader::POTADIRECTORY) << QByteArray("not found") << 404 << int(QNetworkReply::ContentNotFoundError) << false << false;
    QTest::newRow("non-cty-malformed-data") << int(LOVDownloader::POTADIRECTORY) << QByteArray("bad csv") << 200 << int(QNetworkReply::NoError) << false << false;
}

void LOVDatabaseTest::responseHandling()
{
#ifndef Q_OS_LINUX
    QSKIP("This integration case isolates QStandardPaths using XDG_DATA_HOME on Linux.");
#else
    QFETCH(int, type);
    QFETCH(QByteArray, body);
    QFETCH(int, status);
    QFETCH(int, networkError);
    QFETCH(bool, canceled);
    QFETCH(bool, success);
    const auto source = LOVDownloader::sourceDefinition(static_cast<LOVDownloader::SourceType>(type));
    const QDir cache(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QVERIFY(cache.absolutePath().startsWith(directory->path() + '/'));
    QVERIFY(cache.mkpath("."));
    LOVDownloader downloader;
    QSignalSpy finished(&downloader, &LOVDownloader::finished);
    if (canceled)
        downloader.abortRequest();
    QNetworkReply *reply = new FixtureReply(body, status, static_cast<QNetworkReply::NetworkError>(networkError), &downloader);
    reply->setProperty("sourceType", type);
    QVERIFY(QMetaObject::invokeMethod(&downloader, "processReply", Qt::DirectConnection, Q_ARG(QNetworkReply *, reply)));
    if (finished.isEmpty())
        QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), success);
    QCOMPARE(scalar("SELECT COUNT(*) FROM " + source.tableNames.first()).toInt(), 0);
    QVERIFY(!LogParam::getLOVaParam(source.lastTimeConfigName).isValid());
    QCOMPARE(QFile::exists(cache.filePath(source.packageStem + ".ready.db")), success);
    const bool downloaded = success && source.type != LOVDownloader::CTY;
    QCOMPARE(QFile::exists(cache.filePath(source.fileName)), downloaded);
    if (success)
    {
        QCOMPARE(LOVDatabase::applyReady(source, cache.path()), LOVDatabase::ApplyResult::APPLIED);
        QVERIFY(scalar("SELECT COUNT(*) FROM " + source.tableNames.first()).toInt() > 0);
        QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName).isValid(), downloaded);
    }
#endif
}

void LOVDatabaseTest::init()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    directory.reset(new QTemporaryDir);
    QVERIFY(directory->isValid());
    originalDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", directory->path().toUtf8());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(directory->filePath("qlog.db"));
    QVERIFY(db.open());
    execute("PRAGMA journal_mode=WAL");
    execute("CREATE TABLE log_param(name TEXT PRIMARY KEY, value TEXT)");
    execute("CREATE TABLE contacts(id INTEGER PRIMARY KEY, name TEXT)");
    execute("INSERT INTO contacts VALUES(1,'Keep me')");
    QString sql;
    for (const QString &resource : QStringList{"002", "013", "015", "017", "035"})
    {
        QFile file(":/sql/" + resource + ".sql");
        QVERIFY(file.open(QIODevice::ReadOnly));
        sql += QString::fromUtf8(file.readAll());
    }
    for (const auto type : LOVDownloader::sources())
    {
        const auto source = LOVDownloader::sourceDefinition(type);
        for (const QString &table : source.tableNames)
        {
            const QRegularExpression expression("CREATE TABLE(?: IF NOT EXISTS)?\\s+\"?" + table + "\"?\\s*\\([^;]+\\);", QRegularExpression::CaseInsensitiveOption);
            const auto match = expression.match(sql);
            QVERIFY2(match.hasMatch(), qPrintable(table));
            execute(match.captured());
        }
        QVERIFY(LogParam::setLOVParam(source.lastTimeConfigName, QDate()));
    }
}

void LOVDatabaseTest::cleanup()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    if ( originalDataHome.isNull() )
        qunsetenv("XDG_DATA_HOME");
    else
        qputenv("XDG_DATA_HOME", originalDataHome);
    directory.reset();
}

QByteArray LOVDatabaseTest::payload(LOVDownloader::SourceType type) const
{
    switch (type)
    {
    case LOVDownloader::CTY:
        return "OK,Czech Republic,503,EU,15,28,49.50,-15.50,-1.0,OK;=OK2XYZ;\n";
    case LOVDownloader::CLUBLOGCTY:
        return "<clublog><entities><entity><adif>503</adif><name>Fallback name</name><prefix>OK</prefix><deleted>false</deleted><cqz>15</cqz><cont>EU</cont><long>15.5</long><lat>49.5</lat></entity></entities><prefixes><prefix><call>OK</call><adif>503</adif><cqz>15</cqz></prefix></prefixes><exceptions><exception><call>OK2XYZ</call><adif>503</adif></exception></exceptions><zone_exceptions><zone_exception record='1'><call>OK2XYZ</call><zone>16</zone></zone_exception></zone_exceptions></clublog>";
    case LOVDownloader::SATLIST:
        return "AO-7;7530;145;435;;SSB;TEST;Active\n";
    case LOVDownloader::SOTASUMMITS:
        return "SOTA Summits List\nSummitCode,AssociationName,RegionName,SummitName,AltM,AltFt,GridRef1,GridRef2,Longitude,Latitude,Points,BonusPoints,ValidFrom,ValidTo\nOK/JC-001,OK,JC,Test,1000,3280,0,0,15.5,49.5,10,3,2000,2099\n";
    case LOVDownloader::WWFFDIRECTORY:
        return "reference,status,name,program,dxcc,state,county,continent,iota,iaruLocator,latitude,longitude,IUCNcat,validFrom,validTo\nOKFF-0001,Active,Test,OKFF,503,,,EU,,JN89,49.5,15.5,,2000,2099\n";
    case LOVDownloader::IOTALIST:
        return "[{\"refno\":\"EU-001\",\"name\":\"Test island\"}]";
    case LOVDownloader::POTADIRECTORY:
        return "reference,name,active,entityId,locationDesc,latitude,longitude,grid\nCZ-0001,Žďár,1,503,Test,49.5,15.5,JN89\nCZ-0002,New park,1,503,Test,49.5,15.5,JN89\n";
    case LOVDownloader::MEMBERSHIPCONTENTLIST:
        return "TEST,Test club,test.csv,2026-01-01,5\n";
    default:
        return {};
    }
}

LOVDatabase::Job LOVDatabaseTest::job(LOVDownloader::SourceType type, const QByteArray &data) const
{
    LOVDatabase::Job result;
    result.source = LOVDownloader::sourceDefinition(type);
    result.directory = directory->path();
    result.payload = data.isNull() ? payload(type) : data;
    result.downloadDate = QDate(2026, 1, 2);
    QSqlQuery query;
    for (const QString &table : result.source.tableNames)
    {
        if (!query.prepare("SELECT sql FROM sqlite_master WHERE tbl_name=? AND sql IS NOT NULL AND type IN('table','index') ORDER BY type DESC"))
            qFatal("prepare");
        query.bindValue(0, table);
        if (!query.exec()) qFatal("schema");
        while (query.next()) result.schemaStatements << query.value(0).toString();
    }
    return result;
}

QString LOVDatabaseTest::ready(LOVDownloader::SourceType type) const
{
    return directory->filePath(LOVDownloader::sourceDefinition(type).packageStem + ".ready.db");
}

void LOVDatabaseTest::prepare(LOVDownloader::SourceType type, const QByteArray &data)
{
    LOVDatabase worker(job(type, data));
    worker.start();
    QVERIFY(worker.wait(10000));
    QVERIFY2(worker.succeeded(), qPrintable(worker.errorString()));
}

LOVDatabase::ApplyResult LOVDatabaseTest::apply(LOVDownloader::SourceType type)
{
    QString error;
    const auto result = LOVDatabase::applyReady(LOVDownloader::sourceDefinition(type), directory->path(), &error);
    if (!error.isEmpty()) qInfo() << error;
    return result;
}

void LOVDatabaseTest::preparesCompleteSource_data()
{
    QTest::addColumn<int>("type");
    for (const auto type : LOVDownloader::sources())
        QTest::newRow(qPrintable(LOVDownloader::sourceDefinition(type).packageStem)) << int(type);
}

void LOVDatabaseTest::preparesCompleteSource()
{
    QFETCH(int, type);
    const auto sourceType = static_cast<LOVDownloader::SourceType>(type);
    prepare(sourceType);
    QVERIFY(QFile::exists(ready(sourceType)));
    const auto source = LOVDownloader::sourceDefinition(sourceType);
    for (const QString &table : source.tableNames)
        QCOMPARE(scalar("SELECT COUNT(*) FROM " + table).toInt(), 0);
    QVERIFY(!LogParam::getLOVaParam(source.lastTimeConfigName).isValid());
    QCOMPARE(scalar("SELECT name FROM contacts").toString(), QString("Keep me"));
    QVERIFY(!QFile::exists(ready(sourceType) + "-wal"));
    QVERIFY(!QFile::exists(ready(sourceType) + "-shm"));
    QCOMPARE(apply(sourceType), LOVDatabase::ApplyResult::APPLIED);
    for (const QString &table : source.nonEmptyTables)
        QVERIFY(scalar("SELECT COUNT(*) FROM " + table).toInt() > 0);
    QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName), QDate(2026, 1, 2));
    QVERIFY(!QFile::exists(ready(sourceType)));
}

void LOVDatabaseTest::failedParseDoesNotPublish_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QByteArray>("data");
    QTest::newRow("empty") << int(LOVDownloader::POTADIRECTORY) << QByteArray("");
    QTest::newRow("csv-header") << int(LOVDownloader::POTADIRECTORY) << QByteArray("<html>not a list</html>");
    QTest::newRow("csv-short-row") << int(LOVDownloader::POTADIRECTORY) << (payload(LOVDownloader::POTADIRECTORY) + "CZ-9999,bad\n");
    QTest::newRow("csv-unterminated-final-field") << int(LOVDownloader::POTADIRECTORY) << (payload(LOVDownloader::POTADIRECTORY) + "CZ-9999,Test,1,503,Test,49,15,\"UNTERMINATED\n");
    QTest::newRow("csv-unterminated-after-escaped-quote") << int(LOVDownloader::POTADIRECTORY) << (payload(LOVDownloader::POTADIRECTORY) + "CZ-9999,Test,1,503,Test,49,15,\"UNTERMINATED\"\"");
    QTest::newRow("csv-unterminated-trimmed-field") << int(LOVDownloader::WWFFDIRECTORY) << (payload(LOVDownloader::WWFFDIRECTORY) + "OKFF-9999,Active,Test,OKFF,503,,,EU,,JN89,49,15,,2000, \"UNTERMINATED\n");
    QTest::newRow("duplicate") << int(LOVDownloader::POTADIRECTORY) << (payload(LOVDownloader::POTADIRECTORY) + "CZ-0001,dup,1,503,Test,49,15,JN89\n");
    QTest::newRow("truncated-xml") << int(LOVDownloader::CLUBLOGCTY) << QByteArray("<clublog><entities><entity><adif>503</adif>");
    QTest::newRow("bad-json-row") << int(LOVDownloader::IOTALIST) << QByteArray("[{\"refno\":\"EU-001\",\"name\":\"Valid\"},{}]");
    QTest::newRow("invalid-cty-suffix") << int(LOVDownloader::CTY) << (payload(LOVDownloader::CTY).trimmed() + "BAD!OK;\n");
    QTest::newRow("invalid-cty-prefix") << int(LOVDownloader::CTY) << (payload(LOVDownloader::CTY).trimmed() + "BAD!;\n");
    QTest::newRow("invalid-clublog-adif") << int(LOVDownloader::CLUBLOGCTY) << QByteArray(payload(LOVDownloader::CLUBLOGCTY)).replace("<adif>503</adif>", "<adif>bad</adif>");
    QByteArray compressed = FileCompressor::gzip(payload(LOVDownloader::POTADIRECTORY));
    compressed.chop(8);
    QTest::newRow("truncated-gzip") << int(LOVDownloader::POTADIRECTORY) << compressed;
    QTest::newRow("bad-cty-row") << int(LOVDownloader::CTY) << (payload(LOVDownloader::CTY) + "bad\n");
    QTest::newRow("bad-sat-row") << int(LOVDownloader::SATLIST) << (payload(LOVDownloader::SATLIST) + "bad\n");
    QTest::newRow("bad-membership-row") << int(LOVDownloader::MEMBERSHIPCONTENTLIST) << (payload(LOVDownloader::MEMBERSHIPCONTENTLIST) + "bad\n");
}

void LOVDatabaseTest::failedParseDoesNotPublish()
{
    QFETCH(int, type);
    QFETCH(QByteArray, data);
    const auto sourceType = static_cast<LOVDownloader::SourceType>(type);
    LOVDatabase worker(job(sourceType, data));
    worker.start();
    QVERIFY(worker.wait(10000));
    QVERIFY(!worker.succeeded());
    QVERIFY(!QFile::exists(ready(sourceType)));
    QVERIFY(!LogParam::getLOVaParam(LOVDownloader::sourceDefinition(sourceType).lastTimeConfigName).isValid());
}

void LOVDatabaseTest::completeQuotedCSV_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QByteArray>("row");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("expected");
    QTest::newRow("quoted-final-field-at-eof") << int(LOVDownloader::POTADIRECTORY)
        << QByteArray("CZ-9999,Test,1,503,Test,49,15,\"JN89\"") << QString("grid") << QString("JN89");
    QTest::newRow("escaped-quotes-comma-and-newline") << int(LOVDownloader::POTADIRECTORY)
        << QByteArray("CZ-9999,\"Park \"\"North\"\",\nSouth\",1,503,Test,49,15,JN89\n") << QString("name") << QString("Park \"North\",\nSouth");
    QTest::newRow("literal-quote-in-unquoted-field") << int(LOVDownloader::POTADIRECTORY)
        << QByteArray("CZ-9999,Park\"North,1,503,Test,49,15,JN89\n") << QString("name") << QString("Park\"North");
    QTest::newRow("trimmed-quoted-final-field") << int(LOVDownloader::WWFFDIRECTORY)
        << QByteArray("OKFF-9999,Active,Test,OKFF,503,,,EU,,JN89,49,15,,2000, \"2099\"\n") << QString("valid_to") << QString("\"2099\"");
}

void LOVDatabaseTest::completeQuotedCSV()
{
    QFETCH(int, type);
    QFETCH(QByteArray, row);
    QFETCH(QString, field);
    QFETCH(QString, expected);
    const auto sourceType = static_cast<LOVDownloader::SourceType>(type);
    prepare(sourceType, payload(sourceType) + row);
    QCOMPARE(apply(sourceType), LOVDatabase::ApplyResult::APPLIED);
    const QString reference = sourceType == LOVDownloader::POTADIRECTORY ? "CZ-9999" : "OKFF-9999";
    QCOMPARE(scalar("SELECT " + field + " FROM " + LOVDownloader::sourceDefinition(sourceType).tableNames.first()
                    + " WHERE reference='" + reference + "'").toString(), expected);
}

void LOVDatabaseTest::discardPreparedKeepsMainAndRawCaches()
{
    for (const auto type : LOVDownloader::sources())
    {
        const auto source = LOVDownloader::sourceDefinition(type);
        for (const QString &state : {QString(".pending.db"), QString(".ready.db")})
        {
            for (const QString &suffix : {QString(), QString("-journal"), QString("-wal"), QString("-shm")})
            {
                QFile file(directory->filePath(source.packageStem + state + suffix));
                QVERIFY(file.open(QIODevice::WriteOnly));
                QCOMPARE(file.write("old incompatible package"), qint64(24));
            }
        }
        QFile cache(directory->filePath(source.fileName));
        QVERIFY(cache.open(QIODevice::WriteOnly));
        QCOMPARE(cache.write("raw cache"), qint64(9));
    }
    QVERIFY(LOVDatabase::discardPrepared(directory->path()));
    for (const auto type : LOVDownloader::sources())
    {
        const auto source = LOVDownloader::sourceDefinition(type);
        for (const QString &state : {QString(".pending.db"), QString(".ready.db")})
            for (const QString &suffix : {QString(), QString("-journal"), QString("-wal"), QString("-shm")})
                QVERIFY(!QFile::exists(directory->filePath(source.packageStem + state + suffix)));
        QFile cache(directory->filePath(source.fileName));
        QVERIFY(cache.open(QIODevice::ReadOnly));
        QCOMPARE(cache.readAll(), QByteArray("raw cache"));
        QCOMPARE(apply(type), LOVDatabase::ApplyResult::NO_PACKAGE);
    }
    QCOMPARE(scalar("SELECT name FROM contacts").toString(), QString("Keep me"));
    QVERIFY(LOVDatabase::discardPrepared(directory->path()));
}

void LOVDatabaseTest::discardPreparedReportsRemovalFailure()
{
    const QString package = ready(LOVDownloader::CTY);
    QVERIFY(QDir().mkpath(package));
    QFile blocker(package + "/keep");
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    QVERIFY(!LOVDatabase::discardPrepared(directory->path()));
    QVERIFY(QFile::exists(blocker.fileName()));
    QCOMPARE(scalar("SELECT name FROM contacts").toString(), QString("Keep me"));
}

void LOVDatabaseTest::interruptedPreparationDoesNotPublish()
{
    LOVDatabase worker(job(LOVDownloader::POTADIRECTORY));
    connect(&worker, &LOVDatabase::processingSize, &worker, [&worker](qint64) { worker.requestInterruption(); }, Qt::DirectConnection);
    worker.start();
    QVERIFY(worker.wait(10000));
    QVERIFY(!worker.succeeded());
    QVERIFY(!QFile::exists(ready(LOVDownloader::POTADIRECTORY)));
}

void LOVDatabaseTest::existingReadyIsNotOverwritten()
{
    prepare(LOVDownloader::POTADIRECTORY);
    QFile file(ready(LOVDownloader::POTADIRECTORY));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto before = file.readAll();
    file.close();
    LOVDatabase worker(job(LOVDownloader::POTADIRECTORY, QByteArray("bad")));
    worker.start();
    QVERIFY(worker.wait(10000));
    QVERIFY(!worker.succeeded());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), before);
}

void LOVDatabaseTest::fullReplacementAddsChangesAndDeletes()
{
    execute("INSERT INTO pota_directory(reference,name) VALUES('CZ-0001','Old name'),('CZ-9999','Deleted park')");
    prepare(LOVDownloader::POTADIRECTORY);
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::APPLIED);
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory").toInt(), 2);
    QCOMPARE(scalar("SELECT name FROM pota_directory WHERE reference='CZ-0001'").toString(), QString::fromUtf8("Žďár"));
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory WHERE reference='CZ-9999'").toInt(), 0);
    QCOMPARE(scalar("SELECT name FROM contacts").toString(), QString("Keep me"));
}

void LOVDatabaseTest::groupFailureRollsBackAllTables()
{
    execute("INSERT INTO dxcc_entities_ad1c(id,name) VALUES(1,'Old entity')");
    execute("INSERT INTO dxcc_prefixes_ad1c(prefix,exact,dxcc) VALUES('OLD',0,1)");
    execute("CREATE TRIGGER fail_prefix BEFORE INSERT ON dxcc_prefixes_ad1c BEGIN SELECT RAISE(ABORT,'test'); END");
    prepare(LOVDownloader::CTY);
    QCOMPARE(apply(LOVDownloader::CTY), LOVDatabase::ApplyResult::RETRY);
    QCOMPARE(scalar("SELECT name FROM dxcc_entities_ad1c").toString(), QString("Old entity"));
    QCOMPARE(scalar("SELECT prefix FROM dxcc_prefixes_ad1c").toString(), QString("OLD"));
    QVERIFY(QFile::exists(ready(LOVDownloader::CTY)));
}

void LOVDatabaseTest::dateWriteFailureRollsBackTables()
{
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::POTADIRECTORY);
    QVERIFY(LogParam::setLOVParam(source.lastTimeConfigName, QDate(2026, 1, 1)));
    QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName), QDate(2026, 1, 1));
    execute("INSERT INTO pota_directory(reference,name) VALUES('OLD','Keep')");
    execute("CREATE TRIGGER fail_date BEFORE INSERT ON log_param BEGIN SELECT RAISE(ABORT,'date'); END");
    prepare(LOVDownloader::POTADIRECTORY);
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::RETRY);
    QCOMPARE(scalar("SELECT name FROM pota_directory").toString(), QString("Keep"));
    QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName), QDate(2026, 1, 1));
    QVERIFY(QFile::exists(ready(LOVDownloader::POTADIRECTORY)));
}

void LOVDatabaseTest::commitFailureRestoresRowsAndCachedDate()
{
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::POTADIRECTORY);
    QVERIFY(LogParam::setLOVParam(source.lastTimeConfigName, QDate(2026, 1, 1)));
    QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName), QDate(2026, 1, 1));
    execute("PRAGMA foreign_keys=ON");
    execute("CREATE TABLE parent(id INTEGER PRIMARY KEY)");
    execute("CREATE TABLE child(parent_id INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED)");
    execute("CREATE TRIGGER fail_commit AFTER INSERT ON log_param BEGIN INSERT INTO child VALUES(999); END");
    execute("INSERT INTO pota_directory(reference,name) VALUES('OLD','Keep')");
    prepare(LOVDownloader::POTADIRECTORY);
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::RETRY);
    QCOMPARE(scalar("SELECT name FROM pota_directory").toString(), QString("Keep"));
    QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName), QDate(2026, 1, 1));
    QCOMPARE(scalar("SELECT COUNT(*) FROM child").toInt(), 0);
    QVERIFY(QFile::exists(ready(LOVDownloader::POTADIRECTORY)));
}

void LOVDatabaseTest::cachedBootstrapUsesWorkerWithoutChangingMain()
{
#ifndef Q_OS_LINUX
    QSKIP("This integration case isolates QStandardPaths using XDG_DATA_HOME on Linux.");
#else
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::POTADIRECTORY);
    const QDir cache(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QVERIFY(cache.absolutePath().startsWith(directory->path() + '/'));
    QVERIFY(cache.mkpath("."));
    QFile file(cache.filePath(source.fileName));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(payload(source.type)), qint64(payload(source.type).size()));
    file.close();
    for (const auto type : LOVDownloader::sources())
    {
        if (type == source.type)
            continue;
        prepare(type);
        QCOMPARE(apply(type), LOVDatabase::ApplyResult::APPLIED);
        QVERIFY(LogParam::setLOVParam(LOVDownloader::sourceDefinition(type).lastTimeConfigName, QDate::currentDate()));
    }
    const QDate date(2026, 1, 2);
    QVERIFY(LogParam::setLOVParam(source.lastTimeConfigName, date));
    LOVDownloader downloader;
    QSignalSpy finished(&downloader, &LOVDownloader::finished);
    downloader.update(source.type);
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().toBool());
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory").toInt(), 0);
    QCOMPARE(LogParam::getLOVaParam(source.lastTimeConfigName), date);
    QVERIFY(QFile::exists(cache.filePath(source.packageStem + ".ready.db")));
    QCOMPARE(QSqlDatabase::connectionNames().size(), 1);
    const QString package = cache.filePath(source.packageStem + ".ready.db");
    const QString saved = directory->filePath("prepared.db");
    QVERIFY(QFile::rename(package, saved));
    QSignalSpy skipped(&downloader, &LOVDownloader::noUpdate);
    downloader.startUpdates();
    QTRY_COMPARE(skipped.count(), LOVDownloader::sources().size() - 1);
    QCOMPARE(finished.count(), 1);
    QVERIFY(!QFile::exists(package));
    QVERIFY(QFile::rename(saved, package));
    QCOMPARE(LOVDatabase::applyReady(source, cache.path()), LOVDatabase::ApplyResult::APPLIED);
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory").toInt(), 2);
#endif
}

void LOVDatabaseTest::freshSourcesSkipDownloadsAndStopQueuedUpdates()
{
#ifndef Q_OS_LINUX
    QSKIP("This integration case isolates QStandardPaths using XDG_DATA_HOME on Linux.");
#else
    for (const auto type : LOVDownloader::sources())
    {
        prepare(type);
        QCOMPARE(apply(type), LOVDatabase::ApplyResult::APPLIED);
        QVERIFY(LogParam::setLOVParam(LOVDownloader::sourceDefinition(type).lastTimeConfigName, QDate::currentDate()));
    }
    LOVDownloader downloader;
    QSignalSpy skipped(&downloader, &LOVDownloader::noUpdate);
    QSignalSpy finished(&downloader, &LOVDownloader::finished);
    downloader.startUpdates();
    QTRY_COMPARE(skipped.count(), LOVDownloader::sources().size());
    QCOMPARE(finished.count(), 0);
    downloader.startUpdates();
    downloader.stopUpdates();
    QTest::qWait(1);
    QCOMPARE(skipped.count(), LOVDownloader::sources().size());
    QCOMPARE(QSqlDatabase::connectionNames().size(), 1);
#endif
}

void LOVDatabaseTest::readyPackageSuppressesAnotherPreparation()
{
#ifndef Q_OS_LINUX
    QSKIP("This integration case isolates QStandardPaths using XDG_DATA_HOME on Linux.");
#else
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::POTADIRECTORY);
    const QDir cache(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QVERIFY(cache.absolutePath().startsWith(directory->path() + '/'));
    QVERIFY(cache.mkpath("."));
    prepare(source.type);
    QVERIFY(QFile::copy(ready(source.type), cache.filePath(source.packageStem + ".ready.db")));
    LOVDownloader downloader;
    QSignalSpy skipped(&downloader, &LOVDownloader::noUpdate);
    QSignalSpy finished(&downloader, &LOVDownloader::finished);
    downloader.update(source.type, true);
    QCOMPARE(skipped.count(), 1);
    QCOMPARE(finished.count(), 0);
    QVERIFY(downloader.findChildren<LOVDatabase *>().isEmpty());
#endif
}

void LOVDatabaseTest::shutdownStopsCachedCtyWithoutFallback()
{
#ifndef Q_OS_LINUX
    QSKIP("This integration case isolates QStandardPaths using XDG_DATA_HOME on Linux.");
#else
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::CTY);
    const QDir cache(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QVERIFY(cache.absolutePath().startsWith(directory->path() + '/'));
    QVERIFY(cache.mkpath("."));
    QFile file(cache.filePath(source.fileName));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("invalid CTY cache\n") > 0);
    file.close();
    LOVDownloader downloader;
    QSignalSpy finished(&downloader, &LOVDownloader::finished);
    downloader.update(source.type);
    LOVDatabase *worker = downloader.findChild<LOVDatabase *>();
    QVERIFY(worker);
    downloader.stopUpdates();
    QVERIFY(worker->wait(10000));
    QTest::qWait(1);
    QCOMPARE(finished.count(), 0);
    QVERIFY(!QFile::exists(cache.filePath(source.packageStem + ".ready.db")));
    QCOMPARE(QSqlDatabase::connectionNames().size(), 1);
    QCOMPARE(scalar("SELECT COUNT(*) FROM dxcc_entities_ad1c").toInt(), 0);
    {
        LOVDownloader terminating;
        terminating.update(source.type);
    }
    QCOMPARE(QSqlDatabase::connectionNames().size(), 1);
#endif
}

void LOVDatabaseTest::failedBundledCtyPreparationTerminates()
{
#ifndef Q_OS_LINUX
    QSKIP("This integration case isolates QStandardPaths using XDG_DATA_HOME on Linux.");
#else
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::CTY);
    const QDir cache(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QVERIFY(cache.absolutePath().startsWith(directory->path() + '/'));
    QVERIFY(cache.mkpath("."));
    QFile file(cache.filePath(source.fileName));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("invalid CTY cache\n") > 0);
    file.close();
    execute("DROP TABLE dxcc_entities_ad1c");
    LOVDownloader downloader;
    QSignalSpy finished(&downloader, &LOVDownloader::finished);
    QSignalSpy networkFinished(downloader.findChild<QNetworkAccessManager *>(), &QNetworkAccessManager::finished);
    downloader.update(source.type);
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.count(), 1);
    QVERIFY(!finished.first().first().toBool());
    QCOMPARE(networkFinished.count(), 0);
    QVERIFY(!QFile::exists(cache.filePath(source.packageStem + ".ready.db")));
#endif
}

void LOVDatabaseTest::repeatedAdoptionIsSafe()
{
    prepare(LOVDownloader::POTADIRECTORY);
    const QString saved = directory->filePath("saved.db");
    QVERIFY(QFile::copy(ready(LOVDownloader::POTADIRECTORY), saved));
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::APPLIED);
    QVERIFY(QFile::copy(saved, ready(LOVDownloader::POTADIRECTORY)));
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::APPLIED);
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory").toInt(), 2);
    QCOMPARE(LogParam::getLOVaParam(LOVDownloader::sourceDefinition(LOVDownloader::POTADIRECTORY).lastTimeConfigName), QDate(2026, 1, 2));
}

void LOVDatabaseTest::incompatiblePackageDoesNotModifyMain()
{
    prepare(LOVDownloader::POTADIRECTORY);
    execute("ALTER TABLE pota_directory ADD extra TEXT");
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::INVALID);
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory").toInt(), 0);
}

void LOVDatabaseTest::olderPackageDoesNotReplaceNewerImportedData()
{
    prepare(LOVDownloader::POTADIRECTORY);
    const auto source = LOVDownloader::sourceDefinition(LOVDownloader::POTADIRECTORY);
    QVERIFY(LogParam::setLOVParam(source.lastTimeConfigName, QDate(2026, 1, 3)));
    execute("INSERT INTO pota_directory(reference,name) VALUES('NEWER','Keep')");
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::NO_PACKAGE);
    QCOMPARE(scalar("SELECT name FROM pota_directory").toString(), QString("Keep"));
}

void LOVDatabaseTest::bundledCtyDoesNotAdvanceDate()
{
    auto config = job(LOVDownloader::CTY);
    config.downloadDate = QDate();
    QFile file(":/res/data/cty.csv");
    QVERIFY(file.open(QIODevice::ReadOnly));
    config.payload = file.readAll();
    LOVDatabase worker(config);
    worker.start();
    QVERIFY(worker.wait(10000));
    QVERIFY2(worker.succeeded(), qPrintable(worker.errorString()));
    QCOMPARE(apply(LOVDownloader::CTY), LOVDatabase::ApplyResult::APPLIED);
    QVERIFY(!LogParam::getLOVaParam(config.source.lastTimeConfigName).isValid());
    QVERIFY(scalar("SELECT COUNT(*) FROM dxcc_entities_ad1c").toInt() > 300);
}

void LOVDatabaseTest::emptyClublogExceptionsAreAccepted()
{
    QByteArray data = payload(LOVDownloader::CLUBLOGCTY);
    const int begin = data.indexOf("<zone_exception record");
    const int end = data.indexOf("</zone_exception>") + QByteArray("</zone_exception>").size();
    data.remove(begin, end - begin);
    prepare(LOVDownloader::CLUBLOGCTY, data);
    QCOMPARE(apply(LOVDownloader::CLUBLOGCTY), LOVDatabase::ApplyResult::APPLIED);
    QCOMPARE(scalar("SELECT COUNT(*) FROM dxcc_zone_exceptions_clublog").toInt(), 0);
}

void LOVDatabaseTest::zeroAdifClublogExceptionsAreAccepted()
{
    QByteArray data = payload(LOVDownloader::CLUBLOGCTY);
    data.replace("<exception><call>OK2XYZ</call><adif>503</adif>", "<exception><call>OK2XYZ</call><adif>0</adif>");
    prepare(LOVDownloader::CLUBLOGCTY, data);
    QCOMPARE(apply(LOVDownloader::CLUBLOGCTY), LOVDatabase::ApplyResult::APPLIED);
    QCOMPARE(scalar("SELECT dxcc FROM dxcc_prefixes_clublog WHERE prefix='OK2XYZ'").toInt(), 0);
}

void LOVDatabaseTest::compressedSourceIsAccepted()
{
    prepare(LOVDownloader::POTADIRECTORY, FileCompressor::gzip(payload(LOVDownloader::POTADIRECTORY)));
    QCOMPARE(apply(LOVDownloader::POTADIRECTORY), LOVDatabase::ApplyResult::APPLIED);
    QCOMPARE(scalar("SELECT COUNT(*) FROM pota_directory").toInt(), 2);
}

QTEST_GUILESS_MAIN(LOVDatabaseTest)
#include "tst_lovdatabase.moc"
