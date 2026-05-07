#include <QSqlQuery>
#include <QSqlError>

#include "BandPlan.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.data.bandplan");

struct BandModeRange {
    double start;
    double end;
    BandPlan::BandPlanMode mode;
};

// currectly only IARU Region 1 is implemented
// https://www.iaru-r1.org/wp-content/uploads/2019/08/hf_r1_bandplan.pdf
// https://www.oevsv.at/export/shared/.content/.galleries/pdf-Downloads/OVSV-Bandplan_05-2019.pdf
static const BandModeRange r1BandModeTable[] =
{
    // 2200m
    {0.1357, 0.1378, BandPlan::BAND_MODE_CW},

    // 630m
    {0.472, 0.475, BandPlan::BAND_MODE_CW},
    {0.475, 0.479, BandPlan::BAND_MODE_DIGITAL},

    // 160m
    {1.800, 1.838, BandPlan::BAND_MODE_CW},
    {1.838, 1.840, BandPlan::BAND_MODE_DIGITAL},
    {1.840, 1.843, BandPlan::BAND_MODE_FT8},
    {1.843, 2.000, BandPlan::BAND_MODE_LSB},

    // 80m
    {3.500, 3.570, BandPlan::BAND_MODE_CW},
    {3.570, 3.573, BandPlan::BAND_MODE_DIGITAL},
    {3.573, 3.576, BandPlan::BAND_MODE_FT8},
    {3.576, 3.579, BandPlan::BAND_MODE_FT4},
    {3.579, 3.600, BandPlan::BAND_MODE_DIGITAL},
    {3.600, 4.000, BandPlan::BAND_MODE_LSB},

    // 60m
    {5.3515, 5.356, BandPlan::BAND_MODE_CW},
    {5.356, 5.361, BandPlan::BAND_MODE_FT8},
    {5.361, 5.500, BandPlan::BAND_MODE_LSB},

    // 40m
    {7.000, 7.040, BandPlan::BAND_MODE_CW},
    {7.040, 7.047, BandPlan::BAND_MODE_DIGITAL},
    {7.047, 7.050, BandPlan::BAND_MODE_FT4},
    {7.050, 7.060, BandPlan::BAND_MODE_DIGITAL},
    {7.060, 7.074, BandPlan::BAND_MODE_LSB},
    {7.074, 7.077, BandPlan::BAND_MODE_FT8},
    {7.077, 7.300, BandPlan::BAND_MODE_LSB},

    // 30m
    {10.100, 10.130, BandPlan::BAND_MODE_CW},
    {10.130, 10.136, BandPlan::BAND_MODE_DIGITAL},
    {10.136, 10.139, BandPlan::BAND_MODE_FT8},
    {10.139, 10.140, BandPlan::BAND_MODE_DIGITAL},
    {10.140, 10.143, BandPlan::BAND_MODE_FT4},
    {10.143, 10.150, BandPlan::BAND_MODE_DIGITAL},

    // 20m
    {14.000, 14.070, BandPlan::BAND_MODE_CW},
    {14.070, 14.074, BandPlan::BAND_MODE_DIGITAL},
    {14.074, 14.080, BandPlan::BAND_MODE_FT8},
    {14.080, 14.083, BandPlan::BAND_MODE_FT4},
    {14.083, 14.099, BandPlan::BAND_MODE_DIGITAL},
    {14.099, 14.101, BandPlan::BAND_MODE_CW},
    {14.101, 14.350, BandPlan::BAND_MODE_USB},

    // 17m
    {18.068, 18.095, BandPlan::BAND_MODE_CW},
    {18.095, 18.100, BandPlan::BAND_MODE_DIGITAL},
    {18.100, 18.103, BandPlan::BAND_MODE_FT8},
    {18.103, 18.106, BandPlan::BAND_MODE_FT4},
    {18.106, 18.109, BandPlan::BAND_MODE_DIGITAL},
    {18.109, 18.111, BandPlan::BAND_MODE_CW},
    {18.111, 18.168, BandPlan::BAND_MODE_USB},

    // 15m
    {21.000, 21.070, BandPlan::BAND_MODE_CW},
    {21.070, 21.074, BandPlan::BAND_MODE_DIGITAL},
    {21.074, 21.077, BandPlan::BAND_MODE_FT8},
    {21.077, 21.140, BandPlan::BAND_MODE_DIGITAL},
    {21.140, 21.143, BandPlan::BAND_MODE_FT4},
    {21.143, 21.149, BandPlan::BAND_MODE_DIGITAL},
    {21.149, 21.151, BandPlan::BAND_MODE_CW},
    {21.151, 21.450, BandPlan::BAND_MODE_USB},

    // 12m
    {24.890, 24.915, BandPlan::BAND_MODE_CW},
    {24.915, 24.919, BandPlan::BAND_MODE_FT8},
    {24.919, 24.922, BandPlan::BAND_MODE_FT4},
    {24.922, 24.929, BandPlan::BAND_MODE_DIGITAL},
    {24.929, 24.931, BandPlan::BAND_MODE_CW},
    {24.931, 24.990, BandPlan::BAND_MODE_USB},

    // 10m
    {28.000, 28.070, BandPlan::BAND_MODE_CW},
    {28.070, 28.074, BandPlan::BAND_MODE_DIGITAL},
    {28.074, 28.077, BandPlan::BAND_MODE_FT8},
    {28.077, 28.180, BandPlan::BAND_MODE_DIGITAL},
    {28.180, 28.183, BandPlan::BAND_MODE_FT4},
    {28.183, 28.190, BandPlan::BAND_MODE_DIGITAL},
    {28.190, 28.225, BandPlan::BAND_MODE_CW},
    {28.225, 29.700, BandPlan::BAND_MODE_USB},

    // 6m
    {50.000, 50.100, BandPlan::BAND_MODE_CW},
    {50.100, 50.313, BandPlan::BAND_MODE_USB},
    {50.313, 50.318, BandPlan::BAND_MODE_FT8},
    {50.318, 50.321, BandPlan::BAND_MODE_FT4},
    {50.321, 50.400, BandPlan::BAND_MODE_DIGITAL},
    {50.400, 50.500, BandPlan::BAND_MODE_CW},
    {50.500, 54.000, BandPlan::BAND_MODE_PHONE},

    // 4m
    {70.000, 70.100, BandPlan::BAND_MODE_CW},
    {70.100, 70.102, BandPlan::BAND_MODE_FT8},
    {70.102, 70.500, BandPlan::BAND_MODE_USB},

    // 2m
    {144.000, 144.100, BandPlan::BAND_MODE_CW},
    {144.100, 144.120, BandPlan::BAND_MODE_USB},
    {144.120, 144.123, BandPlan::BAND_MODE_FT4},
    {144.123, 144.174, BandPlan::BAND_MODE_USB},
    {144.174, 144.176, BandPlan::BAND_MODE_FT8},
    {144.176, 144.360, BandPlan::BAND_MODE_USB},
    {144.360, 144.400, BandPlan::BAND_MODE_DIGITAL},
    {144.400, 144.491, BandPlan::BAND_MODE_CW},
    {144.491, 144.975, BandPlan::BAND_MODE_DIGITAL},
    {144.975, 148.000, BandPlan::BAND_MODE_USB},

    // 1.25m
    {222.000, 222.150, BandPlan::BAND_MODE_CW},
    {222.150, 225.000, BandPlan::BAND_MODE_USB},

    // 70cm
    {430.000, 432.000, BandPlan::BAND_MODE_USB},
    {432.000, 432.065, BandPlan::BAND_MODE_CW},
    {432.065, 432.067, BandPlan::BAND_MODE_FT8},
    {432.067, 432.100, BandPlan::BAND_MODE_CW},
    {432.100, 440.000, BandPlan::BAND_MODE_USB},

    // 33cm
    {902.000, 928.000, BandPlan::BAND_MODE_USB},

    // 23cm
    {1240.000, 1296.150, BandPlan::BAND_MODE_USB},
    {1296.150, 1296.400, BandPlan::BAND_MODE_CW},
    {1296.400, 1300.000, BandPlan::BAND_MODE_PHONE},

    // 3cm QO100
    // at the moment there is no other satellite that would be used, so we can afford it.
    // It will not affect tropo operation, because it is in the lower part of the band.
    {10489.505, 10489.540, BandPlan::BAND_MODE_CW},
    {10489.540, 10489.580, BandPlan::BAND_MODE_FT8},
    {10489.580, 10489.650, BandPlan::BAND_MODE_DIGITAL},
    {10489.650, 10489.745, BandPlan::BAND_MODE_USB},
    {10489.755, 10489.850, BandPlan::BAND_MODE_USB},
    {10489.850, 10489.990, BandPlan::BAND_MODE_PHONE}
};

