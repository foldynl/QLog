#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QProgressDialog>
#include <QCoreApplication>
#include <zlib.h>
#include "FileCompressor.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.filecompressor");

QByteArray FileCompressor::gzip(const QByteArray &in)
{
    FCT_IDENTIFICATION;

    if ( in.isEmpty() )
        return {};

    z_stream strm{};

    // 16 + MAX_WBITS = gzip format
    if ( deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED,
                      16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK )
        return {};

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    strm.avail_in = static_cast<uInt>(in.size());

    QByteArray out;
    char buf[8192];
    int ret;

    do
    {
        strm.next_out = reinterpret_cast<Bytef*>(buf);
        strm.avail_out = sizeof(buf);

        ret = deflate(&strm, Z_FINISH);

        if ( ret == Z_OK || ret == Z_STREAM_END )
            out.append(buf, static_cast<qsizetype>(sizeof(buf) - strm.avail_out));
    }
    while ( ret == Z_OK );

    deflateEnd(&strm);

    return (ret == Z_STREAM_END) ? out : QByteArray{};
}

QByteArray FileCompressor::gunzip(const QByteArray &in)
{
    FCT_IDENTIFICATION;

    if ( in.isEmpty() )
        return {};

    z_stream strm{};

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    strm.avail_in = static_cast<uInt>(in.size());

    // 16 + MAX_WBITS tells zlib to parse gzip header/footer
    if ( inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK )
        return {};

    QByteArray out;
    char buf[8192];
    int ret = Z_OK;

    while ( ret == Z_OK )
    {
        strm.next_out = reinterpret_cast<Bytef*>(buf);
        strm.avail_out = sizeof(buf);

        ret = inflate(&strm, Z_NO_FLUSH);

        if ( ret == Z_OK || ret == Z_STREAM_END )
            out.append(buf, static_cast<qsizetype>(sizeof(buf) - strm.avail_out));
    }

    inflateEnd(&strm);

    return ret == Z_STREAM_END ? out : QByteArray{};
}

bool FileCompressor::gzipFile(const QString &sourceFile, const QString &destFile,
                              const ProgressCallback &progress, QString *error)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << sourceFile << "->" << destFile;

    if ( error )
        error->clear();

    QFile source(sourceFile);
    if ( !source.open(QIODevice::ReadOnly) )
    {
        qCWarning(runtime) << "Cannot open source file:" << sourceFile << source.errorString();
        if ( error )
            *error = tr("Cannot open the source file for packing %1:\n%2").arg(sourceFile, source.errorString());
        return false;
    }

    QSaveFile dest(destFile);
    dest.setDirectWriteFallback(false);
    if ( !dest.open(QIODevice::WriteOnly) )
    {
        qCWarning(runtime) << "Cannot create dest file:" << destFile << dest.errorString();
        if ( error )
            *error = tr("Cannot create the packed output file %1:\n%2").arg(destFile, dest.errorString());
        return false;
    }

    const qint64 totalSize = source.size();

    // Initialize zlib for gzip compression
    z_stream strm{};

    // 16 + MAX_WBITS = gzip format,
    if ( deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED,
                      16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK )
    {
        qCWarning(runtime) << "deflateInit2 failed";
        if ( error )
            *error = tr("Cannot start packing %1. The compressor could not be initialized.").arg(sourceFile);
        return false;
    }

    char inBuffer[65536];
    char outBuffer[65536];
    qint64 bytesProcessed = 0;
    bool success = true;
    int ret;

    while ( success )
    {
        qint64 bytesRead = source.read(inBuffer, sizeof(inBuffer));
        if ( bytesRead < 0 )
        {
            qCWarning(runtime) << "Read error";
            if ( error )
                *error = tr("Cannot read the source data while packing %1:\n%2").arg(sourceFile, source.errorString());
            success = false;
            break;
        }

        strm.next_in = reinterpret_cast<Bytef*>(inBuffer);
        strm.avail_in = static_cast<uInt>(bytesRead);

        int flush = source.atEnd() ? Z_FINISH : Z_NO_FLUSH;

        do
        {
            strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
            strm.avail_out = sizeof(outBuffer);

            ret = deflate(&strm, flush);
            if ( ret == Z_STREAM_ERROR )
            {
                qCWarning(runtime) << "deflate error";
                if ( error )
                    *error = tr("Cannot compress the data from %1.").arg(sourceFile);
                success = false;
                break;
            }

            qint64 have = sizeof(outBuffer) - strm.avail_out;
            if ( dest.write(outBuffer, have) != have )
            {
                qCWarning(runtime) << "Write error";
                if ( error )
                    *error = tr("Cannot write the packed data to %1:\n%2").arg(destFile, dest.errorString());
                success = false;
                break;
            }
        } while ( strm.avail_out == 0 && success );

        bytesProcessed += bytesRead;

        if ( progress && !progress(bytesProcessed, totalSize) )
        {
            qCDebug(runtime) << "Compression cancelled by user";
            if ( error )
                *error = tr("Packing %1 was cancelled.").arg(sourceFile);
            success = false;
            break;
        }

        if ( source.atEnd() )
            break;
    }

    deflateEnd(&strm);

    if ( !success || ret != Z_STREAM_END )
    {
        if ( error && error->isEmpty() )
            *error = tr("Cannot complete packing %1. The compressed data is incomplete.").arg(sourceFile);
        return false;
    }

    // Some Qt5 builds do not propagate buffered write failures from commit().
    if ( !dest.flush() || !dest.commit() )
    {
        qCWarning(runtime) << "Cannot commit compressed file:" << destFile << dest.errorString();
        if ( error )
            *error = tr("Cannot finish saving the packed file %1:\n%2").arg(destFile, dest.errorString());
        return false;
    }

    qCDebug(runtime) << "File compressed successfully";
    return true;
}

