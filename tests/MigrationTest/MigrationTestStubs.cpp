#include <cstdlib>
#include "data/Data.h"
#include "logformat/LogFormat.h"
#include "service/clublog/ClubLog.h"
#include "service/hrdlog/HRDLog.h"

// These tests exercise database migrations without external DXCC resources or
// network services. Keep the uploader field lists identical to the services.
Data::Data(QObject *parent) : QObject(parent) {}
Data::~Data() = default;
void Data::invalidateDXCCStatusCache(const QSqlRecord &) {}
void Data::invalidateSetOfDXCCStatusCache(const QSet<uint> &) {}
void Data::clearDXCCStatusCache() {}

DxccEntity Data::lookupDxcc(const QString &)
{
    return {};
}

QPair<QString, QString> Data::legacyMode(const QString &)
{
    qFatal("Database lifecycle tests must not parse log formats");
    std::abort();
}

// The database lifecycle uses backup snapshots, but never starts their ADX export.
LogFormat::LogFormat(QTextStream &stream) :
    QObject(nullptr),
    stream(stream),
    exportedFields(QStringLiteral("*")),
    duplicateQSOFunc(nullptr)
{
    qFatal("Database lifecycle tests must not create log-format readers or writers");
    std::abort();
}

LogFormat::~LogFormat() = default;

QStringList ClubLogUploader::uploadedFields =
{
    "start_time", "qsl_rdate", "qsl_sdate", "callsign", "operator", "mode",
    "band", "band_rx", "freq", "qsl_rcvd", "lotw_qsl_rcvd", "qsl_sent",
    "dxcc", "prop_mode", "credit_granted", "rst_sent", "rst_rcvd", "notes",
    "gridsquare", "vucc_grids", "sat_name"
};

QStringList HRDLogUploader::uploadedFields =
{
    "start_time", "freq", "band", "callsign", "mode", "submode", "rst_rcvd",
    "rst_sent", "qsl_sent", "qsl_rcvd", "qsl_via", "eqsl_qslrdate",
    "eqsl_qsl_rcvd", "eqsl_qslsdate", "eqsl_qsl_sent", "lotw_qsl_rcvd",
    "lotw_qslrdate", "lotw_qsl_sent", "lotw_qslsdate", "gridsquare", "dxcc",
    "qslmsg", "comment", "distance", "operator", "sat_mode", "sat_name",
    "prop_mode"
};
