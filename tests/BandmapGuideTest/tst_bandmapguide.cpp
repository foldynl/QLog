#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/LogParam.h"
#include "data/BandmapGuide.h"

namespace {
QString lastErrorString(const QSqlQuery &query)
{
    return query.lastError().isValid() ? query.lastError().text() : QString();
}

void compareRange(const BandmapGuide::Range &actual,
                  double from,
                  double to,
                  const QColor &color,
                  const QString &label)
{
    QCOMPARE(actual.from, from);
    QCOMPARE(actual.to, to);
    QCOMPARE(actual.color, color);
    QCOMPARE(actual.label, label);
}
}

class BandmapGuideTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void cleanupTestCase();

    void profilesUseExampleGuideOnFirstRun();
    void saveProfilesRoundTripFiltersInvalidData();
    void currentProfileFallsBackToFirstProfile();
    void profileExistsRejectsEmptyAndMissingId();
    void setCurrentProfileIdEmitsChangedOnlyOnRealChange();
    void setEnabledEmitsChangedOnlyOnRealChange();
    void writeReadProfileRoundTripAssignsNewId();
    void readProfileAcceptsProfilesWrapper();
    void readProfileUsesFilenameWhenTitleMissing();
    void readProfileSupportsLegacyNameField();
    void readProfileRejectsInvalidJsonAndMissingFile();

private:
    QString writeTextFile(const QString &fileName, const QByteArray &content);

    QTemporaryDir tempDir;
};

void BandmapGuideTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    QVERIFY(tempDir.isValid());

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());

    QSqlQuery query;
    QVERIFY2(query.exec("CREATE TABLE log_param (name TEXT PRIMARY KEY, value TEXT)"),
             qPrintable(lastErrorString(query)));
}

void BandmapGuideTest::cleanup()
{
    LogParam::setBandmapGuideProfiles(QString());
    LogParam::setBandmapGuideCurrentProfile(QString());
    LogParam::setBandmapGuideEnabled(false);
}

