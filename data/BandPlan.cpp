#include <QSqlQuery>
#include <QSqlError>

#include "BandPlan.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.bandplan");

BandPlan::BandPlanMode BandPlan::freq2BandMode(const double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    // currectly only IARU Region 1 is implemented
    // https://www.iaru-r1.org/wp-content/uploads/2019/08/hf_r1_bandplan.pdf
    // https://www.oevsv.at/export/shared/.content/.galleries/pdf-Downloads/OVSV-Bandplan_05-2019.pdf

    // 2200m
    if (freq >= 0.1357 && freq <= 0.1378) return BAND_MODE_CW;


    // 630m
    else if (freq >= 0.472 && freq <= 0.475) return BAND_MODE_CW;
    else if (freq >  0.475 && freq <= 0.479) return BAND_MODE_DIGITAL;

    // 160m
    else if ( freq >= 1.800 && freq < 2.000 )
    {
        if (freq >= 1.800 && freq < 1.838) return BAND_MODE_CW;
        else if (freq >= 1.838 && freq < 1.840) return BAND_MODE_DIGITAL;
        else if (freq >= 1.840 && freq < 1.842) return BAND_MODE_FT8;
        else if (freq >= 1.842 && freq < 2.000) return BAND_MODE_LSB;
    }
    // 80m
    else if (freq >= 3.500 && freq < 4.000)
    {
        if (freq >= 3.500 && freq < 3.570) return BAND_MODE_CW;
        else if (freq >= 3.570 && freq < 3.573) return BAND_MODE_DIGITAL;
        else if (freq >= 3.573 && freq < 3.575) return BAND_MODE_FT8;
        else if (freq >= 3.575 && freq < 3.600) return BAND_MODE_DIGITAL;
        else if (freq >= 3.600 && freq < 4.000) return BAND_MODE_LSB;
    }
    // 60m
    else if (freq >= 5.3515 && freq <= 5.354) return BAND_MODE_CW;
    else if (freq > 5.354 && freq <= 5.500) return BAND_MODE_LSB;

    // 40m
    else if (freq >= 7.000 && freq < 7.300)
    {
        if (freq >= 7.000 && freq < 7.040) return BAND_MODE_CW;
        else if (freq >= 7.040 && freq < 7.060) return BAND_MODE_DIGITAL;
        else if (freq >= 7.060 && freq < 7.074) return BAND_MODE_LSB;
        else if (freq >= 7.074 && freq < 7.076) return BAND_MODE_FT8;
        else if (freq >= 7.076 && freq < 7.300) return BAND_MODE_LSB;
    }
    // 30m
    else if (freq >= 10.100 && freq < 10.150)
    {
        if (freq >= 10.100 && freq < 10.130) return BAND_MODE_CW;
        else if (freq >= 10.130 && freq < 10.136) return BAND_MODE_DIGITAL;
        else if (freq >= 10.136 && freq < 10.138) return BAND_MODE_FT8;
        else if (freq >= 10.138 && freq < 10.150) return BAND_MODE_DIGITAL;
    }
    // 20m
    else if (freq >= 14.000 && freq < 14.350)
    {
        if (freq >= 14.000 && freq < 14.070) return BAND_MODE_CW;
        else if (freq >= 14.070 && freq < 14.074) return BAND_MODE_DIGITAL;
        else if (freq >= 14.074 && freq < 14.076) return BAND_MODE_FT8;
        else if (freq >= 14.076 && freq < 14.099) return BAND_MODE_DIGITAL;
        else if (freq >= 14.099 && freq < 14.101) return BAND_MODE_CW;
        else if (freq >= 14.101 && freq < 14.350) return BAND_MODE_USB;
    }
    // 17m
    else if (freq >= 18.068 && freq < 18.168)
    {
        if (freq >= 18.068 && freq < 18.095) return BAND_MODE_CW;
        else if (freq >= 18.095 && freq < 18.100) return BAND_MODE_DIGITAL;
        else if (freq >= 18.100 && freq < 18.102) return BAND_MODE_FT8;
        else if (freq >= 18.102 && freq < 18.109) return BAND_MODE_DIGITAL;
        else if (freq >= 18.109 && freq < 18.111) return BAND_MODE_CW;
        else if (freq >= 18.111 && freq < 18.168) return BAND_MODE_USB;
    }
    // 15m
    else if (freq >= 21.000 && freq < 21.450)
    {
        if (freq >= 21.000 && freq < 21.070) return BAND_MODE_CW;
        else if (freq >= 21.070 && freq < 21.074) return BAND_MODE_DIGITAL;
        else if (freq >= 21.074 && freq < 21.076) return BAND_MODE_FT8;
        else if (freq >= 21.076 && freq < 21.149) return BAND_MODE_DIGITAL;
        else if (freq >= 21.149 && freq < 21.151) return BAND_MODE_CW;
        else if (freq >= 21.151 && freq < 21.450) return BAND_MODE_USB;
    }
    // 12m
    else if (freq >= 24.890 && freq < 24.990)
    {
        if (freq >= 24.890 && freq < 24.915) return BAND_MODE_CW;
        else if (freq >= 24.915 && freq < 24.917) return BAND_MODE_FT8;
        else if (freq >= 24.917 && freq < 24.929) return BAND_MODE_DIGITAL;
        else if (freq >= 24.929 && freq < 24.931) return BAND_MODE_CW;
        else if (freq >= 24.931 && freq < 24.990) return BAND_MODE_USB;
    }
    // 10m
    else if (freq >= 28.000 && freq < 29.700)
    {
        if (freq >= 28.000 && freq < 28.070) return BAND_MODE_CW;
        else if (freq >= 28.070 && freq < 28.074) return BAND_MODE_DIGITAL;
        else if (freq >= 28.074 && freq < 28.076) return BAND_MODE_FT8;
        else if (freq >= 28.076 && freq < 28.190) return BAND_MODE_DIGITAL;
        else if (freq >= 28.190 && freq < 28.225) return BAND_MODE_CW;
        else if (freq >= 28.225 && freq < 29.700) return BAND_MODE_USB;
    }
    // 6m
    else if (freq >= 50.000 && freq < 54.000)
    {
        if (freq >= 50.000 && freq < 50.100) return BAND_MODE_CW;
        else if (freq >= 50.100 && freq < 50.313) return BAND_MODE_USB;
        else if (freq >= 50.313 && freq < 50.315) return BAND_MODE_FT8;
        else if (freq >= 50.315 && freq < 50.400) return BAND_MODE_DIGITAL;
        else if (freq >= 50.400 && freq < 50.500) return BAND_MODE_CW;
        else if (freq >= 50.500 && freq < 54.000) return BAND_MODE_PHONE;
    }
    // 4m
    else if (freq >=70.000 && freq < 70.500)
    {
        if (freq >=70.000 && freq < 70.100) return BAND_MODE_CW;
        else if (freq >= 70.100 && freq < 70.102) return BAND_MODE_FT8;
        else if (freq >= 70.102 && freq < 70.250) return BAND_MODE_USB;
        else if (freq >=70.2500 && freq < 70.500) return BAND_MODE_USB;
    }
    // 2m
    else if (freq >= 144.000 && freq < 148.000)
    {
        if (freq >= 144.000 && freq < 144.100) return BAND_MODE_CW;
        else if (freq >= 144.100 && freq <  144.174) return BAND_MODE_USB;
        else if (freq >= 144.174 && freq <= 144.176) return BAND_MODE_FT8;
        else if (freq >  144.176 && freq <  144.360) return BAND_MODE_USB;
        else if (freq >= 144.360 && freq <  144.400) return BAND_MODE_DIGITAL;
        else if (freq >  144.400 && freq <  144.491) return BAND_MODE_CW;
        else if (freq >= 144.491 && freq <  144.975) return BAND_MODE_DIGITAL;
        else if (freq >= 144.975 && freq <  148.000) return BAND_MODE_USB;
    }
    // 1.25m
    else if (freq >= 222.0 && freq < 222.150) return BAND_MODE_CW;
    else if (freq >= 222.150 && freq < 225.00) return BAND_MODE_USB;

    // 70cm
    else if (freq >= 430.0 && freq < 432.0) return BAND_MODE_USB;
    else if (freq >= 432.0 && freq < 432.065) return BAND_MODE_CW;
    else if (freq >= 432.065 && freq < 432.067) return BAND_MODE_FT8;
    else if (freq >= 432.067 && freq < 432.1) return BAND_MODE_CW;
    else if (freq >= 432.1 && freq < 440.0) return BAND_MODE_USB;

    // 33cm
    else if (freq >= 902.0 && freq < 928.0) return BAND_MODE_USB;

    // 23cm
    else if (freq >= 1240.0 && freq < 1296.15) return BAND_MODE_USB;
    else if (freq >= 1296.15 && freq < 1296.4) return BAND_MODE_CW;
    else if (freq >= 1296.4 && freq < 1300.0) return BAND_MODE_PHONE;

    // 13cm QO100
    else if (freq >= 2400.005 && freq < 2400.040) return BAND_MODE_CW;
    else if (freq >= 2400.040 && freq < 2400.080) return BAND_MODE_FT8;
    else if (freq >= 2400.150 && freq < 2400.350) return BAND_MODE_USB;
    else if (freq >= 2400.350 && freq < 2400.490) return BAND_MODE_PHONE;

    // 3cm QO100
    else if (freq >= 10489.505 && freq < 10489.540) return BAND_MODE_CW;
    else if (freq >= 10489.540 && freq < 10489.580) return BAND_MODE_FT8;
    else if (freq >= 10489.650 && freq < 10489.850) return BAND_MODE_USB;
    else if (freq >= 10489.850 && freq < 10489.990) return BAND_MODE_PHONE;

    return BAND_MODE_PHONE;
}

