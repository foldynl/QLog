#ifndef QLOG_MODELS_WSJTXACTIVITY_H
#define QLOG_MODELS_WSJTXACTIVITY_H

#include <QDateTime>
#include <QList>
#include <QVariant>

class WsjtxActivity
{
public:
    static const int HISTORY_LENGTH = 8;

    struct Sample
    {
        QDateTime decodedAt;
        QDateTime receivedAt;
        qint32 snr;
    };

    QDateTime update(const QDateTime &receivedAt,
                     const QTime &decodeTime,
                     qint32 snr,
                     int periodMilliseconds);
    QVariantList history(const QDateTime &referencePeriod,
                         int periodMilliseconds) const;
    const QList<Sample> &samples() const;
    QDateTime latestActivity() const;
    static int signalLevel(qint32 snr);

private:
    static QDateTime decodeDateTime(const QDateTime &receivedAt,
                                    const QTime &decodeTime);

    QList<Sample> activitySamples;
};

#endif // QLOG_MODELS_WSJTXACTIVITY_H
