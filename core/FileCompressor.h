#ifndef QLOG_CORE_FILECOMPRESSOR_H
#define QLOG_CORE_FILECOMPRESSOR_H

#include <QString>
#include <QCoreApplication>
#include <QByteArray>
#include <functional>

class QWidget;

class FileCompressor
{
    Q_DECLARE_TR_FUNCTIONS(FileCompressor)

public:
    // Progress callback: (bytesProcessed, totalBytes); return false to cancel
    using ProgressCallback = std::function<bool(qint64, qint64)>;

    // Compress data using gzip (in-memory)
    static QByteArray gzip(const QByteArray &in);

    // Decompress gzip data (in-memory)
    static QByteArray gunzip(const QByteArray &in);

    // Compress file using gzip (streaming)
    static bool gzipFile(const QString &sourceFile, const QString &destFile,
                         const ProgressCallback &progress = nullptr, QString *error = nullptr);

    // Decompress gzip file (streaming)
    static bool gunzipFile(const QString &sourceFile, const QString &destFile,
                           const ProgressCallback &progress = nullptr);

    // File compression/decompression with progress dialog
    static bool gzipFileWithProgress(const QString &sourceFile, const QString &destFile,
                                     QWidget *parent, const QString &title, QString *error = nullptr);
    static bool gunzipFileWithProgress(const QString &sourceFile, const QString &destFile,
                                       QWidget *parent, const QString &title);
};

#endif // QLOG_CORE_FILECOMPRESSOR_H
