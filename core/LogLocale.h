#ifndef QLOG_CORE_LOGLOCALE_H
#define QLOG_CORE_LOGLOCALE_H

#include <QSettings>
#include <QLocale>
#include <QRegularExpression>

class LogLocale : public QLocale
{
public:
    LogLocale();

    const QString formatTimeLongWithoutTZ() const;
    const QString formatTimeShort() const;
    const QString formatTimeLong() const;
    const QString formatDateShortWithYYYY() const;
    const QString formatDateTimeShortWithYYYY() const;
    bool getSettingUse24hformat() const;
    void setSettingUse24hformat(bool value);

    bool getSettingUseSystemDateFormat() const;
    void setSettingUseSystemDateFormat(bool value);
    const QString getSettingDateFormat() const;
    void setSettingDateFormat(const QString &value);

    bool getSettingUseMetric() const;
    void setSettingUseMetric(bool value);

private:
    const QRegularExpression regexp;
    bool is24hUsed;
    bool isMetricUnitUsed;
    QSettings settings;
    QString systemDateFormat;

    void changeTime12_24Format(QString &formatString) const;
};

#endif // QLOG_CORE_LOGLOCALE_H
