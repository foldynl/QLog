#include <QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFile>
#include <QRandomGenerator>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#include <signal.h>
#endif

#include "core/FileCompressor.h"

class FileCompressorTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // In-memory tests
    void gzip_emptyData_returnsEmpty();
    void gzip_smallData_compressesAndDecompresses();
    void gzip_largeData_compressesAndDecompresses();
    void gzip_binaryData_compressesAndDecompresses();
    void gunzip_invalidData_returnsEmpty();
    void gunzip_truncatedData_returnsEmpty();

    // File-based tests
    void gzipFile_smallFile_compressesAndDecompresses();
    void gzipFile_largeFile_compressesAndDecompresses();
    void gzipFile_nonExistentSource_returnsFalse();
    void gzipFile_invalidDestPath_returnsFalse();
    void gzipFile_progressCallback_isCalled();
    void gzipFile_progressCallbackCancel_stopsCompression();
    void gzipFile_failurePreservesDestination_data();
    void gzipFile_failurePreservesDestination();
    void gzipFile_successPublishesCompleteArchive();
    void gzipFile_commitFailureReturnsFalse();
    void gzipFile_reportsErrors();
    void fileWriteFailurePreservesDestination_data();
    void fileWriteFailurePreservesDestination();
    void gunzipFile_requiresStreamEnd_data();
    void gunzipFile_requiresStreamEnd();
    void gunzipFile_reportsErrors_data();
    void gunzipFile_reportsErrors();

    // Benchmarks
    void gzip_benchmark_compression();
    void gzip_benchmark_decompression();

private:
    QByteArray generateRandomData(int size);
    QByteArray generateCompressibleData(int size);

    QTemporaryDir *tempDir = nullptr;
};

void FileCompressorTest::initTestCase()
{
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
}

void FileCompressorTest::cleanupTestCase()
{
    delete tempDir;
    tempDir = nullptr;
}

QByteArray FileCompressorTest::generateRandomData(int size)
{
    QByteArray data(size, '\0');
    for (int i = 0; i < size; ++i)
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return data;
}

QByteArray FileCompressorTest::generateCompressibleData(int size)
{
    // Generate data with repeated patterns (highly compressible)
    QByteArray pattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QByteArray data;
    data.reserve(size);
    while (data.size() < size)
        data.append(pattern);
    return data.left(size);
}

// ============================================================================
// In-memory tests
// ============================================================================

void FileCompressorTest::gzip_emptyData_returnsEmpty()
{
    QByteArray empty;
    QByteArray compressed = FileCompressor::gzip(empty);
    QVERIFY(compressed.isEmpty());

    QByteArray decompressed = FileCompressor::gunzip(empty);
    QVERIFY(decompressed.isEmpty());
}

void FileCompressorTest::gzip_smallData_compressesAndDecompresses()
{
    QByteArray original = "Hello, World! This is a test message for compression.";

    QByteArray compressed = FileCompressor::gzip(original);
    QVERIFY(!compressed.isEmpty());
    QVERIFY(compressed != original);

    QByteArray decompressed = FileCompressor::gunzip(compressed);
    QCOMPARE(decompressed, original);
}

void FileCompressorTest::gzip_largeData_compressesAndDecompresses()
{
    // 1 MB of compressible data
    QByteArray original = generateCompressibleData(1024 * 1024);

    QByteArray compressed = FileCompressor::gzip(original);
    QVERIFY(!compressed.isEmpty());
    // Compressible data should be significantly smaller
    QVERIFY(compressed.size() < original.size() / 2);

    QByteArray decompressed = FileCompressor::gunzip(compressed);
    QCOMPARE(decompressed, original);
}

void FileCompressorTest::gzip_binaryData_compressesAndDecompresses()
{
    // Random binary data (less compressible)
    QByteArray original = generateRandomData(10000);

    QByteArray compressed = FileCompressor::gzip(original);
    QVERIFY(!compressed.isEmpty());

    QByteArray decompressed = FileCompressor::gunzip(compressed);
    QCOMPARE(decompressed, original);
}

void FileCompressorTest::gunzip_invalidData_returnsEmpty()
{
    QByteArray invalid = "This is not gzip compressed data";
    QByteArray result = FileCompressor::gunzip(invalid);
    QVERIFY(result.isEmpty());
}

