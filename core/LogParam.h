#ifndef QLOG_CORE_LOGPARAM_H
#define QLOG_CORE_LOGPARAM_H

#include <QObject>
#include <QDate>
#include <QVariant>
#include <QMutex>

class LogParam : public QObject
{
    Q_OBJECT
public:
    explicit LogParam(QObject *parent = nullptr);

    /*********
     * LOV
     ********/
    static bool setLOVParam(const QString &LOVName, const QVariant &value);
    static QDate getLOVaParam(const QString &LOVName);

    /*********
     * Backup
     ********/
    static bool setLastBackupDate(const QDate date);
    static QDate getLastBackupDate();

    /*********
     * LogID
     ********/
    static bool setLogID(const QString &id);
    static QString getLogID();

    /*********
     * Contest
     ********/
    static bool setContestSeqnoType(const QVariant &data);
    static int getContestSeqnoType();
    static bool setContestManuDupeType(const QVariant &data);
    static int getContestDupeType();
    static bool setContestLinkExchange(const QVariant &data);
    static int getContestLinkExchange();
    static bool setContestFilter(const QString &filterName);
    static QString getContestFilter();
    static bool setContestID(const QString &contestID);
    static QString getContestID();
    static bool setContestDupeDate(const QDateTime date);
    static QDateTime getContestDupeDate();
    static void removeConetstDupeDate();

    /********************
     * DXCC Confirmation
     ********************/
    static bool getDxccConfirmedByLotwState();
    static bool setDxccConfirmedByLotwState(bool state);
    static bool setDxccConfirmedByPaperState(bool state);
    static bool getDxccConfirmedByPaperState();
    static bool setDxccConfirmedByEqslState(bool state);
    static bool getDxccConfirmedByEqslState();
    static int getContestSeqno(const QString &band = QString());
    static bool setContestSeqno(int value, const QString &band = QString());
    static void removeContestSeqno();

    /*********
     * Bandmap
     *********/
    static QStringList bandmapsWidgets();
    static void removeBandmapWidgetGroup(const QString &group);
    static double getBandmapScrollFreq(const QString& widgetID, const QString &bandName);
    static bool setBandmapScrollFreq(const QString& widgetID, const QString &bandName, double scroll);
    static QVariant getBandmapZoom(const QString& widgetID, const QString &bandName, const QVariant &defaultValue);
    static bool setBandmapZoom(const QString& widgetID, const QString &bandName, const QVariant &zoom);
    static bool setBandmapAging(const QString& widgetID, int aging);
    static int getBandmapAging(const QString& widgetID);
    static bool setBandmapCenterRX(const QString& widgetID, bool centerRX);
    static bool getBandmapCenterRX(const QString& widgetID);
    static bool setBandmapShowEmergency(const QString& widgetID, bool show);
    static bool getBandmapShowEmergency(const QString& widgetID);

    /*******************
     * UploadQSO Dialog
     *******************/
    static QString getUploadQSOLastCall();
    static void setUploadQSOLastCall(const QString &call);
    static bool getUploadeqslQSLComment();
    static void setUploadeqslQSLComment(const bool state);
    static bool getUploadeqslQSLMessage();
    static QString getUploadeqslQTHProfile();
    static void setUploadeqslQTHProfile(const QString &qthProfile);
    static void setUploadeqslQSLMessage(const bool state);
    static bool getUploadServiceState(const QString& name);
    static void setUploadServiceState(const QString& name, bool state);
    static int getUploadQSOFilterType();
    static void setUploadQSOFilterType(int filterID);
    static QString getUploadLoTWLocation();
    static void setUploadLoTWLocation(const QString &location);

    /*********************
     * DownloadQSL Dialog
     *********************/
    static bool getDownloadQSLServiceState(const QString& name);
    static void setDownloadQSLServiceState(const QString& name, bool state);
    static QDate getDownloadQSLServiceLastDate(const QString& name);
    static void setDownloadQSLServiceLastDate(const QString& name, const QDate &date);
    static bool getDownloadQSLServiceLastQSOQSL(const QString& name);
    static void setDownloadQSLServiceLastQSOQSL(const QString& name, bool state);
    static QString getDownloadQSLLoTWLastCall();
    static void setDownloadQSLLoTWLastCall(const QString &call);
    static QString getDownloadQSLeQSLLastProfile();
    static void setDownloadQSLeQSLLastProfile(const QString &profile);

    /*********
     * QRZ
     ********/
    static QString getQRZCOMCallbookUsername();
    static void setQRZCOMCallbookUsername(const QString& username);
    static QStringList getQRZCOMAPICallsignsList();
    static void setQRZCOMAPICallsignsList(const QStringList &list);