const QString BandPlan::bandMode2BandModeGroupString(const BandPlanMode &bandPlanMode)
{
    FCT_IDENTIFICATION;

    switch ( bandPlanMode )
    {
    case BAND_MODE_CW: return BandPlan::MODE_GROUP_STRING_CW;

    case BAND_MODE_DIGITAL: return BandPlan::MODE_GROUP_STRING_DIGITAL;

    case BAND_MODE_FT8: return BandPlan::MODE_GROUP_STRING_FT8;

    case BAND_MODE_PHONE:
    case BAND_MODE_LSB:
    case BAND_MODE_USB:
        return BandPlan::MODE_GROUP_STRING_PHONE;

    case BAND_MODE_UNKNOWN:
        return QString();
    }
    return QString();
}

const QString BandPlan::freq2BandModeGroupString(const double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    return bandMode2BandModeGroupString(freq2BandMode(freq));
}

const QString BandPlan::bandPlanMode2ExpectedMode(const BandPlanMode &bandPlanMode,
                                                  QString &submode)
{
    FCT_IDENTIFICATION;

    submode = QString();

    switch ( bandPlanMode )
    {
    case BAND_MODE_CW: {submode = QString(); return "CW";}
    case BAND_MODE_LSB: {submode = "LSB"; return "SSB";}
    case BAND_MODE_USB: {submode = "USB"; return "SSB";}
    case BAND_MODE_FT8: {return "FT8";}
    case BAND_MODE_DIGITAL: {submode = "USB"; return "SSB";} // imprecise, but let's try this
    //case BAND_MODE_PHONE: // it can be FM, SSB, AM - no Mode Change
    default:
        submode = QString();
    }

    return QString();
}

