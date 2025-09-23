#ifndef QLOG_DATA_DATA_H
#define QLOG_DATA_DATA_H

#include <QtCore>
#include <QSqlQuery>
#include "Dxcc.h"
#include "SOTAEntity.h"
#include "WWFFEntity.h"
#include "POTAEntity.h"
#include "core/zonedetect.h"
#include "core/QuadKeyCache.h"

class Data : public QObject
{
    Q_OBJECT
public:

    enum DupeType
    {
        ALL_BANDS = 1,
        EACH_BAND = 2,
        EACH_BAND_MODE = 3,
        NO_CHECK = 4
    };

    enum SeqType
    {
        SINGLE = 1,
        PER_BAND = 2
    };

    const QMap<QString, QString> qslSentEnum = {
        {"Y", tr("Yes")},
        {"N", tr("No")},
        {"R", tr("Requested")},
        {"Q", tr("Queued")},
        {"I", tr("Invalid")}
    };
    const QMap<QString, QString> qslSentViaEnum = {
        {"B", tr("Bureau")},
        {"D", tr("Direct")},
        {"E", tr("Electronic")},
        {" ", tr("Blank")}
    };
    const QMap<QString, QString> qslRcvdEnum = {
        {"Y", tr("Yes")},
        {"N", tr("No")},
        {"R", tr("Requested")},
        {"I", tr("Invalid")}
    };
    const QMap<QString, QString> uploadStatusEnum = {
        {"Y", tr("Yes")},
        {"N", tr("No")},
        {"M", tr("Modified")},
        {" ", tr("Blank")}
    };
    const QMap<QString, QString> antPathEnum = {
        {"G", tr("Grayline")},
        {"O", tr("Other")},
        {"S", tr("Short Path")},
        {"L", tr("Long Path")},
        {" ", tr("Blank")}
    };
    const QMap<QString, QString> boolEnum = {
        {"Y", tr("Yes")},
        {"N", tr("No")},
        {" ", tr("Blank")}
    };
    const QMap<QString, QString> qsoCompleteEnum = {
        {"Y", tr("Yes")},
        {"N", tr("No")},
        {"Nil", tr("Not Heard")},
        {"?", tr("Uncertain")},
        {" ", tr("Blank")}
    };
    const QMap<QString, QString> morseKeyTypeEnum = {
        {"SK", tr("Straight Key")},
        {"SS", tr("Sideswiper")},
        {"BUG", tr("Mechanical semi-automatic keyer or Bug")},
        {"FAB", tr("Mechanical fully-automatic keyer or Bug")},
        {"SP", tr("Single Paddle")},
        {"DP", tr("Dual Paddle")},
        {"CPU", tr("Computer Driven")},
        {" ", tr("Blank")}
        };
    const QMap<QString, QString> downloadStatusEnum = {
        {"Y", tr("Yes")},
        {"N", tr("No")},
        {"I", tr("Invalid")},
        {" ", tr("Blank")}
    };

    explicit Data(QObject *parent = nullptr);
    ~Data();
    static Data* instance()
    {
        static Data instance;
        return &instance;
    };

    static DxccStatus dxccNewStatusWhenQSOAdded(const DxccStatus &oldStatus,
                                       const qint32 oldDxcc,
                                       const QString &oldBand,
                                       const QString &oldMode,
                                       const qint32 newDxcc,
                                       const QString &newBand,
                                       const QString &newMode);
    static qulonglong dupeNewCountWhenQSOAdded(qulonglong oldCounter,
                                               const QString &oldBand,
                                               const QString &oldMode,
                                               const QString &addedBand,
                                               const QString &addedMode);
    static qulonglong dupeNewCountWhenQSODelected(qulonglong oldCounter,
                                                  const QString &oldBand,
                                                  const QString &oldMode,
                                                  const QString &deletedBand,
                                                  const QString &deletedMode);