bool FileCompressor::gunzipFile(const QString &sourceFile, const QString &destFile,
                                const ProgressCallback &progress, QString *error)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << sourceFile << "->" << destFile;
    if ( error )
        error->clear();

    QFile source(sourceFile);
    if ( !source.open(QIODevice::ReadOnly) )
    {
        qCWarning(runtime) << "Cannot open source file:" << sourceFile << source.errorString();
        if ( error )
            *error = tr("Cannot open packed file %1:\n%2").arg(sourceFile, source.errorString());
        return false;
    }

    QSaveFile dest(destFile);
    dest.setDirectWriteFallback(false);
    if ( !dest.open(QIODevice::WriteOnly) )
    {
        qCWarning(runtime) << "Cannot create dest file:" << destFile << dest.errorString();
        if ( error )
            *error = tr("Cannot create unpacked file %1:\n%2").arg(destFile, dest.errorString());
        return false;
    }

    const qint64 totalSize = source.size();

    // Initialize zlib for gzip decompression
    z_stream strm{};

    // 16 + MAX_WBITS tells zlib to parse gzip header/footer
    const int initialization = inflateInit2(&strm, 16 + MAX_WBITS);
    if ( initialization != Z_OK )
    {
        qCWarning(runtime) << "inflateInit2 failed" << initialization;
        if ( error )
            *error = initialization == Z_MEM_ERROR
                         ? tr("Cannot unpack %1. There is not enough memory to start decompression.").arg(sourceFile)
                         : tr("Cannot start unpacking %1 (decompression error %2).").arg(sourceFile).arg(initialization);
        return false;
    }

    char inBuffer[65536];
    char outBuffer[65536];
    qint64 bytesProcessed = 0;
    bool success = true;
    int ret = Z_OK;

    while ( ret != Z_STREAM_END && success )
    {
        qint64 bytesRead = source.read(inBuffer, sizeof(inBuffer));
        if ( bytesRead < 0 )
        {
            qCWarning(runtime) << "Read error" << source.errorString();
            if ( error )
                *error = tr("Cannot read packed file %1:\n%2").arg(sourceFile, source.errorString());
            success = false;
            break;
        }

        if ( bytesRead == 0 )
            break;

        strm.next_in = reinterpret_cast<Bytef*>(inBuffer);
        strm.avail_in = static_cast<uInt>(bytesRead);

        do
        {
            strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
            strm.avail_out = sizeof(outBuffer);

            ret = inflate(&strm, Z_NO_FLUSH);
            if ( ret == Z_STREAM_ERROR || ret == Z_NEED_DICT ||
                 ret == Z_DATA_ERROR || ret == Z_MEM_ERROR )
            {
                qCWarning(runtime) << "inflate error:" << ret;
                if ( error )
                    *error = ret == Z_MEM_ERROR ? tr("Cannot unpack %1. There is not enough memory.").arg(sourceFile)
                                               : tr("Cannot unpack %1. The packed file is damaged or is not a valid QLog packed file.").arg(sourceFile);
                success = false;
                break;
            }

            qint64 have = sizeof(outBuffer) - strm.avail_out;
            if ( dest.write(outBuffer, have) != have )
            {
                qCWarning(runtime) << "Write error" << dest.errorString();
                if ( error )
                    *error = tr("Cannot write unpacked file %1:\n%2").arg(destFile, dest.errorString());
                success = false;
                break;
            }
        } while ( strm.avail_out == 0 && success );

        bytesProcessed += bytesRead;

        if ( progress && !progress(bytesProcessed, totalSize) )
        {
            qCDebug(runtime) << "Decompression cancelled by user";
            if ( error )
                *error = tr("Unpacking %1 was cancelled.").arg(sourceFile);
            success = false;
            break;
        }
    }

    inflateEnd(&strm);

    if ( !success || ret != Z_STREAM_END )
    {
        qCWarning(runtime) << "Incomplete or invalid compressed file:" << sourceFile;
        if ( error && error->isEmpty() )
            *error = tr("Cannot unpack %1. The packed file is incomplete or damaged.").arg(sourceFile);
        return false;
    }

    if ( !dest.flush() || !dest.commit() )
    {
        qCWarning(runtime) << "Cannot commit decompressed file:" << destFile << dest.errorString();
        if ( error )
            *error = tr("Cannot save unpacked file %1:\n%2").arg(destFile, dest.errorString());
        return false;
    }

    qCDebug(runtime) << "File decompressed successfully";
    return true;
}

