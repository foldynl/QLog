#ifndef QLOG_LOGFORMAT_CABRILLOFORMAT_H
#define QLOG_LOGFORMAT_CABRILLOFORMAT_H

#include "LogFormat.h"
#include <QList>
#include <QMap>

class CabrilloFormat : public LogFormat
{
public:
    explicit CabrilloFormat(QTextStream &stream);

    struct HeaderData
    {
        QString callsign;
        QString operatorName;
        QString email;
        QString operators;
        QString address;
        QString gridLocator;
        QString categoryOperator;
        QString categoryBand;
        QString categoryPower;
        QString categoryMode;
        QString categoryAssisted;
        QString categoryStation;
        QString categoryTransmitter;
        QString categoryTime;
        QString categoryOverlay;
        QString location;
        QString club;
        QString offtime;
        QString soapbox;
    };

    void setTemplateId(int id);
    void setHeaderData(const HeaderData &data);
    void setTransmitterId(int id);

    void exportStart() override;
    void exportContact(const QSqlRecord &record,
                       QMap<QString, QString> *appliedMap = nullptr) override;
    void exportEnd() override;

    struct TemplateInfo
    {
        int id;
        QString name;
        QString contestName;
        QString defaultCategoryMode;
    };

    struct ColumnDef
    {
        int position;
        QString dbField;
        int width;
        QString formatter;
        QString label;
    };

    struct CategoryItem
    {
        QString value;
        QString label;
    };

    static QList<TemplateInfo> templateList();
    static TemplateInfo templateInfo(int templateId);
    static QList<ColumnDef> loadTemplateColumns(int templateId);
    static QString formatField(const QString &value,
                               const QString &formatter,
                               int width);

    static QList<CategoryItem> bandCategories();
    static QList<CategoryItem> modeCategories();
    static QList<CategoryItem> powerCategories();
    static QList<CategoryItem> operatorCategories();
    static QList<CategoryItem> assistedCategories();
    static QList<CategoryItem> stationCategories();
    static QList<CategoryItem> transmitterCategories();
    static QList<CategoryItem> timeCategories();
    static QList<CategoryItem> overlayCategories();
    static QList<CategoryItem> formatterTypes();

    // Formatter type identifiers
    static const QString FMT_NONE;
    static const QString FMT_FREQ_KHZ;
    static const QString FMT_TIME_HHMM;
    static const QString FMT_DATE_YYYY_MM_DD;
    static const QString FMT_RST_SHORT;
    static const QString FMT_UPPER;
    static const QString FMT_MODE_CABRILLO;
    static const QString FMT_TRANSMITTER_ID;
    static const QString FMT_PADDED_NR;

    // Band category values
    static const QString BAND_ALL;
    static const QString BAND_160M;
    static const QString BAND_80M;
    static const QString BAND_40M;
    static const QString BAND_20M;
    static const QString BAND_15M;
    static const QString BAND_10M;
    static const QString BAND_6M;
    static const QString BAND_4M;
    static const QString BAND_2M;
    static const QString BAND_222;
    static const QString BAND_432;
    static const QString BAND_902;
    static const QString BAND_1_2G;
    static const QString BAND_LIGHT;
    static const QString BAND_VHF_3_BAND;
    static const QString BAND_VHF_FM_ONLY;

    // Mode category values
    static const QString MODE_CW;
    static const QString MODE_SSB;
    static const QString MODE_RTTY;
    static const QString MODE_FM;
    static const QString MODE_DIGI;
    static const QString MODE_MIXED;

    // Power category values
    static const QString POWER_HIGH;
    static const QString POWER_LOW;
    static const QString POWER_QRP;

    // Operator category values
    static const QString OPERATOR_SINGLE;
    static const QString OPERATOR_MULTI;
    static const QString OPERATOR_CHECKLOG;

    // Assisted category values
    static const QString ASSISTED_NO;
    static const QString ASSISTED_YES;

    // Station category values
    static const QString STATION_FIXED;
    static const QString STATION_MOBILE;
    static const QString STATION_PORTABLE;
    static const QString STATION_ROVER;
    static const QString STATION_ROVER_LIMITED;
    static const QString STATION_ROVER_UNLIMITED;
    static const QString STATION_EXPEDITION;
    static const QString STATION_HQ;
    static const QString STATION_SCHOOL;
    static const QString STATION_DISTRIBUTED;

    // Transmitter category values
    static const QString TRANSMITTER_ONE;
    static const QString TRANSMITTER_TWO;
    static const QString TRANSMITTER_LIMITED;
    static const QString TRANSMITTER_UNLIMITED;
    static const QString TRANSMITTER_SWL;

    // Time category values
    static const QString TIME_6_HOURS;
    static const QString TIME_12_HOURS;
    static const QString TIME_24_HOURS;

    // Overlay category values
    static const QString OVERLAY_CLASSIC;
    static const QString OVERLAY_ROOKIE;
    static const QString OVERLAY_TB_WIRES;
    static const QString OVERLAY_NOVICE_TECH;
    static const QString OVERLAY_OVER_50;

    const QString FORMAT_VERSION_STRING = "3.0";
    const QString PROGRAMID_STRING = "QLog";

private:
    static const int MAX_LINE_LENGTH = 75;
    static const int MAX_ADDRESS_LINES = 6;

    void loadTemplate();
    void writeMultiLineField(const QString &key, const QString &value,
                             int maxLines = 0);

    QMap<QString, QString> buildHeaderFields() const;

    int templateId;
    int transmitterId;
    HeaderData headerData;
    QList<ColumnDef> columns;
    QString contestName;
    bool multiOpEnabled;

    const QStringList headerOrder = {
        "CALLSIGN", "NAME", "EMAIL", "OPERATORS", "ADDRESS",
        "GRID-LOCATOR",
        "CATEGORY-OPERATOR", "CATEGORY-BAND", "CATEGORY-POWER",
        "CATEGORY-MODE", "CATEGORY-ASSISTED",
        "CATEGORY-STATION", "CATEGORY-TRANSMITTER",
        "CATEGORY-TIME", "CATEGORY-OVERLAY",
        "CLUB", "OFFTIME", "SOAPBOX"
    };
};

#endif // QLOG_LOGFORMAT_CABRILLOFORMAT_H