    /**********
     * Cloudlog
     **********/
    static QString getCloudlogAPIEndpoint();
    static void setCloudlogAPIEndpoint(const QString &endpoint);
    static uint getCloudlogStationID();
    static void setCloudlogStationID(uint stationID);

    /*********
     * Clublog
     ********/
    static QString getClublogLogbookReqEmail();
    static void setClublogLogbookReqEmail(const QString& email);
    static bool getClublogUploadImmediatelyEnabled();
    static void setClublogUploadImmediatelyEnabled(bool state);

    /*********
     * eQSL
     ********/
    static QString getEQSLLogbookUsername();
    static void setEQSLLogbookUsername(const QString& username);

    /*********
     * HamQTH
     ********/
    static QString getHamQTHCallbookUsername();
    static void setHamQTHCallbookUsername(const QString& username);

    /*********
     * HRDLog
     ********/
    static QString getHRDLogLogbookReqCallsign();
    static void setHRDLogLogbookReqCallsign(const QString& callsign);
    static bool getHRDLogOnAir();
    static void setHRDLogOnAir(bool state);

    /*********
     * KSTChat
     ********/
    static QString getKSTChatUsername();
    static void setKSTChatUsername(const QString& username);

    /*********
     * LoTW
     ********/
    static QString getLoTWCallbookUsername();
    static void setLoTWCallbookUsername(const QString& username);
    static QString getLoTWTQSLPath(const QString &defaultPath);
    static void setLoTWTQSLPath(const QString& path);
    static bool isLoTWTQSLPathKey(const QString &key);

    /*******************
     * Callbook setting
     *******************/
    static QString getPrimaryCallbook(const QString &defaultValue);
    static void setPrimaryCallbook(const QString& callbookName);
    static QString getSecondaryCallbook(const QString &defaultValue);
    static void setSecondaryCallbook(const QString& callbookName);
    static QString getCallbookWebLookupURL(const QString &defaultURL);
    static void setCallbookWebLookupURL(const QString& url);

    /************************
     * Network Notifications
     ************************/
    static QString getNetworkNotifLogQSOAddrs();
    static void setNetworkNotifLogQSOAddrs(const QString &addrs);
    static QString getNetworkNotifDXCSpotAddrs();
    static void setNetworkNotifDXCSpotAddrs(const QString &addrs);
    static QString getNetworkNotifWSJTXCQSpotAddrs();
    static void setNetworkNotifWSJTXCQSpotAddrs(const QString &addrs);
    static QString getNetworkNotifAlertsSpotAddrs();
    static void setNetworkNotifAlertsSpotAddrs(const QString &addrs);
    static QString getNetworkNotifRigStateAddrs();
    static void setNetworkNotifRigStateAddrs(const QString &addrs);
    static int getNetworkWsjtxListenerPort(int defaultPort);
    static void setNetworkNotifRigStateAddrs(int port);
    static QString getNetworkWsjtxForwardAddrs();
    static void setNetworkWsjtxForwardAddrs(const QString &addrs);
    static bool getNetworkWsjtxListenerJoinMulticast();
    static void setNetworkWsjtxListenerJoinMulticast(bool state);
    static QString getNetworkWsjtxListenerMulticastAddr();
    static void setNetworkWsjtxListenerMulticastAddr(const QString &addr);
    static int getNetworkWsjtxListenerMulticastTTL();
    static void setNetworkWsjtxListenerMulticastTTL(int ttl);

    /********************
     * Club Member Lists
     ********************/
    static QStringList getEnabledMemberlists();
    static void setEnabledMemberlists(const QStringList &list);
    static int getAlertAging();
    static void setAlertAging(int aging);
    static QByteArray getAlertWidgetState();

    /***************
     * Alert Dialog
     ***************/
    static void setAlertWidgetState(const QByteArray &state);

    /*************
     * CW Console
     *************/
    static bool getCWConsoleSendWord();
    static void setCWConsoleSendWord(bool state);

    /*********
     * Chat
     ********/
    static bool getChatSelectedRoom();
    static void setChatSelectedRoom(int room);

    /*************
     * NewContact
     *************/
    static double getNewContactFreq();
    static void setNewContactFreq(double freq);
    static QString getNewContactMode();
    static void setNewContactMode(const QString &mode);
    static QString getNewContactSubMode();
    static void setNewContactSubMode(const QString &submode);
    static double getNewContactPower();
    static void setNewContactPower(double power);
    static int getNewContactTabIndex();
    static void setNewContactTabIndex(int index);
    static QString getNewContactQSLSent();
    static void setNewContactQSLSent(const QString &qslsent);
    static QString getNewContactLoTWQSLSent();
    static void setNewContactLoTWQSLSent(const QString &qslsent);
    static QString getNewContactEQSLWQSLSent();
    static void setNewContactEQSLQSLSent(const QString &qslsent);
    static QString getNewContactQSLVia();
    static void setNewContactQSLVia(const QString &qslvia);
    static QString getNewContactPropMode();
    static void setNewContactPropMode(const QString &propmode);
    static bool getNewContactTabsExpanded();
    static void setNewContactTabsExpanded(bool state);
    static QString getNewContactSatName();
    static void setNewContactSatName(const QString &name);