const QString BandPlan::freq2ExpectedMode(const double freq, QString &submode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    return bandPlanMode2ExpectedMode(freq2BandMode(freq), submode);
}

const Band BandPlan::freq2Band(double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    QSqlQuery query;

    if ( ! query.prepare("SELECT name, start_freq, end_freq "
                         "FROM bands "
                         "WHERE :freq BETWEEN start_freq AND end_freq") )
    {
        qWarning() << "Cannot prepare Select statement";
        return Band();
    }

    query.bindValue(0, freq);

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute select statement" << query.lastError();
        return Band();
    }

    if ( query.next() )
    {
        Band band;
        band.name = query.value(0).toString();
        band.start = query.value(1).toDouble();
        band.end = query.value(2).toDouble();
        return band;
    }

    return Band();
}

const QList<Band> BandPlan::bandsList(const bool onlyDXCCBands,
                                      const bool onlyEnabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << onlyDXCCBands << onlyEnabled;

    QSqlQuery query;
    QList<Band> ret;

    QString stmt = QString("SELECT name, start_freq, end_freq "
                           "FROM bands WHERE 1 = 1 ");

    if ( onlyEnabled )
    {
        stmt.append("AND enabled = 1 ");
    }

    if ( onlyDXCCBands )
    {
        stmt.append("AND ((1.9 between start_freq and end_freq) "
                    "      OR (3.6 between start_freq and end_freq) "
                    "      OR (7.1 between start_freq and end_freq) "
                    "      OR (10.1 between start_freq and end_freq) "
                    "      OR (14.1 between start_freq and end_freq) "
                    "      OR (18.1 between start_freq and end_freq) "
                    "      OR (21.1 between start_freq and end_freq) "
                    "      OR (24.9 between start_freq and end_freq) "
                    "      OR (28.1 between start_freq and end_freq) "
                    "      OR (50.1 between start_freq and end_freq) "
                    "      OR (145.1 between start_freq and end_freq) "
                    "      OR (421.1 between start_freq and end_freq) "
                    "      OR (1241.0 between start_freq and end_freq) "
                    "      OR (2301.0 between start_freq and end_freq) "
                    "      OR (10001.0 between start_freq and end_freq)) ");
    }

    stmt.append("ORDER BY start_freq ");

    qCDebug(runtime) << stmt;

    if ( ! query.prepare(stmt) )
    {
        qWarning() << "Cannot prepare Select statement";
        return ret;
    }

    if ( ! query.exec() )
    {
        qWarning() << "Cannot execute select statement" << query.lastError();
        return ret;
    }

    while ( query.next() )
    {
        Band band;
        band.name = query.value(0).toString();
        band.start = query.value(1).toDouble();
        band.end = query.value(2).toDouble();
        ret << band;
    }

    return ret;
}

const QString BandPlan::modeToDXCCModeGroup(const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;

    QSqlQuery query;

    if ( !query.prepare("SELECT modes.dxcc "
                        "FROM modes "
                        "WHERE modes.name = :mode LIMIT 1"))
    {
        qWarning() << "Cannot prepare Select statement";
        return QString();
    }

    query.bindValue(0, mode);

    if ( query.exec() )
    {
        QString ret;
        query.next();
        ret = query.value(0).toString();
        return ret;
    }

    return QString();
}

const QString BandPlan::modeToModeGroup(const QString &mode)
{
    FCT_IDENTIFICATION;

    return ( mode == "FT8" ) ? BandPlan::MODE_GROUP_STRING_FT8
                             : BandPlan::modeToDXCCModeGroup(mode);
}

BandPlan::BandPlan()
{
    FCT_IDENTIFICATION;
}

const QString BandPlan::MODE_GROUP_STRING_CW = "CW";
const QString BandPlan::MODE_GROUP_STRING_DIGITAL = "DIGITAL";
const QString BandPlan::MODE_GROUP_STRING_FT8 = "FT8";
const QString BandPlan::MODE_GROUP_STRING_PHONE = "PHONE";
