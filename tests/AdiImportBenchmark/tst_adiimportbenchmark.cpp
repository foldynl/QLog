#include <QtTest>
#include <QBuffer>
#include <QElapsedTimer>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTextStream>

#include "logformat/AdiFormat.h"

class TestAdiFormat : public AdiFormat
{
public:
    explicit TestAdiFormat(QTextStream &stream) :
        AdiFormat(stream)
    {
    }

    void importStartForTest()
    {
        importStart();
    }

    bool readContactForTest(QVariantMap &contact)
    {
        return readContact(contact);
    }
};

struct BenchmarkSample
{
    QString callsign;
    QString band;
    QString mode;
    QString submode;
    QString freq;
    QString dxcc;
    QString country;
    QString continent;
    QString cqz;
    QString ituz;
    QString grid;
    QString name;
    QString qth;
    QString comment;
    QString potaRef;
    QString myPotaRef;
    QString satName;
    QDateTime startTime;
};

struct StageTimings
{
    qint64 parseMs = 0;
    qint64 parseAndMapMs = 0;
    qint64 duplicateCurrentExistingIndexesMs = 0;
    qint64 insertMs = 0;
    qint64 logMs = 0;
    int parsed = 0;
    int mapped = 0;
    int duplicateHits = 0;
    int inserted = 0;
    int existingContactIndexCount = 0;
    qsizetype logBytes = 0;
};

class AdiImportBenchmark : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void benchmarkImportStages_data();
    void benchmarkImportStages();

private:
    static bool benchmarkEnabled();
    static QByteArray tag(const QByteArray &name,
                          const QByteArray &value,
                          const QByteArray &type = QByteArray());
    static QVector<BenchmarkSample> generateSamples(int count);
    static QByteArray generateAdi(const QVector<BenchmarkSample> &samples);
    static bool executeSqlFile(int version, QString *error);
    static bool resetDatabase(QString *error);
    static QStringList contactIndexNames(QString *error);
    static QSqlRecord contactRecordTemplate();
    static void setRecordValue(QSqlRecord &record, const QString &field, const QVariant &value);
    static void fillRecord(QSqlRecord &record, const BenchmarkSample &sample, int index);
    static qint64 measureParse(const QByteArray &adi, int *parsed);
    static qint64 measureParseAndMap(const QByteArray &adi, const QSqlRecord &recordTemplate, int *mapped);
    static bool populateDuplicateCandidates(const QVector<BenchmarkSample> &samples, QString *error);
    static qint64 measureDuplicateCheckCurrent(const QVector<BenchmarkSample> &samples,
                                               int *duplicateHits,
                                               QString *error);
    static qint64 measureInsert(const QVector<BenchmarkSample> &samples,
                                const QSqlRecord &recordTemplate,
                                int *inserted,
                                QString *error);
    static qint64 measureImportLog(const QVector<BenchmarkSample> &samples,
                                   const QSqlRecord &recordTemplate,
                                   qsizetype *logBytes);
};

void AdiImportBenchmark::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    Q_INIT_RESOURCE(res);
}