static int bandModeTableSize = sizeof(r1BandModeTable) / sizeof(r1BandModeTable[0]);

BandPlan::BandPlanMode BandPlan::freq2BandMode(const double freq)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << freq;

    int left = 0;
    int right = bandModeTableSize - 1;

    while ( left <= right )
    {
        int mid = (left + right) / 2;
        const BandModeRange &range = r1BandModeTable[mid];

        if (freq < range.start) right = mid - 1;
        else if (freq >= range.end) left = mid + 1;
        else return range.mode;
    }
    // fallback
    return BandPlan::BAND_MODE_PHONE;
}

const QString BandPlan::bandMode2BandModeGroupString(const BandPlanMode &bandPlanMode)
{
    FCT_IDENTIFICATION;

    switch ( bandPlanMode )
    {
    case BAND_MODE_CW: return BandPlan::MODE_GROUP_STRING_CW;

    case BAND_MODE_DIGITAL: return BandPlan::MODE_GROUP_STRING_DIGITAL;

    case BAND_MODE_FT8:
    case BAND_MODE_FT4:
    case BAND_MODE_FT2:
        return BandPlan::MODE_GROUP_STRING_FTx;

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
    case BAND_MODE_FT4: {submode = "FT4";return "MFSK";}
    case BAND_MODE_FT2: // not currently specified - use USB
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

    if ( ! query.prepare("SELECT name, start_freq, end_freq, sat_designator "
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
        band.satDesignator  = query.value(3).toString();
        return band;
    }

    return Band();
}

const Band BandPlan::bandName2Band(const QString &name)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name;

    QSqlQuery query;

    if ( ! query.prepare("SELECT name, start_freq, end_freq, sat_designator "
                       "FROM bands "
                       "WHERE name = :name LIMIT 1") )
    {
        qWarning() << "Cannot prepare Select statement";
        return Band();
    }

    query.bindValue(0, name.toLower());

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
        band.satDesignator  = query.value(3).toString();
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

    QString stmt(QLatin1String("SELECT name, start_freq, end_freq, sat_designator "
                               "FROM bands WHERE 1 = 1 "));

    if ( onlyEnabled )
        stmt.append("AND enabled = 1 ");

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
        band.satDesignator = query.value(3).toString();
        ret << band;
    }

    return ret;
}

const QString BandPlan::modeToDXCCModeGroup(const QString &mode)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode;

    if ( mode.isEmpty() ) return QString();

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

    return isFTxMode(mode) ? BandPlan::MODE_GROUP_STRING_FTx
                           : BandPlan::modeToDXCCModeGroup(mode);
}

bool BandPlan::isFTxMode(const QString &mode)
{
    return mode == "FT8"
        || mode == "FT4"
        || mode == "FT2";
}

bool BandPlan::isFTxBandMode(BandPlanMode mode)
{
    return mode == BAND_MODE_FT8
        || mode == BAND_MODE_FT4
        || mode == BAND_MODE_FT2;
}

BandPlan::BandPlan()
{
    FCT_IDENTIFICATION;
}

const QString BandPlan::MODE_GROUP_STRING_CW = "CW";
const QString BandPlan::MODE_GROUP_STRING_DIGITAL = "DIGITAL";
const QString BandPlan::MODE_GROUP_STRING_FTx = "FTx";
const QString BandPlan::MODE_GROUP_STRING_PHONE = "PHONE";
