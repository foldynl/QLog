#include "LogLocale.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.loglocale");

LogLocale::LogLocale() :
    regexp(QRegularExpression(R"(, tttt|\(t\)|\(tttt\)|\[tttt\]|\btttt\b|\btt\b|\bt\b)")),
    is24hUsed(!timeFormat(QLocale::ShortFormat).contains("ap", Qt::CaseInsensitive)),
    isMetricUnitUsed(measurementSystem() == QLocale::MetricSystem)
{
    FCT_IDENTIFICATION;

    systemDateFormat = dateFormat(QLocale::ShortFormat);
    qCDebug(runtime) << systemDateFormat;

    if ( systemDateFormat.contains("yy") && !systemDateFormat.contains("yyyy") )
        systemDateFormat.replace("yy", "yyyy");
}

void LogLocale::changeTime12_24Format(QString &formatString) const
{
    if ( getSettingUse24hformat() )
        formatString.remove("ap", Qt::CaseInsensitive).remove("a", Qt::CaseInsensitive);
    else if ( is24hUsed )
    {
        formatString += " AP";
        formatString = formatString.toLower();
    }
}

const QString LogLocale::formatTimeLongWithoutTZ() const
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << timeFormat(QLocale::LongFormat);

    QString ret = timeFormat(QLocale::LongFormat).remove(regexp).trimmed();

    changeTime12_24Format(ret);
    qCDebug(runtime) << "format:" << ret;
    return ret;
}

const QString LogLocale::formatTimeShort() const
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << timeFormat(QLocale::ShortFormat);

    QString ret = timeFormat(QLocale::ShortFormat);

    changeTime12_24Format(ret);
    qCDebug(runtime) << "format:" << ret;
    return ret;
}

const QString LogLocale::formatTimeLong() const
{
    FCT_IDENTIFICATION;

    QString ret = formatTimeLongWithoutTZ();
    ret
#if (QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
                                           .append(" t");
#else
                                           .append(" ttt");
#endif

    qCDebug(runtime) << "format:" << ret;
    return ret;
}

const QString LogLocale::formatDateShortWithYYYY() const
{
    FCT_IDENTIFICATION;

    QString ret = (getSettingUseSystemDateFormat()) ? systemDateFormat : getSettingDateFormat();
    qCDebug(runtime) << "format:" << ret;
    return ret;
}

const QString LogLocale::formatDateTimeShortWithYYYY() const
{
    FCT_IDENTIFICATION;

    QString ret = formatDateShortWithYYYY() + " " + formatTimeShort();

    qCDebug(runtime) << "format:" << ret;
    return ret;
}

bool LogLocale::getSettingUse24hformat() const
{
    return settings.value("use24hformat", is24hUsed).toBool();
}

void LogLocale::setSettingUse24hformat(bool value)
{
    settings.setValue("use24hformat", value);
}

bool LogLocale::getSettingUseMetric() const
{
    return settings.value("usemetricformat", isMetricUnitUsed).toBool();
}

void LogLocale::setSettingUseMetric(bool value)
{
    settings.setValue("usemetricformat", value);
}

bool LogLocale::getSettingUseSystemDateFormat() const
{
    return settings.value("usesystemdateformat", true).toBool();
}

void LogLocale::setSettingUseSystemDateFormat(bool value)
{
    settings.setValue("usesystemdateformat", value);
}

const QString LogLocale::getSettingDateFormat() const
{
    return settings.value("customdateformatstring", systemDateFormat).toString();
}

void LogLocale::setSettingDateFormat(const QString &value)
{
    settings.setValue("customdateformatstring", value);
}