void AdiImportBenchmark::cleanup()
{
    const QString connectionName = QString::fromLatin1(QSqlDatabase::defaultConnection);
    if ( !QSqlDatabase::contains(connectionName) )
        return;

    {
        QSqlDatabase db = QSqlDatabase::database();
        if ( db.isValid() )
            db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void AdiImportBenchmark::benchmarkImportStages_data()
{
    QTest::addColumn<int>("recordCount");

    QTest::newRow("50k") << 50000;
}

void AdiImportBenchmark::benchmarkImportStages()
{
    if ( !benchmarkEnabled() )
        QSKIP("Set QLOG_RUN_ADI_IMPORT_BENCHMARK=1 to run the ADI import benchmark.");

    QFETCH(int, recordCount);

    const QVector<BenchmarkSample> samples = generateSamples(recordCount);
    const QByteArray adi = generateAdi(samples);

    QString error;
    QVERIFY2(resetDatabase(&error), qPrintable(error));
    const QSqlRecord recordTemplate = contactRecordTemplate();
    QVERIFY(recordTemplate.count() > 0);

    StageTimings timings;
    timings.parseMs = measureParse(adi, &timings.parsed);
    QCOMPARE(timings.parsed, recordCount);

    timings.parseAndMapMs = measureParseAndMap(adi, recordTemplate, &timings.mapped);
    QCOMPARE(timings.mapped, recordCount);

    QVERIFY2(resetDatabase(&error), qPrintable(error));
    QVERIFY2(populateDuplicateCandidates(samples, &error), qPrintable(error));
    const QStringList existingContactIndexes = contactIndexNames(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(existingContactIndexes.contains(QStringLiteral("contacts_callsign_idx")));
    QVERIFY(existingContactIndexes.contains(QStringLiteral("contacts_band_idx")));
    QVERIFY(existingContactIndexes.contains(QStringLiteral("contacts_mode_idx")));
    QVERIFY(existingContactIndexes.contains(QStringLiteral("contacts_start_time_idx")));
    timings.existingContactIndexCount = existingContactIndexes.size();

    timings.duplicateCurrentExistingIndexesMs =
        measureDuplicateCheckCurrent(samples, &timings.duplicateHits, &error);
    QVERIFY2(timings.duplicateCurrentExistingIndexesMs >= 0, qPrintable(error));

    QVERIFY2(resetDatabase(&error), qPrintable(error));
    const QSqlRecord insertRecordTemplate = contactRecordTemplate();
    timings.insertMs = measureInsert(samples, insertRecordTemplate, &timings.inserted, &error);
    QVERIFY2(timings.insertMs >= 0, qPrintable(error));
    QCOMPARE(timings.inserted, recordCount);

    timings.logMs = measureImportLog(samples, recordTemplate, &timings.logBytes);

    const qint64 mappingEstimate = qMax<qint64>(0, timings.parseAndMapMs - timings.parseMs);
    const QString report =
        QStringLiteral("ADI import benchmark: records=%1, adif_kB=%2, "
                       "contacts_existing_index_count=%3, parse_ms=%4, parse_map_ms=%5, "
                       "mapping_estimate_ms=%6, duplicate_current_existing_indexes_ms=%7, "
                       "duplicate_hits=%8, db_insert_ms=%9, log_ms=%10, log_kB=%11")
            .arg(recordCount)
            .arg(adi.size() / 1024)
            .arg(timings.existingContactIndexCount)
            .arg(timings.parseMs)
            .arg(timings.parseAndMapMs)
            .arg(mappingEstimate)
            .arg(timings.duplicateCurrentExistingIndexesMs)
            .arg(timings.duplicateHits)
            .arg(timings.insertMs)
            .arg(timings.logMs)
            .arg(timings.logBytes / 1024);

    qInfo().noquote() << report;
}

bool AdiImportBenchmark::benchmarkEnabled()
{
    return qEnvironmentVariableIntValue("QLOG_RUN_ADI_IMPORT_BENCHMARK") != 0;
}

QByteArray AdiImportBenchmark::tag(const QByteArray &name,
                                   const QByteArray &value,
                                   const QByteArray &type)
{
    QByteArray output("<");
    output += name;
    output += ':';
    output += QByteArray::number(value.size());
    if ( !type.isEmpty() )
    {
        output += ':';
        output += type;
    }
    output += '>';
    output += value;
    return output;
}

QVector<BenchmarkSample> AdiImportBenchmark::generateSamples(int count)
{
    static const struct {
        const char *call;
        const char *dxcc;
        const char *country;
        const char *continent;
        const char *cqz;
        const char *ituz;
        const char *grid;
        const char *name;
        const char *qth;
    } stations[] = {
        {"OK1AAA", "503", "Czech Republic", "EU", "15", "28", "JO70AA", "Pavel", "Praha"},
        {"DL1ABC", "230", "Germany", "EU", "14", "28", "JO62QN", "Hans", "Berlin"},
        {"K1ABC", "291", "United States", "NA", "5", "8", "FN42AA", "John", "Boston"},
        {"JA1XYZ", "339", "Japan", "AS", "25", "45", "PM95VP", "Ken", "Tokyo"},
        {"PY2ZZZ", "108", "Brazil", "SA", "11", "15", "GG66LK", "Carlos", "Sao Paulo"},
        {"VK3AAA", "150", "Australia", "OC", "30", "59", "QF22OD", "Chris", "Melbourne"}
    };

    static const struct {
        const char *band;
        const char *mode;
        const char *submode;
        const char *freq;
    } bandModes[] = {
        {"40m", "CW", "", "7.025"},
        {"20m", "MFSK", "FT8", "14.074"},
        {"15m", "SSB", "", "21.285"},
        {"10m", "RTTY", "", "28.090"},
        {"17m", "MFSK", "FT4", "18.104"}
    };

    QVector<BenchmarkSample> samples;
    samples.reserve(count);

    const QDateTime base(QDate(2024, 1, 1), QTime(0, 0, 0), QTimeZone::utc());

    for ( int i = 0; i < count; ++i )
    {
        const auto &station = stations[i % (sizeof(stations) / sizeof(stations[0]))];
        const auto &bandMode = bandModes[i % (sizeof(bandModes) / sizeof(bandModes[0]))];

        BenchmarkSample sample;
        sample.callsign = QString::fromLatin1(station.call) + QString::number(i % 90);
        sample.band = QString::fromLatin1(bandMode.band);
        sample.mode = QString::fromLatin1(bandMode.mode);
        sample.submode = QString::fromLatin1(bandMode.submode);
        sample.freq = QString::fromLatin1(bandMode.freq);
        sample.dxcc = QString::fromLatin1(station.dxcc);
        sample.country = QString::fromLatin1(station.country);
        sample.continent = QString::fromLatin1(station.continent);
        sample.cqz = QString::fromLatin1(station.cqz);
        sample.ituz = QString::fromLatin1(station.ituz);
        sample.grid = QString::fromLatin1(station.grid);
        sample.name = QString::fromLatin1(station.name);
        sample.qth = QString::fromLatin1(station.qth);
        sample.comment = QStringLiteral("Contest QSO %1, propagated via realistic ADI import benchmark").arg(i);
        sample.potaRef = QStringLiteral("OK-%1").arg(1000 + (i % 200), 4, 10, QLatin1Char('0'));
        sample.myPotaRef = QStringLiteral("OK-%1").arg(2000 + (i % 150), 4, 10, QLatin1Char('0'));
        sample.satName = (i % 97 == 0) ? QStringLiteral("QO-100") : QString();
        sample.startTime = base.addSecs(i * 73);
        samples.append(sample);
    }

    return samples;
}

QByteArray AdiImportBenchmark::generateAdi(const QVector<BenchmarkSample> &samples)
{
    QByteArray output("QLog ADI import benchmark\r\n");
    output += tag("ADIF_VER", "3.1.7");
    output += tag("PROGRAMID", "QLog");
    output += tag("USERDEF", "APP_QLOG_BENCHMARK");
    output += "<EOH>\r\n";
    output.reserve(output.size() + samples.size() * 720);

    for ( int i = 0; i < samples.size(); ++i )
    {
        const BenchmarkSample &sample = samples.at(i);
        const QDate date = sample.startTime.date();
        const QTime time = sample.startTime.time();

        output += tag("CALL", sample.callsign.toLatin1());
        output += tag("QSO_DATE", date.toString(QStringLiteral("yyyyMMdd")).toLatin1(), "D");
        output += tag("TIME_ON", time.toString(QStringLiteral("hhmmss")).toLatin1(), "T");
        output += tag("TIME_OFF", time.addSecs(180 + (i % 300)).toString(QStringLiteral("hhmmss")).toLatin1(), "T");
        output += tag("BAND", sample.band.toLatin1());
        output += tag("FREQ", sample.freq.toLatin1(), "N");
        output += tag("MODE", sample.mode.toLatin1());
        if ( !sample.submode.isEmpty() )
            output += tag("SUBMODE", sample.submode.toLatin1());
        output += tag("RST_SENT", (i % 3 == 0) ? "599" : "59");
        output += tag("RST_RCVD", (i % 4 == 0) ? "599" : "59");
        output += tag("DXCC", sample.dxcc.toLatin1(), "N");
        output += tag("COUNTRY", sample.country.toLatin1());
        output += tag("CONT", sample.continent.toLatin1());
        output += tag("CQZ", sample.cqz.toLatin1(), "N");
        output += tag("ITUZ", sample.ituz.toLatin1(), "N");
        output += tag("GRIDSQUARE", sample.grid.toLatin1());
        output += tag("NAME", sample.name.toLatin1());
        output += tag("QTH", sample.qth.toLatin1());
        output += tag("COMMENT", sample.comment.toLatin1());
        output += tag("STATION_CALLSIGN", "OK1QLOG");
        output += tag("MY_DXCC", "503", "N");
        output += tag("MY_GRIDSQUARE", "JO70AA");
        output += tag("OPERATOR", "OK1QLOG");
        output += tag("TX_PWR", QByteArray::number(50 + (i % 70)), "N");
        output += tag("ANT_AZ", QByteArray::number((i * 7) % 360), "N");
        output += tag("ANT_EL", QByteArray::number((i * 3) % 90), "N");
        output += tag("QSL_SENT", (i % 13 == 0) ? "Y" : "N");
        output += tag("QSL_RCVD", (i % 17 == 0) ? "Y" : "N");
        output += tag("LOTW_QSL_SENT", (i % 5 == 0) ? "Y" : "N");
        output += tag("LOTW_QSL_RCVD", (i % 7 == 0) ? "Y" : "N");
        output += tag("EQSL_QSL_SENT", (i % 11 == 0) ? "Y" : "N");
        output += tag("EQSL_QSL_RCVD", (i % 19 == 0) ? "Y" : "N");
        output += tag("POTA_REF", sample.potaRef.toLatin1());
        output += tag("MY_POTA_REF", sample.myPotaRef.toLatin1());
        if ( !sample.satName.isEmpty() )
            output += tag("SAT_NAME", sample.satName.toLatin1());
        output += tag("APP_QLOG_BENCHMARK", QByteArray::number(i), "S");
        output += "<EOR>\r\n";
    }

    return output;
}

bool AdiImportBenchmark::executeSqlFile(int version, QString *error)
{
    const QString resourceName = QStringLiteral(":/res/sql/migration_%1.sql")
                                     .arg(version, 3, 10, QLatin1Char('0'));
    QFile sqlFile(resourceName);
    if ( !sqlFile.open(QIODevice::ReadOnly | QIODevice::Text) )
    {
        *error = QStringLiteral("Cannot open %1").arg(resourceName);
        return false;
    }

    const QString sqlContent = QTextStream(&sqlFile).readAll();
    const QStringList statements = sqlContent.split(QLatin1Char('\n'))
                                       .join(QStringLiteral(" "))
                                       .split(QLatin1Char(';'));

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);

    if ( !db.transaction() )
    {
        *error = db.lastError().text();
        return false;
    }

    for ( const QString &statement : statements )
    {
        const QString trimmed = statement.trimmed();
        if ( trimmed.isEmpty() )
            continue;

        if ( !query.exec(trimmed) )
        {
            *error = QStringLiteral("Migration %1 failed: %2\n%3")
                         .arg(version, 3, 10, QLatin1Char('0'))
                         .arg(query.lastError().text(), trimmed);
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

bool AdiImportBenchmark::resetDatabase(QString *error)
{
    const QString connectionName = QString::fromLatin1(QSqlDatabase::defaultConnection);
    if ( QSqlDatabase::contains(connectionName) )
    {
        {
            QSqlDatabase db = QSqlDatabase::database();
            if ( db.isValid() )
                db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    db.setConnectOptions(QStringLiteral("QSQLITE_ENABLE_REGEXP"));
    if ( !db.open() )
    {
        *error = db.lastError().text();
        return false;
    }

    for ( int version = 1; version <= 39; ++version )
    {
        if ( !executeSqlFile(version, error) )
            return false;
    }

    return true;
}

QStringList AdiImportBenchmark::contactIndexNames(QString *error)
{
    QStringList names;
    QSqlQuery query(QStringLiteral("PRAGMA index_list('contacts')"));
    if ( !query.isActive() )
    {
        *error = query.lastError().text();
        return names;
    }

    while ( query.next() )
        names.append(query.value(QStringLiteral("name")).toString());

    return names;
}

QSqlRecord AdiImportBenchmark::contactRecordTemplate()
{
    QSqlTableModel model;
    model.setTable(QStringLiteral("contacts"));
    model.removeColumn(model.fieldIndex(QStringLiteral("id")));
    return model.record();
}

void AdiImportBenchmark::setRecordValue(QSqlRecord &record,
                                        const QString &field,
                                        const QVariant &value)
{
    const int index = record.indexOf(field);
    if ( index >= 0 )
        record.setValue(index, value);
}

void AdiImportBenchmark::fillRecord(QSqlRecord &record,
                                    const BenchmarkSample &sample,
                                    int index)
{
    record.clearValues();

    setRecordValue(record, QStringLiteral("start_time"), sample.startTime);
    setRecordValue(record, QStringLiteral("end_time"), sample.startTime.addSecs(180 + (index % 300)));
    setRecordValue(record, QStringLiteral("callsign"), sample.callsign.toUpper());
    setRecordValue(record, QStringLiteral("rst_sent"), (index % 3 == 0) ? QStringLiteral("599") : QStringLiteral("59"));
    setRecordValue(record, QStringLiteral("rst_rcvd"), (index % 4 == 0) ? QStringLiteral("599") : QStringLiteral("59"));
    setRecordValue(record, QStringLiteral("freq"), sample.freq);
    setRecordValue(record, QStringLiteral("band"), sample.band);
    setRecordValue(record, QStringLiteral("mode"), sample.mode);
    setRecordValue(record, QStringLiteral("submode"), sample.submode);
    setRecordValue(record, QStringLiteral("name"), sample.name);
    setRecordValue(record, QStringLiteral("qth"), sample.qth);
    setRecordValue(record, QStringLiteral("gridsquare"), sample.grid);
    setRecordValue(record, QStringLiteral("dxcc"), sample.dxcc.toInt());
    setRecordValue(record, QStringLiteral("country"), sample.country);
    setRecordValue(record, QStringLiteral("country_intl"), sample.country);
    setRecordValue(record, QStringLiteral("cont"), sample.continent);
    setRecordValue(record, QStringLiteral("cqz"), sample.cqz.toInt());
    setRecordValue(record, QStringLiteral("ituz"), sample.ituz.toInt());
    setRecordValue(record, QStringLiteral("pfx"), sample.callsign.left(3));
    setRecordValue(record, QStringLiteral("qsl_rcvd"), (index % 17 == 0) ? QStringLiteral("Y") : QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("qsl_sent"), (index % 13 == 0) ? QStringLiteral("Y") : QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("lotw_qsl_rcvd"), (index % 7 == 0) ? QStringLiteral("Y") : QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("lotw_qsl_sent"), (index % 5 == 0) ? QStringLiteral("Y") : QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("eqsl_qsl_rcvd"), (index % 19 == 0) ? QStringLiteral("Y") : QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("eqsl_qsl_sent"), (index % 11 == 0) ? QStringLiteral("Y") : QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("dcl_qsl_rcvd"), QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("dcl_qsl_sent"), QStringLiteral("N"));
    setRecordValue(record, QStringLiteral("tx_pwr"), 50 + (index % 70));
    setRecordValue(record, QStringLiteral("comment"), sample.comment);
    setRecordValue(record, QStringLiteral("station_callsign"), QStringLiteral("OK1QLOG"));
    setRecordValue(record, QStringLiteral("my_dxcc"), 503);
    setRecordValue(record, QStringLiteral("my_gridsquare"), QStringLiteral("JO70AA"));
    setRecordValue(record, QStringLiteral("operator"), QStringLiteral("OK1QLOG"));
    setRecordValue(record, QStringLiteral("ant_az"), (index * 7) % 360);
    setRecordValue(record, QStringLiteral("ant_el"), (index * 3) % 90);
    setRecordValue(record, QStringLiteral("pota_ref"), sample.potaRef);
    setRecordValue(record, QStringLiteral("my_pota_ref"), sample.myPotaRef);
    setRecordValue(record, QStringLiteral("sat_name"), sample.satName);
    setRecordValue(record, QStringLiteral("fields"),
                   QStringLiteral("{\"app_qlog_benchmark\":\"%1\"}").arg(index));
}

qint64 AdiImportBenchmark::measureParse(const QByteArray &adi, int *parsed)
{
    QByteArray input(adi);
    QBuffer buffer(&input);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    QTextStream stream(&buffer);
    TestAdiFormat format(stream);
    format.importStartForTest();

    QVariantMap contact;
    QElapsedTimer timer;
    timer.start();

    int count = 0;
    while ( format.readContactForTest(contact) )
    {
        ++count;
        contact.clear();
    }

    *parsed = count;
    return timer.elapsed();
}

qint64 AdiImportBenchmark::measureParseAndMap(const QByteArray &adi,
                                              const QSqlRecord &recordTemplate,
                                              int *mapped)
{
    QByteArray input(adi);
    QBuffer buffer(&input);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    QTextStream stream(&buffer);
    TestAdiFormat format(stream);
    format.importStartForTest();

    QSqlRecord record(recordTemplate);
    QElapsedTimer timer;
    timer.start();

    int count = 0;
    while ( true )
    {
        record.clearValues();
        if ( !format.importNext(record) )
            break;
        ++count;
    }

    *mapped = count;
    return timer.elapsed();
}

bool AdiImportBenchmark::populateDuplicateCandidates(const QVector<BenchmarkSample> &samples,
                                                     QString *error)
{
    QSqlQuery insert;
    if ( !insert.prepare(QStringLiteral("INSERT INTO contacts "
                                        "(callsign, mode, band, sat_name, start_time, "
                                        "qsl_rcvd, qsl_sent, lotw_qsl_rcvd, lotw_qsl_sent, "
                                        "eqsl_qsl_rcvd, eqsl_qsl_sent, dcl_qsl_rcvd, dcl_qsl_sent) "
                                        "VALUES (?, ?, ?, ?, ?, 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N')")) )
    {
        *error = insert.lastError().text();
        return false;
    }

    QSqlDatabase::database().transaction();

    for ( int i = 0; i < samples.size(); i += 10 )
    {
        const BenchmarkSample &sample = samples.at(i);
        insert.bindValue(0, sample.callsign.toUpper());
        insert.bindValue(1, sample.mode);
        insert.bindValue(2, sample.band);
        insert.bindValue(3, sample.satName);
        insert.bindValue(4, sample.startTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
        if ( !insert.exec() )
        {
            *error = insert.lastError().text();
            QSqlDatabase::database().rollback();
            return false;
        }
    }

    return QSqlDatabase::database().commit();
}

qint64 AdiImportBenchmark::measureDuplicateCheckCurrent(const QVector<BenchmarkSample> &samples,
                                                        int *duplicateHits,
                                                        QString *error)
{
    QSqlQuery dupQuery;
    if ( !dupQuery.prepare(QStringLiteral("SELECT * FROM contacts "
                                          "WHERE callsign=upper(:callsign) "
                                          "AND upper(mode)=upper(:mode) "
                                          "AND upper(band)=upper(:band) "
                                          "AND COALESCE(sat_name, '') = COALESCE(:sat_name, '') "
                                          "AND ABS(JULIANDAY(start_time)-JULIANDAY(datetime(:startdate)))*24*60<30")) )
    {
        *error = dupQuery.lastError().text();
        return -1;
    }

    int hits = 0;
    QElapsedTimer timer;
    timer.start();

    for ( const BenchmarkSample &sample : samples )
    {
        dupQuery.bindValue(QStringLiteral(":callsign"), sample.callsign);
        dupQuery.bindValue(QStringLiteral(":mode"), sample.mode);
        dupQuery.bindValue(QStringLiteral(":band"), sample.band);
        dupQuery.bindValue(QStringLiteral(":startdate"),
                           sample.startTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
        dupQuery.bindValue(QStringLiteral(":sat_name"), sample.satName);

        if ( !dupQuery.exec() )
        {
            *error = dupQuery.lastError().text();
            return -1;
        }

        if ( dupQuery.next() )
            ++hits;
    }

    *duplicateHits = hits;
    return timer.elapsed();
}

qint64 AdiImportBenchmark::measureInsert(const QVector<BenchmarkSample> &samples,
                                         const QSqlRecord &recordTemplate,
                                         int *inserted,
                                         QString *error)
{
    QSqlRecord record(recordTemplate);
    QSqlQuery insertQuery;
    if ( !insertQuery.prepare(QSqlDatabase::database().driver()->sqlStatement(QSqlDriver::InsertStatement,
                                                                              QStringLiteral("contacts"),
                                                                              record,
                                                                              true)) )
    {
        *error = insertQuery.lastError().text();
        return -1;
    }

    QSqlDatabase::database().transaction();

    int count = 0;
    QElapsedTimer timer;
    timer.start();

    for ( int i = 0; i < samples.size(); ++i )
    {
        fillRecord(record, samples.at(i), i);

        for ( int field = 0; field < record.count(); ++field )
            insertQuery.bindValue(field, record.value(field));

        if ( !insertQuery.exec() )
        {
            *error = insertQuery.lastError().text();
            QSqlDatabase::database().rollback();
            return -1;
        }
        ++count;
    }

    const qint64 elapsed = timer.elapsed();
    QSqlDatabase::database().commit();

    *inserted = count;
    return elapsed;
}

qint64 AdiImportBenchmark::measureImportLog(const QVector<BenchmarkSample> &samples,
                                            const QSqlRecord &recordTemplate,
                                            qsizetype *logBytes)
{
    QSqlRecord record(recordTemplate);
    QString log;
    log.reserve(samples.size() * 80);
    QTextStream stream(&log);

    QElapsedTimer timer;
    timer.start();

    for ( int i = 0; i < samples.size(); ++i )
    {
        fillRecord(record, samples.at(i), i);
        stream << QStringLiteral("[QSO#%1]: ").arg(i + 1)
               << QStringLiteral("INFO: ")
               << QStringLiteral("Imported")
               << QStringLiteral(" (%1; %2; %3)")
                      .arg(record.value(QStringLiteral("start_time")).toDateTime()
                               .toTimeZone(QTimeZone::utc())
                               .toString(QStringLiteral("yyyy-MM-dd")),
                           record.value(QStringLiteral("callsign")).toString(),
                           record.value(QStringLiteral("mode")).toString())
               << QLatin1Char('\n');
    }

    stream.flush();
    const qint64 elapsed = timer.elapsed();
    *logBytes = log.size() * qsizetype(sizeof(QChar));
    return elapsed;
}

QTEST_APPLESS_MAIN(AdiImportBenchmark)

#include "tst_adiimportbenchmark.moc"