void BandmapGuideTest::cleanupTestCase()
{
    const QString connectionName = QString::fromLatin1(QSqlDatabase::defaultConnection);
    {
        QSqlDatabase db = QSqlDatabase::database();
        if ( db.isValid() )
            db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QString BandmapGuideTest::writeTextFile(const QString &fileName, const QByteArray &content)
{
    const QString path = tempDir.filePath(fileName);
    QFile file(path);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        qFatal("Cannot open temporary guide file for writing");
    if ( file.write(content) != static_cast<qint64>(content.size()) )
        qFatal("Cannot write complete temporary guide file");
    file.close();
    return path;
}

void BandmapGuideTest::saveProfilesRoundTripFiltersInvalidData()
{
    BandmapGuide::Profile valid;
    valid.id = QStringLiteral("guide-1");
    valid.name = QStringLiteral("Region 1");
    valid.ranges << BandmapGuide::Range(14.000, 14.070, QColor(QStringLiteral("#204080")), QStringLiteral("CW"));
    valid.ranges << BandmapGuide::Range(14.070, 14.070, QColor(QStringLiteral("#ff0000")), QStringLiteral("Zero"));
    valid.ranges << BandmapGuide::Range(14.100, 14.200, QColor(), QStringLiteral("No color"));

    BandmapGuide::Profile invalidProfile;
    invalidProfile.id = QStringLiteral("invalid");
    invalidProfile.ranges << BandmapGuide::Range(7.000, 7.030, QColor(QStringLiteral("#00ff00")));

    QSignalSpy changedSpy(BandmapGuide::instance(), &BandmapGuide::changed);
    QVERIFY(changedSpy.isValid());

    BandmapGuide::saveProfiles({valid, invalidProfile});

    QCOMPARE(changedSpy.count(), 1);

    const QList<BandmapGuide::Profile> profiles = BandmapGuide::profiles();
    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().id, QStringLiteral("guide-1"));
    QCOMPARE(profiles.first().name, QStringLiteral("Region 1"));
    QCOMPARE(profiles.first().ranges.size(), 1);
    compareRange(profiles.first().ranges.first(),
                 14.000,
                 14.070,
                 QColor(QStringLiteral("#204080")),
                 QStringLiteral("CW"));
}

void BandmapGuideTest::profilesUseExampleGuideOnFirstRun()
{
    const QList<BandmapGuide::Profile> profiles = BandmapGuide::profiles();

    QCOMPARE(profiles.size(), 1);
    QCOMPARE(profiles.first().id, QStringLiteral("iaru-region-1"));
    QCOMPARE(profiles.first().name, QStringLiteral("IARU Region 1"));
    QVERIFY(!profiles.first().ranges.isEmpty());
    QVERIFY(BandmapGuide::profileExists(QStringLiteral("iaru-region-1")));
    QCOMPARE(BandmapGuide::currentProfile().id, profiles.first().id);
}

void BandmapGuideTest::currentProfileFallsBackToFirstProfile()
{
    BandmapGuide::Profile first;
    first.id = QStringLiteral("first");
    first.name = QStringLiteral("First Guide");
    first.ranges << BandmapGuide::Range(14.000, 14.070, QColor(QStringLiteral("#204080")));

    BandmapGuide::Profile second;
    second.id = QStringLiteral("second");
    second.name = QStringLiteral("Second Guide");
    second.ranges << BandmapGuide::Range(7.000, 7.040, QColor(QStringLiteral("#804020")));

    BandmapGuide::saveProfiles({first, second});
    BandmapGuide::setCurrentProfileId(QStringLiteral("missing"));

    const BandmapGuide::Profile current = BandmapGuide::currentProfile();

    QCOMPARE(current.id, first.id);
    QCOMPARE(current.name, first.name);
}

void BandmapGuideTest::profileExistsRejectsEmptyAndMissingId()
{
    BandmapGuide::Profile profile;
    profile.id = QStringLiteral("guide-1");
    profile.name = QStringLiteral("Guide");
    profile.ranges << BandmapGuide::Range(14.000, 14.070, QColor(QStringLiteral("#204080")));
    BandmapGuide::saveProfiles({profile});

    QVERIFY(BandmapGuide::profileExists(QStringLiteral("guide-1")));
    QVERIFY(!BandmapGuide::profileExists(QString()));
    QVERIFY(!BandmapGuide::profileExists(QStringLiteral("missing")));
}

void BandmapGuideTest::setCurrentProfileIdEmitsChangedOnlyOnRealChange()
{
    QSignalSpy changedSpy(BandmapGuide::instance(), &BandmapGuide::changed);
    QVERIFY(changedSpy.isValid());

    BandmapGuide::setCurrentProfileId(QStringLiteral("guide-1"));
    QCOMPARE(changedSpy.count(), 1);

    BandmapGuide::setCurrentProfileId(QStringLiteral("guide-1"));
    QCOMPARE(changedSpy.count(), 1);

    BandmapGuide::setCurrentProfileId(QStringLiteral("guide-2"));
    QCOMPARE(changedSpy.count(), 2);
}

void BandmapGuideTest::setEnabledEmitsChangedOnlyOnRealChange()
{
    QSignalSpy changedSpy(BandmapGuide::instance(), &BandmapGuide::changed);
    QVERIFY(changedSpy.isValid());

    BandmapGuide::setEnabled(true);
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(BandmapGuide::isEnabled());

    BandmapGuide::setEnabled(true);
    QCOMPARE(changedSpy.count(), 1);

    BandmapGuide::setEnabled(false);
    QCOMPARE(changedSpy.count(), 2);
    QVERIFY(!BandmapGuide::isEnabled());
}

void BandmapGuideTest::writeReadProfileRoundTripAssignsNewId()
{
    BandmapGuide::Profile profile;
    profile.id = QStringLiteral("original-id");
    profile.name = QStringLiteral("Exported Guide");
    profile.ranges << BandmapGuide::Range(7.000, 7.040, QColor(QStringLiteral("#123456")), QStringLiteral("CW"));
    profile.ranges << BandmapGuide::Range(7.040, 7.074, QColor(QStringLiteral("#654321")), QStringLiteral("DIGI"));

    const QString path = tempDir.filePath(QStringLiteral("exported-guide.json"));
    QString error;
    QVERIFY2(BandmapGuide::writeProfileToFile(profile, path, &error), qPrintable(error));

    const BandmapGuide::Profile imported = BandmapGuide::readProfileFromFile(path, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(imported.isValid());
    QVERIFY(!imported.id.isEmpty());
    QVERIFY(imported.id != profile.id);
    QCOMPARE(imported.name, profile.name);
    QCOMPARE(imported.ranges.size(), 2);
    compareRange(imported.ranges.at(0), 7.000, 7.040, QColor(QStringLiteral("#123456")), QStringLiteral("CW"));
    compareRange(imported.ranges.at(1), 7.040, 7.074, QColor(QStringLiteral("#654321")), QStringLiteral("DIGI"));
}

void BandmapGuideTest::readProfileAcceptsProfilesWrapper()
{
    const QByteArray json = R"json({
        "version": 1,
        "profiles": [
            {
                "id": "stored-id",
                "title": "Wrapped Guide",
                "ranges": [
                    { "from": 14.000, "to": 14.070, "color": "#0a64c8", "label": "CW" },
                    { "from": 14.070, "to": 14.070, "color": "#ff0000", "label": "Invalid width" },
                    { "from": 14.100, "to": 14.200, "color": "not-a-color", "label": "Invalid color" }
                ]
            }
        ]
    })json";
    const QString path = writeTextFile(QStringLiteral("wrapped-guide.json"), json);

    QString error;
    const BandmapGuide::Profile imported = BandmapGuide::readProfileFromFile(path, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(imported.isValid());
    QVERIFY(!imported.id.isEmpty());
    QVERIFY(imported.id != QStringLiteral("stored-id"));
    QCOMPARE(imported.name, QStringLiteral("Wrapped Guide"));
    QCOMPARE(imported.ranges.size(), 1);
    compareRange(imported.ranges.first(),
                 14.000,
                 14.070,
                 QColor(QStringLiteral("#0a64c8")),
                 QStringLiteral("CW"));
}

void BandmapGuideTest::readProfileUsesFilenameWhenTitleMissing()
{
    const QByteArray json = R"json({
        "version": 1,
        "ranges": [
            { "from": 3.500, "to": 3.600, "color": "#abcdef", "label": "CW" }
        ]
    })json";
    const QString path = writeTextFile(QStringLiteral("filename-title.json"), json);

    QString error;
    const BandmapGuide::Profile imported = BandmapGuide::readProfileFromFile(path, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(imported.isValid());
    QCOMPARE(imported.name, QStringLiteral("filename-title"));
    QCOMPARE(imported.ranges.size(), 1);
    compareRange(imported.ranges.first(),
                 3.500,
                 3.600,
                 QColor(QStringLiteral("#abcdef")),
                 QStringLiteral("CW"));
}

