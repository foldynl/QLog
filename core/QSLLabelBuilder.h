#ifndef QLOG_CORE_QSLLABELBUILDER_H
#define QLOG_CORE_QSLLABELBUILDER_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QVector>

#include "core/QSLPrintLabelRenderer.h"

enum class QSLLabelOrder
{
    Callsign = 0,
    QslRoute = 1
};

struct QSLSourceQso
{
    QDateTime startTime;
    QString callsign;
    QString qslVia;
    QString prefix;
    QSLLabelData::QsoRow row;
    int dxcc = 0;
};

struct QSLLabelBuildOptions
{
    QString dateFormat;
    int maxRows = 4;
    QSLLabelOrder order = QSLLabelOrder::Callsign;
    bool showQslManager = false;
    bool showRoutePrefix = false;
};

class QSLLabelBuilder
{
public:
    void setSourceQsos(QVector<QSLSourceQso> qsos);
    void clear();
    QList<QSLLabelData> build(const QSLLabelBuildOptions &options);

private:
    struct Route
    {
        QString callsign;
        QString prefix;
        QString qslManager;
    };

    Route resolveRoute(const QSLSourceQso &source);
    QString qslManager(const QString &qslVia);
    QString callsignPrefix(const QString &callsign);
    QString dxccPrefix(int dxcc);

    QVector<QSLSourceQso> sourceQsos;
    QString formattedDateFormat;
    QHash<QString, QString> qslManagerCache;
    QHash<QString, QString> callsignPrefixCache;
    QHash<int, QString> dxccPrefixCache;
};

#endif // QLOG_CORE_QSLLABELBUILDER_H