bool FileCompressor::gzipFileWithProgress(const QString &sourceFile, const QString &destFile,
                                          QWidget *parent, const QString &title, QString *error)
{
    FCT_IDENTIFICATION;

    QProgressDialog progressDialog(title, QString(), 0, 100, parent);
    progressDialog.setWindowTitle(title);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(500);  // Show after 500ms
    progressDialog.setValue(0);

    FileCompressor::ProgressCallback progressCallback = [&progressDialog](qint64 processed, qint64 total)
    {
        if ( total > 0 )
            progressDialog.setValue(static_cast<int>(processed * 100 / total));
        QCoreApplication::processEvents();
        return !progressDialog.wasCanceled();
    };

    bool result = gzipFile(sourceFile, destFile, progressCallback, error);

    progressDialog.setValue(100);

    return result;
}

bool FileCompressor::gunzipFileWithProgress(const QString &sourceFile, const QString &destFile,
                                            QWidget *parent, const QString &title, QString *error)
{
    FCT_IDENTIFICATION;

    QProgressDialog progressDialog(title, QString(), 0, 100, parent);
    progressDialog.setWindowTitle(title);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(500);  // Show after 500ms
    progressDialog.setValue(0);

    FileCompressor::ProgressCallback progressCallback = [&progressDialog](qint64 processed, qint64 total)
    {
        if ( total > 0 )
            progressDialog.setValue(static_cast<int>(processed * 100 / total));
        QCoreApplication::processEvents();
        return !progressDialog.wasCanceled();
    };

    bool result = gunzipFile(sourceFile, destFile, progressCallback, error);

    progressDialog.setValue(100);

    return result;
}