    /*************
     * Online Map
     *************/
    static QStringList getMapLayerStates(const QString &widgetID);
    static bool getMapLayerState(const QString &widgetID, const QString &layerName);
    static void setMapLayerState(const QString &widgetID, const QString &layerName, bool state);

    /***************
     * WSJTX Dialog
     ***************/
    static uint getWsjtxFilterDxccStatus();
    static void setWsjtxFilterDxccStatus(uint mask);
    static QString getWsjtxFilterContRE();
    static void setWsjtxFilterContRE(const QString &re);
    static int getWsjtxFilterDistance();
    static void setWsjtxFilterDistance(int dist);
    static int getWsjtxFilterSNR();
    static void setWsjtxFilterSNR(int snr);
    static QStringList getWsjtxMemberlists();
    static void setWsjtxMemberlists(const QStringList &list);
    static QByteArray getWsjtxWidgetState();
    static void setWsjtxWidgetState(const QByteArray &state);
    static bool getWsjtxOutputColorCQSpot();
    static void setWsjtxOutputColorCQSpot(bool state);

    /******************
     * DXCluster Dialog
     ******************/
    static uint getDXCFilterDxccStatus();
    static void setDXCFilterDxccStatus(uint mask);
    static QString getDXCFilterContRE();
    static void setDXCFilterContRE(const QString &re);
    static QString getDXCFilterSpotterContRE();
    static void setDXCFilterSpotterContRE(const QString &re);
    static bool getDXCFilterDedup();
    static void setDXCFilterDedup(bool state);
    static int getDXCFilterDedupTime(int defaultValue);
    static void setDXCFilterDedupTime(int value);
    static int getDXCFilterDedupFreq(int defaultValue);
    static void setDXCFilterDedupFreq(int value);
    static QStringList getDXCFilterMemberlists();
    static void setDXCFilterMemberlists(const QStringList &list);
    static bool getDXCAutoconnectServer();
    static void setDXCAutoconnectServer(bool state);
    static bool getDXCKeepQSOs();
    static void setDXCKeepQSOs(bool state);
    static QByteArray getDXCDXTableState();
    static void setDXCDXTableState(const QByteArray &state);
    static QByteArray getDXCWCYTableState();
    static void setDXCWCYTableState(const QByteArray &state);
    static QByteArray getDXCWWVTableState();
    static void setDXCWWVTableState(const QByteArray &state);
    static QByteArray getDXCTOALLTableState();
    static void setDXCTOALLTableState(const QByteArray &state);
    static int getDXCConsoleFontSize();
    static void setDXCConsoleFontSize(int value);
    static QStringList getDXCServerlist();
    static void setDXCServerlist(const QStringList &list);
    static QString getDXCLastServer();
    static void setDXCLastServer(const QString &server);
    static QString getDXCFilterModeRE();
    static void setDXCFilterModeRE(const QString &re);
    static QStringList getDXCExcludedBands();
    static void setDXCExcludedBands(const QStringList &excluded);
    static bool setDXCTrendContinent(const QString &cont);
    static QString getDXCTrendContinent(const QString &def);
    static void removeDXCTrendContinent();

    /**************
     * Export ADIF
     **************/
    static QSet<int> getExportColumnSet(const QString &paramName, const QSet<int> &defaultValue);
    static void setExportColumnSet(const QString &paramName, const QSet<int> &set);

    /*****************
     * Logbook dialog
     *****************/
    static QByteArray getLogbookState();
    static void setLogbookState(const QByteArray &state);
    static int getLogbookFilterSearchType(int defaultValue);
    static void setLogbookFilterSearchType(int type);
    static QString getLogbookFilterBand();
    static void setLogbookFilterBand(const QString &name);
    static QString getLogbookFilterMode();
    static void setLogbookFilterMode(const QString &name);
    static QString getLogbookFilterCountry();
    static void setLogbookFilterCountry(const QString &name);
    static QString getLogbookFilterUserFilter();
    static void setLogbookFilterUserFilter(const QString &name);
    static QString getLogbookFilterClub();
    static void setLogbookFilterClub(const QString &name);

    /************************
     * Encrypted Passwords
     ************************/
    static QByteArray getEncryptedPasswords();
    static void setEncryptedPasswords(const QByteArray &data);
    static void removeEncryptedPasswords();
    static QString getSourcePlatform();
    static void setSourcePlatform(const QString &platform);
    static void removeSourcePlatform();