void FileCompressorTest::gunzip_truncatedData_returnsEmpty()
{
    QByteArray original = "Test data for truncation test";
    QByteArray compressed = FileCompressor::gzip(original);
    QVERIFY(!compressed.isEmpty());

    // Truncate the compressed data
    QByteArray truncated = compressed.left(compressed.size() / 2);
    QByteArray result = FileCompressor::gunzip(truncated);
    // Should return empty or partial data, but not crash
    QVERIFY(result != original);
}

// ============================================================================
// File-based tests
// ============================================================================

void FileCompressorTest::gzipFile_smallFile_compressesAndDecompresses()
{
    QString sourceFile = tempDir->filePath("small_source.txt");
    QString compressedFile = tempDir->filePath("small_compressed.gz");
    QString decompressedFile = tempDir->filePath("small_decompressed.txt");

    // Create source file
    QByteArray original = "Small file content for compression test.\n";
    {
        QFile file(sourceFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(original);
    }

    // Compress
    QVERIFY(FileCompressor::gzipFile(sourceFile, compressedFile));
    QVERIFY(QFile::exists(compressedFile));

    // Decompress
    QVERIFY(FileCompressor::gunzipFile(compressedFile, decompressedFile));
    QVERIFY(QFile::exists(decompressedFile));

    // Verify content
    QFile resultFile(decompressedFile);
    QVERIFY(resultFile.open(QIODevice::ReadOnly));
    QCOMPARE(resultFile.readAll(), original);
}

void FileCompressorTest::gzipFile_largeFile_compressesAndDecompresses()
{
    QString sourceFile = tempDir->filePath("large_source.bin");
    QString compressedFile = tempDir->filePath("large_compressed.gz");
    QString decompressedFile = tempDir->filePath("large_decompressed.bin");

    // Create 5 MB source file with compressible data
    QByteArray original = generateCompressibleData(5 * 1024 * 1024);
    {
        QFile file(sourceFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(original);
    }

    // Compress
    QVERIFY(FileCompressor::gzipFile(sourceFile, compressedFile));
    QVERIFY(QFile::exists(compressedFile));

    // Compressed file should be smaller
    QFileInfo compressedInfo(compressedFile);
    QVERIFY(compressedInfo.size() < original.size() / 2);

    // Decompress
    QVERIFY(FileCompressor::gunzipFile(compressedFile, decompressedFile));

    // Verify content
    QFile resultFile(decompressedFile);
    QVERIFY(resultFile.open(QIODevice::ReadOnly));
    QCOMPARE(resultFile.readAll(), original);
}

void FileCompressorTest::gzipFile_nonExistentSource_returnsFalse()
{
    QString sourceFile = tempDir->filePath("non_existent_file.txt");
    QString compressedFile = tempDir->filePath("output.gz");

    QVERIFY(!FileCompressor::gzipFile(sourceFile, compressedFile));
    QVERIFY(!QFile::exists(compressedFile));
}

void FileCompressorTest::gzipFile_invalidDestPath_returnsFalse()
{
    QString sourceFile = tempDir->filePath("source_for_invalid_dest.txt");
    QString invalidDest = "/nonexistent/path/output.gz";

    // Create source file
    {
        QFile file(sourceFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
    }

    QVERIFY(!FileCompressor::gzipFile(sourceFile, invalidDest));
}

void FileCompressorTest::gzipFile_progressCallback_isCalled()
{
    QString sourceFile = tempDir->filePath("progress_source.bin");
    QString compressedFile = tempDir->filePath("progress_compressed.gz");

    // Create 1 MB file
    QByteArray original = generateCompressibleData(1024 * 1024);
    {
        QFile file(sourceFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(original);
    }

    int callCount = 0;
    qint64 lastProcessed = 0;
    bool progressValid = true;

    auto progressCallback = [&](qint64 processed, qint64 total) -> bool {
        ++callCount;
        if (processed < lastProcessed || total <= 0)
            progressValid = false;
        lastProcessed = processed;
        return true;  // Continue
    };

    QVERIFY(FileCompressor::gzipFile(sourceFile, compressedFile, progressCallback));
    QVERIFY(callCount > 0);
    QVERIFY(progressValid);
}

void FileCompressorTest::gzipFile_progressCallbackCancel_stopsCompression()
{
    QString sourceFile = tempDir->filePath("cancel_source.bin");
    QString compressedFile = tempDir->filePath("cancel_compressed.gz");

    // Create 2 MB file
    QByteArray original = generateCompressibleData(2 * 1024 * 1024);
    {
        QFile file(sourceFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(original);
    }

    int callCount = 0;

    auto progressCallback = [&](qint64, qint64) -> bool {
        ++callCount;
        // Cancel after a few calls
        return callCount < 3;
    };

    QVERIFY(!FileCompressor::gzipFile(sourceFile, compressedFile, progressCallback));
    // Destination file should be cleaned up
    QVERIFY(!QFile::exists(compressedFile));
}

void FileCompressorTest::gzipFile_failurePreservesDestination_data()
{
    QTest::addColumn<bool>("missingSource");
    QTest::newRow("missing-source") << true;
    QTest::newRow("cancelled") << false;
}

void FileCompressorTest::gzipFile_failurePreservesDestination()
{
    QFETCH(bool, missingSource);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source");
    const QString destinationPath = directory.filePath("backup.dbe");
    const QByteArray previousBackup("previous backup");
    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::WriteOnly));
    QCOMPARE(destination.write(previousBackup), qint64(previousBackup.size()));
    destination.close();
    if ( !missingSource )
    {
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("new data"), qint64(8));
    }

    QVERIFY(!FileCompressor::gzipFile(sourcePath, destinationPath,
                                      [](qint64, qint64) { return false; }));
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), previousBackup);
}

