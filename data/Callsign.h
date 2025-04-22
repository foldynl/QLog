#ifndef QLOG_DATA_CALLSIGN_H
#define QLOG_DATA_CALLSIGN_H

#include <QObject>
#include <QRegularExpression>

class Callsign : public QObject
{
    Q_OBJECT
public:
    explicit Callsign(const QString &callsign,
                      QObject *parent = nullptr);
    static const QRegularExpression callsignRegEx();
    static const QString callsignRegExString();
    static const QStringList secondarySpecialSuffixes;

    const QString getCallsign() const;
    const QString getHostPrefix() const;
    const QString getHostPrefixWithDelimiter() const;
    const QString getBase() const;
    const QString getBasePrefix() const;
    const QString getBasePrefixNumber() const;
    const QString getSuffix() const;
    const QString getSuffixWithDelimiter() const;
    const QString getWPXPrefix() const;
    bool isValid() const;

private:
    QString fullCallsign;
    QString hostPrefix;
    QString hostPrefixWithDelimiter;
    QString base;
    QString basePrefix;
    QString basePrefixNumber;
    QString suffix;
    QString suffixWithDelimiter;
    bool valid;
};

#endif // QLOG_DATA_CALLSIGN_H