    /**************
     * Main Window
     *************/
    static bool getMainWindowAlertBeep();
    static void setMainWindowAlertBeep(bool state);
    static int getMainWindowDarkMode();
    static void setMainWindowDarkMode(int state);
    static QByteArray getMainWindowGeometry();
    static void setMainWindowGeometry(const QByteArray &state);
    static QByteArray getMainWindowState();
    static void setMainWindowState(const QByteArray &state);
    static QString getMainWindowBandmapWidgets();
    static void setMainWindowBandmapWidgets(const QString &value);
    static void removeMainWindowBandmapWidgets();

    /*********************
     * QSL Print Labels
     *********************/
    static int getQslLabelTemplate();
    static void setQslLabelTemplate(int index);
    static QString getQslLabelFooterLeft();
    static void setQslLabelFooterLeft(const QString &text);
    static QString getQslLabelFooterRight();
    static void setQslLabelFooterRight(const QString &text);
    static int getQslLabelSkip();
    static void setQslLabelSkip(int count);
    static int getQslLabelZoom();
    static void setQslLabelZoom(int zoom);
    static int getQslLabelCustomPageSize();
    static void setQslLabelCustomPageSize(int pageSizeIndex);
    static int getQslLabelCustomCols();
    static void setQslLabelCustomCols(int cols);
    static int getQslLabelCustomRows();
    static void setQslLabelCustomRows(int rows);
    static double getQslLabelCustomLabelWidth();
    static void setQslLabelCustomLabelWidth(double width);
    static double getQslLabelCustomLabelHeight();
    static void setQslLabelCustomLabelHeight(double height);
    static double getQslLabelCustomLeftMargin();
    static void setQslLabelCustomLeftMargin(double margin);
    static double getQslLabelCustomTopMargin();
    static void setQslLabelCustomTopMargin(double margin);
    static double getQslLabelCustomHSpacing();
    static void setQslLabelCustomHSpacing(double spacing);
    static double getQslLabelCustomVSpacing();
    static void setQslLabelCustomVSpacing(double spacing);
    static bool getQslLabelPrintBorders();
    static void setQslLabelPrintBorders(bool enabled);
    static QString getQslLabelDateFormat();
    static void setQslLabelDateFormat(const QString &format);
    static QString getQslLabelSansFont();
    static void setQslLabelSansFont(const QString &family);
    static QString getQslLabelMonoFont();
    static void setQslLabelMonoFont(const QString &family);
    static QString getQslLabelExtraColumn();
    static void setQslLabelExtraColumn(const QString &column);
    static QString getQslLabelExtraColumnHeader();
    static void setQslLabelExtraColumnHeader(const QString &header);
    static QString getQslLabelToRadioText();
    static void setQslLabelToRadioText(const QString &text);
    static QString getQslLabelHdrDate();
    static void setQslLabelHdrDate(const QString &text);
    static QString getQslLabelHdrTime();
    static void setQslLabelHdrTime(const QString &text);
    static QString getQslLabelHdrBand();
    static void setQslLabelHdrBand(const QString &text);
    static QString getQslLabelHdrMode();
    static void setQslLabelHdrMode(const QString &text);
    static QString getQslLabelHdrQsl();
    static void setQslLabelHdrQsl(const QString &text);
    static int getQslLabelMaxRows();
    static void setQslLabelMaxRows(int rows);
    static double getQslLabelFontSizeToRadio();
    static void setQslLabelFontSizeToRadio(double size);
    static double getQslLabelFontSizeCallsign();
    static void setQslLabelFontSizeCallsign(double size);
    static double getQslLabelFontSizeHeader();
    static void setQslLabelFontSizeHeader(double size);
    static double getQslLabelFontSizeData();
    static void setQslLabelFontSizeData(double size);

private:
    static QCache<QString, QVariant> localCache;
    static QMutex cacheMutex;

    static bool setParam(const QString&, const QVariant &);
    static bool setParam(const QString&, const QStringList &);
    static QVariant getParam(const QString&, const QVariant &defaultValue = QVariant());
    static QStringList getParamStringList(const QString&, const QStringList &defaultValue = QStringList());
    static void removeParamGroup(const QString&);
    static QStringList getKeys(const QString &);
    static QString escapeString(const QString &input, QChar escapeChar = '\\', QChar delimiter = ',');
    static QString unescapeString(const QString &input, QChar escapeChar = '\\');
    static QString serializeStringList(const QStringList &list, QChar delimiter = ',', QChar escapeChar = '\\');
    static QStringList deserializeStringList(const QString &input, QChar delimiter = ',', QChar escapeChar = '\\');
};

#endif // QLOG_CORE_LOGPARAM_H
