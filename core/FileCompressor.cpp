#include <QFile>
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

    return out;
}

bool FileCompressor::gzipFile(const QString &sourceFile, const QString &destFile,
                              const ProgressCallback &progress)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << sourceFile << "->" << destFile;

    QFile source(sourceFile);
    if ( !source.open(QIODevice::ReadOnly) )
    {
        qWarning() << "Cannot open source file:" << sourceFile;
        return false;
    }

    QFile dest(destFile);
    if ( !dest.open(QIODevice::WriteOnly) )
    {
        qWarning() << "Cannot create dest file:" << destFile;
        return false;
    }

    const qint64 totalSize = source.size();

    // Initialize zlib for gzip compression
    z_stream strm{};

    // 16 + MAX_WBITS = gzip format,
    if ( deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED,
                      16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK )
    {
        qWarning() << "deflateInit2 failed";
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
            qWarning() << "Read error";
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
                qWarning() << "deflate error";
                success = false;
                break;
            }

            qint64 have = sizeof(outBuffer) - strm.avail_out;
            if ( dest.write(outBuffer, have) != have )
            {
                qWarning() << "Write error";
                success = false;
                break;
            }
        } while ( strm.avail_out == 0 && success );

        bytesProcessed += bytesRead;

        if ( progress && !progress(bytesProcessed, totalSize) )
        {
            qCDebug(runtime) << "Compression cancelled by user";
            success = false;
            break;
        }

        if ( source.atEnd() )
            break;
    }

    deflateEnd(&strm);

    if ( !success || ret != Z_STREAM_END )
    {
        dest.close();
        QFile::remove(destFile);
        return false;
    }

    qCDebug(runtime) << "File compressed successfully";
    return true;
}

bool FileCompressor::gunzipFile(const QString &sourceFile, const QString &destFile,
                                const ProgressCallback &progress)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << sourceFile << "->" << destFile;

    QFile source(sourceFile);
    if ( !source.open(QIODevice::ReadOnly) )
    {
        qWarning() << "Cannot open source file:" << sourceFile;
        return false;
    }

    QFile dest(destFile);
    if ( !dest.open(QIODevice::WriteOnly) )
    {
        qWarning() << "Cannot create dest file:" << destFile;
        return false;
    }

    const qint64 totalSize = source.size();

    // Initialize zlib for gzip decompression
    z_stream strm{};

    // 16 + MAX_WBITS tells zlib to parse gzip header/footer
    if ( inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK )
    {
        qWarning() << "inflateInit2 failed";
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
            qWarning() << "Read error";
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
                qWarning() << "inflate error:" << ret;
                success = false;
                break;
            }

            qint64 have = sizeof(outBuffer) - strm.avail_out;
            if ( dest.write(outBuffer, have) != have )
            {
                qWarning() << "Write error";
                success = false;
                break;
            }
        } while ( strm.avail_out == 0 && success );

        bytesProcessed += bytesRead;

        if ( progress && !progress(bytesProcessed, totalSize) )
        {
            qCDebug(runtime) << "Decompression cancelled by user";
            success = false;
            break;
        }
    }

    inflateEnd(&strm);

    if ( !success )
    {
        dest.close();
        QFile::remove(destFile);
        return false;
    }

    qCDebug(runtime) << "File decompressed successfully";
    return true;
}

bool FileCompressor::gzipFileWithProgress(const QString &sourceFile, const QString &destFile,
                                          QWidget *parent, const QString &title)
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

    bool result = gzipFile(sourceFile, destFile, progressCallback);

    progressDialog.setValue(100);

    return result;
}

bool FileCompressor::gunzipFileWithProgress(const QString &sourceFile, const QString &destFile,
                                            QWidget *parent, const QString &title)
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

    bool result = gunzipFile(sourceFile, destFile, progressCallback);

    progressDialog.setValue(100);

    return result;
}