void FileCompressorTest::gzipFile_successPublishesCompleteArchive()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source");
    const QString destinationPath = directory.filePath("backup.dbe");
    const QByteArray original("complete replacement data");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(original), qint64(original.size()));
    source.close();
    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::WriteOnly));
    QCOMPARE(destination.write("old backup"), qint64(10));
    destination.close();

    QVERIFY(FileCompressor::gzipFile(sourcePath, destinationPath));
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(FileCompressor::gunzip(destination.readAll()), original);
}

void FileCompressorTest::gzipFile_commitFailureReturnsFalse()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source");
    const QString destinationPath = directory.filePath("backup.dbe");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("new data"), qint64(8));
    source.close();
    bool destinationBlocked = false;

    const bool result = FileCompressor::gzipFile(sourcePath, destinationPath,
        [&](qint64, qint64)
        {
            // A directory at the target makes final publication fail on all platforms.
            QFile::remove(destinationPath);
            destinationBlocked = QDir().mkdir(destinationPath);
            return true;
        });
    QVERIFY(destinationBlocked);
    QVERIFY(!result);
    QVERIFY(QFileInfo(destinationPath).isDir());
}

void FileCompressorTest::gzipFile_reportsErrors()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source");
    const QString destinationPath = directory.filePath("backup.dbe");
    QString error;
    QVERIFY(!FileCompressor::gzipFile(sourcePath, destinationPath, nullptr, &error));
    QVERIFY(!error.isEmpty());

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("new data"), qint64(8));
    source.close();
    QVERIFY(!FileCompressor::gzipFile(sourcePath, directory.path(), nullptr, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(FileCompressor::gzipFile(sourcePath, destinationPath, nullptr, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(!FileCompressor::gzipFile(sourcePath, destinationPath,
                                      [](qint64, qint64) { return false; }, &error));
    QVERIFY(!error.isEmpty());
    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(FileCompressor::gunzip(destination.readAll()), QByteArray("new data"));
}

void FileCompressorTest::gunzipFile_requiresStreamEnd_data()
{
    QTest::addColumn<QByteArray>("archive");
    QTest::addColumn<bool>("valid");
    const QByteArray compressed = FileCompressor::gzip("complete replacement data");
    QVERIFY(compressed.size() > 8);
    QTest::newRow("valid") << compressed << true;
    QTest::newRow("missing-footer") << compressed.left(compressed.size() - 8) << false;
    QTest::newRow("truncated-payload") << compressed.left(compressed.size() / 2) << false;
    QByteArray badCrc = compressed;
    badCrc[badCrc.size() - 8] = char(badCrc.at(badCrc.size() - 8) ^ 1);
    QTest::newRow("invalid-crc") << badCrc << false;
    QTest::newRow("empty") << QByteArray() << false;
}

void FileCompressorTest::fileWriteFailurePreservesDestination_data()
{
    QTest::addColumn<bool>("decompress");
    QTest::newRow("gzip") << false;
    QTest::newRow("gunzip") << true;
}

void FileCompressorTest::fileWriteFailurePreservesDestination()
{
#ifdef Q_OS_UNIX
    QFETCH(bool, decompress);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source");
    const QString destinationPath = directory.filePath("output");
    const QByteArray original("complete replacement data");
    const QByteArray sourceData = decompress ? FileCompressor::gzip(original) : original;
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(sourceData), qint64(sourceData.size()));
    source.close();
    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::WriteOnly));
    QCOMPARE(destination.write("previous output"), qint64(15));
    destination.close();

    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit restrictedLimit = originalLimit;
    restrictedLimit.rlim_cur = 0;
    const auto previousSignal = signal(SIGXFSZ, SIG_IGN);
    const int restricted = setrlimit(RLIMIT_FSIZE, &restrictedLimit);
    bool result = false;
    if ( restricted == 0 )
        result = decompress ? FileCompressor::gunzipFile(sourcePath, destinationPath)
                            : FileCompressor::gzipFile(sourcePath, destinationPath);
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    signal(SIGXFSZ, previousSignal);
    QVERIFY(restricted == 0);
    QVERIFY(restored == 0);
    QVERIFY(!result);
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), QByteArray("previous output"));
#else
    QSKIP("Requires the Unix file-size limit to force a real write failure.");
