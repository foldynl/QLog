#include "WsjtxActivity.h"

#include <QtGlobal>

QDateTime WsjtxActivity::update(const QDateTime &receivedAt,
                                const QTime &decodeTime,
                                qint32 snr,
                                int periodMilliseconds)
{
    if ( !receivedAt.isValid() || !decodeTime.isValid() || periodMilliseconds <= 0 )
        return QDateTime();

    const QDateTime receivedUtc = receivedAt.toUTC();
    const QDateTime decodedAt = decodeDateTime(receivedUtc, decodeTime);

    for ( Sample &sample : activitySamples )
    {
        if ( sample.decodedAt == decodedAt )
        {
            sample.receivedAt = receivedUtc;
            sample.snr = snr;
            return decodedAt;
        }
    }

    Sample sample;
    sample.decodedAt = decodedAt;
    sample.receivedAt = receivedUtc;
    sample.snr = snr;

    int insertionIndex = 0;
    while ( insertionIndex < activitySamples.size()
            && activitySamples.at(insertionIndex).decodedAt < decodedAt )
    {
        ++insertionIndex;
    }

    activitySamples.insert(insertionIndex, sample);
    while ( activitySamples.size() > HISTORY_LENGTH )
        activitySamples.removeFirst();

    return decodedAt;
}

QVariantList WsjtxActivity::history(const QDateTime &referencePeriod,
                                    int periodMilliseconds) const
{
    QVariantList result;
    for ( int i = 0; i < HISTORY_LENGTH; ++i )
        result.append(QVariant());
    if ( !referencePeriod.isValid() || periodMilliseconds <= 0 )
        return result;

    const QDateTime referenceUtc = referencePeriod.toUTC();

    for ( const Sample &sample : activitySamples )
    {
        const qint64 elapsed = sample.decodedAt.msecsTo(referenceUtc);
        if ( elapsed < 0 )
            continue;

        const qint64 periodOffset = (elapsed + periodMilliseconds / 2)
                                    / periodMilliseconds;
        if ( periodOffset >= HISTORY_LENGTH )
            continue;

        result[HISTORY_LENGTH - 1 - static_cast<int>(periodOffset)] = sample.snr;
    }

    return result;
}

const QList<WsjtxActivity::Sample> &WsjtxActivity::samples() const
{
    return activitySamples;
}

QDateTime WsjtxActivity::latestActivity() const
{
    QDateTime latest;
    for ( const Sample &sample : activitySamples )
    {
        if ( !latest.isValid() || latest < sample.receivedAt )
            latest = sample.receivedAt;
    }
    return latest;
}

int WsjtxActivity::signalLevel(qint32 snr)
{
    if ( snr <= -15 )
        return 1;
    if ( snr <= -7 )
        return 2;
    return 3;
}

QDateTime WsjtxActivity::decodeDateTime(const QDateTime &receivedAt,
                                        const QTime &decodeTime)
{
    const QDateTime receivedUtc = receivedAt.toUTC();
    QDateTime decoded(receivedUtc.date(), decodeTime, Qt::UTC);
    const qint64 halfDayMilliseconds = 12LL * 60LL * 60LL * 1000LL;

    if ( decoded.msecsTo(receivedUtc) > halfDayMilliseconds )
        decoded = decoded.addDays(1);
    else if ( receivedUtc.msecsTo(decoded) > halfDayMilliseconds )
        decoded = decoded.addDays(-1);

    return decoded;
}