void BandmapGuideTest::readProfileSupportsLegacyNameField()
{
    const QByteArray json = R"json({
        "version": 1,
        "name": "Legacy Guide",
        "ranges": [
            { "from": 21.000, "to": 21.070, "color": "#123abc", "label": "CW" }
        ]
    })json";
    const QString path = writeTextFile(QStringLiteral("legacy-name.json"), json);

    QString error;
    const BandmapGuide::Profile imported = BandmapGuide::readProfileFromFile(path, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(imported.isValid());
    QCOMPARE(imported.name, QStringLiteral("Legacy Guide"));
    QCOMPARE(imported.ranges.size(), 1);
    compareRange(imported.ranges.first(),
                 21.000,
                 21.070,
                 QColor(QStringLiteral("#123abc")),
                 QStringLiteral("CW"));
}

void BandmapGuideTest::readProfileRejectsInvalidJsonAndMissingFile()
{
    QString error;
    const BandmapGuide::Profile missing = BandmapGuide::readProfileFromFile(
        tempDir.filePath(QStringLiteral("missing-guide.json")), &error);

    QVERIFY(!missing.isValid());
    QVERIFY(!error.isEmpty());

    error.clear();
    const QString invalidPath = writeTextFile(QStringLiteral("invalid-guide.json"),
                                             QByteArrayLiteral("{ this is not json"));
    const BandmapGuide::Profile invalidJson = BandmapGuide::readProfileFromFile(invalidPath, &error);

    QVERIFY(!invalidJson.isValid());
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(BandmapGuideTest)

#include "tst_bandmapguide.moc"