#endif
}

void FileCompressorTest::gunzipFile_requiresStreamEnd()
{
    QFETCH(QByteArray, archive);
    QFETCH(bool, valid);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source.gz");
    const QString destinationPath = directory.filePath("output");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(archive), qint64(archive.size()));
    source.close();
    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::WriteOnly));
    QCOMPARE(destination.write("previous output"), qint64(15));
    destination.close();

    QCOMPARE(FileCompressor::gunzipFile(sourcePath, destinationPath), valid);
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), valid ? QByteArray("complete replacement data")
                                        : QByteArray("previous output"));
}

void FileCompressorTest::gunzipFile_reportsErrors_data()
{
    QTest::addColumn<QString>("failure");
    QTest::newRow("missing-source") << QString("missing-source");
    QTest::newRow("invalid-archive") << QString("invalid-archive");
    QTest::newRow("truncated-archive") << QString("truncated-archive");
    QTest::newRow("blocked-output") << QString("blocked-output");
    QTest::newRow("cancelled") << QString("cancelled");
    QTest::newRow("commit-failure") << QString("commit-failure");
}

void FileCompressorTest::gunzipFile_reportsErrors()
{
    QFETCH(QString, failure);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath("source.dbe");
    const QString destinationPath = directory.filePath("database.db");
    if ( failure != "missing-source" )
    {
        QByteArray archive = FileCompressor::gzip("packed data");
        if ( failure == "invalid-archive" )
            archive = "not a packed file";
        else if ( failure == "truncated-archive" )
            archive.chop(4);
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(archive), qint64(archive.size()));
    }
    if ( failure == "blocked-output" )
        QVERIFY(QDir().mkdir(destinationPath));

    bool blockedCommit = false;
    QString error("stale error");
    QVERIFY(!FileCompressor::gunzipFile(sourcePath, destinationPath,
        [&](qint64, qint64)
        {
            if ( failure == "commit-failure" )
                blockedCommit = QDir().mkdir(destinationPath);
            return failure != "cancelled";
        }, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(error != "stale error");
    const bool outputFailure = failure == "blocked-output" || failure == "commit-failure";
    QVERIFY2(error.contains(outputFailure ? destinationPath : sourcePath), qPrintable(error));
    if ( failure == "commit-failure" )
        QVERIFY(blockedCommit);

    QFile validSource(sourcePath);
    QVERIFY(validSource.open(QIODevice::WriteOnly));
    const QByteArray archive = FileCompressor::gzip("packed data");
    QCOMPARE(validSource.write(archive), qint64(archive.size()));
    validSource.close();
    if ( outputFailure )
        QVERIFY(QDir().rmdir(destinationPath));
    QVERIFY(FileCompressor::gunzipFile(sourcePath, destinationPath, nullptr, &error));
    QVERIFY(error.isEmpty());
}

// ============================================================================
// Benchmarks
// ============================================================================

void FileCompressorTest::gzip_benchmark_compression()
{
    QByteArray data = generateCompressibleData(1024 * 1024);  // 1 MB

    QByteArray compressed;
    QBENCHMARK {
        compressed = FileCompressor::gzip(data);
    }

    QVERIFY(!compressed.isEmpty());
}

void FileCompressorTest::gzip_benchmark_decompression()
{
    QByteArray data = generateCompressibleData(1024 * 1024);  // 1 MB
    QByteArray compressed = FileCompressor::gzip(data);

    QByteArray decompressed;
    QBENCHMARK {
        decompressed = FileCompressor::gunzip(compressed);
    }

    QCOMPARE(decompressed, data);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    FileCompressorTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_filecompressor.moc"