    static QColor statusToColor(const DxccStatus &status, bool isDupe, const QColor &defaultColor);
    static QString colorToHTMLColor(const QColor&);
    static QString statusToText(const DxccStatus &status);
    static QString removeAccents(const QString &input);
    static int getITUZMin();
    static int getITUZMax();
    static int getCQZMin();
    static int getCQZMax();
    static QString dbFilename();
    static QString debugFilename();
    static double MHz2UserFriendlyFreq(double,
                                       QString &unit,
                                       unsigned char &efectiveDecP);
    static const QStringList& getContinentList();

    static qulonglong countDupe(const QString& callsign,
                                const QString &band,
                                const QString &mode);

    static QString safeQueryString(const QUrlQuery &query);
    DxccStatus dxccStatus(int dxcc, const QString &band, const QString &mode);
    QStringList contestList();
    QStringList propagationModesList() const { return QStringList{""} + propagationModes.values(); }
    QStringList propagationModesIDList() const { return QStringList{""} + propagationModes.keys(); }
    QString propagationModeTextToID(const QString &propagationText) const { return propagationModes.key(propagationText);}
    QString propagationModeIDToText(const QString &propagationID) const { return propagationModes.value(propagationID);}
    DxccEntity lookupDxcc(const QString &callsign);
    DxccEntity lookupDxccID(const int dxccID);
    SOTAEntity lookupSOTA(const QString &SOTACode);
    POTAEntity lookupPOTA(const QString &POTACode);
    WWFFEntity lookupWWFF(const QString &reference);
    const QString dxccFlag(int dxcc) const {return flags.value(dxcc);} ;
    QPair<QString, QString> legacyMode(const QString &mode);
    QStringList satModeList() { return satModes.values();}
    QStringList satModesIDList() { return satModes.keys(); }
    QString satModeTextToID(const QString &satModeText) { return satModes.key(satModeText);}
    QString satModeIDToText(const QString &satModeID) { return satModes.value(satModeID);}
    QStringList iotaList() { return iotaRef.values();}
    QStringList iotaIDList() { return iotaRef.keys();}
    QString iotaTextToID(const QString &iotaText) { return iotaRef.key(iotaText);}
    QStringList sotaIDList() { return sotaRefID.keys();}
    QStringList wwffIDList() { return wwffRefID.keys();}
    QStringList potaIDList() { return potaRefID.keys();}
    QString getIANATimeZone(double, double);
    QStringList sigIDList();

signals:

public slots:
    void invalidateDXCCStatusCache(const QSqlRecord &record);
    void invalidateSetOfDXCCStatusCache(const QSet<uint> &entities);
    void clearDXCCStatusCache();

private:
    void loadContests();
    void loadPropagationModes();
    void loadLegacyModes();
    void loadDxccFlags();
    void loadSatModes();
    void loadIOTA();
    void loadSOTA();
    void loadWWFF();
    void loadPOTA();
    void loadTZ();

    QHash<int, QString> flags;
    QMap<QString, QString> contests;
    QMap<QString, QString> propagationModes;
    QMap<QString, QPair<QString, QString>> legacyModes;
    QMap<QString, QString> satModes;
    QMap<QString, QString> iotaRef;
    QMap<QString, QString> sotaRefID;
    QMap<QString, QString> wwffRefID;
    QMap<QString, QString> potaRefID;
    ZoneDetect * zd;
    QSqlQuery queryDXCC;
    QSqlQuery queryDXCCID;
    QSqlQuery querySOTA;
    QSqlQuery queryWWFF;
    QSqlQuery queryPOTA;
    bool isDXCCQueryValid;
    bool isSOTAQueryValid;
    bool isWWFFQueryValid;
    bool isPOTAQueryValid;
    bool isDXCCIDQueryValid;
    QuadKeyCache<DxccStatus> dxccStatusCache;

    static const char translitTab[];
    static const int tranlitIndexMap[];
};

#endif // QLOG_DATA_DATA_H
