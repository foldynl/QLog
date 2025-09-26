%define REPO_VERSION %(echo $REPO_VERSION)

Summary: Qt Logging program for hamradio operators
Name: QLog
Version: %{REPO_VERSION}
Release: 1%{?dist}
License: GPLv3
Group: Productivity/Hamradio/Logging
Source: https://github.com/foldynl/QLog/archive/refs/tags/v%{version}.tar.gz#/qlog-%{version}.tar.gz
URL: https://github.com/foldynl/QLog/wiki
Packager: Ladislav Foldyna <ok1mlg@gmail.com>

%description
QLog is an Amateur Radio logging application for Linux, Windows and Mac OS. It
is based on the Qt 5 framework and uses SQLite as database backend.

%prep
%global debug_package %{nil}
%setup
%setup -T -D -b 1 


%build
/usr/bin/qmake-qt5 PREFIX='/usr'
%make_build

%install
INSTALL_ROOT=%{buildroot} make -f Makefile install

%post

%postun

%files
%{_bindir}/*
%license LICENSE
%doc README.md Changelog
%{_datadir}/applications/qlog.desktop
%{_datadir}//icons/hicolor/256x256/apps/qlog.png
%{_metainfodir}/*

%changelog
* Fri Sep 26 2025 Ladislav Foldyna - 0.46.1-1
- Fixed QSO filter incorrectly displays inserted date (issue #752)
- Fixed Logbook Search Icon is not centered (issue #753)
- Fixed Using CQRLog API Key for Clublog (issue #759)
- Fixed Online Service Password leaking via debug files (issue #760)
- Fixed Tabs are not showed properly (PR #762 @aa5sh)
- Fixed inability to edit Power in QSO Detail (issue #763)

* Fri Sep 12 2025 Ladislav Foldyna - 0.46.0-1
- [NEW] - NewContact: POTA/SOTA/WWFF/IOTA info is taken from the nearest spot (@aa5sh @foldynl)
- [NEW] - DXSpots are sent to Flex Radio (PR #694 @aa5sh)
- [NEW] - CWConsole - Added support for automatic periodic sending of CW Macros (issue #708)
- [NEW] - Added mapping of Winkey hardware buttons to CW macro keys F1–F4 (issue #711)
- [NEW] - Added Search Types to the Logbook Search Editbox (issue #733)
- [NEW] - Rig - Periodic Rig status reporting independent of its changes (PR #730 @aa5sh)
- [NEW] - Settings - Serial Port Completer for MacOS and Linux (PR #737 @aa5sh)
- [NEW] - Hamlib - Attempt to send Power On when connecting the Rig
- [NEW] - POTA Spots Info is received from the API at api.pota.app to improve POTA detection
- [CHANGED] - Logbook Search Filter Widget - Added Search Widget
- [CHANGED] - QSO Detail Country boxes use the new Search Filter Widget
- [CHANGED] - NewContact - RST field uses the Overwrite Mode
- [CHANGED] - NewContact - Default cursor position in RST is at the first character
- [CHANGED] - Setting Dialog - Modes - Default Report can define cursor position
- [CHANGED] - HamlibRot - Changed Poll Interval from 500 to 2000ms
- Fixed DUPE is not detected for the first Contest QSO (issue #699)
- Fixed FREQ_RX/BAND_RX is present only when RX and TX freqs are different (issue #714)
- Fixed Repeated log recording (issue #722)
- Fixed QSO Filter contains untranslated QSO items (issue #732)

* Fri Jul 11 2025 Ladislav Foldyna - 0.45.0-1
- [NEW] - Single Dialog for Upload/Download Online Services (issue #448)
- [NEW] - Added option to swap paddles to Winkey settings (issue #676)
- [NEW] - Added native support for the FLRig interface (issue #679)
- [NEW] - Added New Version Notification - only for MacOS and Windows (PR #669 @aa5sh)
- [NEW] - QRZ Upload - Added support for multiple API Keys
- [NEW] - Logbook - Added highlighting to the filter button when a filter is active
- [NEW] - WSJTX - Filtered label is shown when filter is enabled
- [NEW] - DXC - Filtered label is shown when filter is enabled
- [NEW] - Added support to Upload Cloudlog/Wavelog
- [NEW] - Add JS8 to legacy_modes.json (issue #677)
- [CHANGED] - Unification Settings storage
- [CHANGED] - Calculate distances according to IARU rules
- Fixed missing Wsjtx Spot values in the AlertWindow

* Sun May 11 2025 Ladislav Foldyna - 0.44.1-1
- Fixed Rotator Widget Seg Fault for new users (issue #666)

* Fri May 9 2025 Ladislav Foldyna - 0.44.0-1
- [NEW] - Activity Manager - Added SKCC, UKSMG and FISTS as Dynamic Fields
- [NEW] - QSO - FISTS, SKCC, and UKSMG are auto-filled from MembershipQE (issue #628)
- [NEW] - Rotator - Added QSO destination needle (issue #644)
- [NEW] - QSO Detail - Adds grayline and short path (PR #653 @kyleboyle)
- [CHANGED] - Rotator - Needle colors correspond to the online map (issue #644)
- Fixed TCI cw_macros must contain RigID (issue #663)
- Fixed TCI setKeySpeed sets keyer and macros speed (issue #663)
- Fixed Data mode missing in rig control window - rigctld (issue #660 @aa5sh)
- Improved DXC Mode detection
- Updated Simplified Chinese translation

* Sat Apr 19 2025 Ladislav Foldyna - 0.43.1-1
- Fixed Click on PHONE DX Spots sets wrong mode (issue #453)
- Fixed No freq via Omnirig for IC7400 (issue #639)

* Fri Apr 4 2025 Ladislav Foldyna - 0.43.0-1
- [NEW] - Added support to receive QSOs from CSN SAT Device (PR #610 @aa5sh)
- [NEW] - Bandmap - Multiple independent bandmap windows (PR #593 @kyleboyle @foldynl)
- [NEW] - Winkey Keyer driver currently supports v1 and v2 hardware
- [NEW] - QSO Detail - QSLs QSLr Msg is editable
- [NEW] - Activity Manager - Added new dynamic field - QSL Message Send
- [CHANGED] - HamlibDrv - Enabled Power, RIT, XIT, Morse for RIG Netctl
- [CHANGED] - QSO Detail - QSLMSG replaced by QSLMSG_RCVD (issue #633)
- [CHANGED] - eQSL Upload - Added an option to choose a QSLMsg field
- [CHANGED] - eQSL Download - eQSL QSLMSG is stored to QLog QSLMSG_RCVD
- [CHANGED] - eQSL Download - Added QSLMSG_RCVD merging - eQSL message is appended
- Fixed Speed pot doesn't seem to work with a WinKeyer; double chars (issue #618)
- Fixed BandMap Spots colour change after a QSO (issue #632)
- Fixed Incorrect Field Mapping for Received eQSL Messages (issue #633)

* Fri Mar 7 2025 Ladislav Foldyna - 0.42.2-1
- Fixed Logbook country translations (issue #608)
- Fixed Unexpected dialog when QSO after contest (issue #614)
- Fixed Statistics Widget does not display NULL continents (@aa5sh)
- Fixed Statistics Widget does not display NULL Band, Mode
- Fixed Statistics Widget TOP10 does not display removed DXCCs
- Fixed Statistics Widget TOP10 does not display translated country names
- Fixed Awards Widget does not display removed DXCCs

* Sat Feb 22 2025 Ladislav Foldyna - 0.42.1-1
- Fixed Unexpected timezone info (issue #600)
- Fixed DXCC Statistics picks more entities (issue #601)
- Fixed a crash when no internet connection

* Fri Feb 14 2025 Ladislav Foldyna - 0.42.0-1
- [NEW] - Awards - Added Slots - total over each band (issue #538)
- [NEW] - Awards - Added Grid Award - 2/4/6 Chars grid (issue #564)
- [NEW] - Settings - Added options to switch 12/24 time and date format (issue #573)
- [NEW] - Activity Manager - Added new dynamic fields - Rig (issue #575)
- [NEW] - LoTW Import - Fill missing information also for confirmed QSOs (@aa5sh)
- [NEW] - Added CW macro QTH
- [NEW] - DXC - Added 15min Trend
- [CHANGED] - Changed IOTA LOV Source, the official web is used
- [CHANGED] - CSV Export: Time and Date formats use ISO8601 format (issue #562)
- [CHANGED] - Settings - Renamed Shortcuts to GUI tab
- [CHANGED] - LOV - Improved LOV download performance (PR #582 @aa5sh)
- Partial fix Windows State/Size does not save in case of fullscreen (issue #418)
- Fixing TCI error when you change Rig (issue #526)
- Fixed DXCC Award total worked confirmed counts included deleted entities (PR #588 @aa5sh)
- Fixed Raspberry PI Flatpak - Import Select file dialog crashes (issue #589)
- Suppressed the ability to edit Contest fields after the start (issue #590)

* Tue Jan 21 2025 Ladislav Foldyna - 0.41.1-1
- Fixed compilation issue under Debian 12 (issue #571)
- Fixed Incorrect GPL version in rpm/deb packages (issue #572)
- Fixed MacOS floating utility window bug (PR #576 @kyleboyle)
- Updated IT translation

* Sat Jan 11 2025 Ladislav Foldyna - 0.41.0-1
- [NEW] - Logbook - Added a new context menu item - Update QSO from Callbook (issue #450 @aa5sh)
- [NEW] - DIGI mode is used in case of DXC Digi Spots (issue #480)
- [NEW] - DXC - Retrieve information about SOTA, POTA, IOTA and WWFF from comment (issue #482)
- [NEW] - Alert - Added SOTA, POTA, IOTA and WWFF filter
- [NEW] - Added the COM Port Completer for Windows platform (issue #490)
- [NEW] - Settings - Added DXCC Confirmed By options (issue #508)
- [NEW] - Added POTA Export Formatter (activator/hunter) (PR #514 @kyleboyle)
- [NEW] - CW Console - CW Halt with the user-defined shortcut (issue #518)
- [NEW] - Added an input parameter to save debug message to file
- [NEW] - Logbook - Added sorting function to logbook table columns (PR #540 @kyleboyle)
- [NEW] - Network Notification - Added Rig Status Notification
- [NEW] - Implemented ADIF 3.1.5
- [NEW] - ADIF 3.1.5 - Added new submodes FSKH245 and FSKH105 for HELL
- [NEW] - ADIF 3.1.5 - Added new contest IDs
- [NEW] - ADIF 3.1.5 - Added new columns (Import/Export only)
- [NEW] - ADIF 3.1.5 - Added My DARC DOK to Station Profile
- [CHANGED] - Settings: disabled band and mode name modification
- [CHANGED] - DX Stats contain all enabled bands (issue #538)
- [CHANGED] - Removed Freq, TimeDate On/Off Data Type Indicators (issue #552)
- [CHANGED] - ADIF 3.1.5 - VUCC and MY_VUCC can contain 6 or 4-chars locators
- [CHANGED] - Stop exporting default value N for qsl_rcvd, qsl_sent, lotw/dcl/eslq_qsl_rcvd/sent
- [CHANGED] - Extended QSL/Import Dupe matching rule to Callsign, Band, Mode, Time and Sat_Name (issue #563)
- Fixed MacOS - keep floating windows visible on app unfocus (issue #530)
- Fixed Contest Filter ignores the first QSO (issue #529)
- Fixed It is not possible to quit Qlog with detached widgets Rot and Rig (issue #534)
- Fixed ADX/CSV/JSON do not export non-ASCII chars (issue #542)
- Fixed Checking the 60m checkbox in cluster filters allows 160m spots to appear (issue #543 @aa5sh)
- Fixed Problems uploading to QRZ.com (issue #559 PR #561 @aa5sh)
- Fixed DX Stat screen is jumping up/down
- Fixed Omnirig drivers: Digi modes are not correclty recognized

* Fri Nov 29 2024 Ladislav Foldyna - 0.40.1-1
- Fixed Bands - Added missing 8m band (issue #515)
- Fixed CW Console - EXCSTR does not work properly (issue #517)
- Fixed Activity Manager - Missing Propagation Mode None (issue #519)
- Fixed QSO Filter - filter fields with random order (PR #525 @aa5sh)
- Fixed TCI error when you change Rig (issue #526)
- Fixed NewContact - satellite mode too wide (issue #527)

* Sun Nov 24 2024 Ladislav Foldyna - 0.40.0-1
- [NEW] - Activity Manager - merged Layout Manager and profiles (issue #408)
- [NEW] - Activity Manager - Added new dynamic fields - Contest fields, RX/TX Power
- [NEW] - Added light support for contests (issue #345)
- [NEW] - Added CW macros EXCHSTR, EXCHNR, EXCHNRN
- [NEW] - Export Filter - Added user filter combo (original idea PR #476 @aa5sh)
- [NEW] - New Contact -  Added expand/collapse button to QSO field tab widget (PR #495 @kyleboyle)
- [NEW] - Alert - Added CQZ and ITUZ filters
- [NEW] - KSTChat - Added a new 40MHz room (PR #496 @kyleboyle)
- [NEW] - Station Profile contains Operator Callsign (issue #441 @kyleboyle)
- [NEW] - Station Profile contains County (issue #493 @kyleboyle)
- [NEW] - Statistics - Adds time of day and better qso mapping (PR #501 @kyleboyle)
- [NEW] - Bandmap - Tooltip shows a spotter callsign (PR #507 @Skittlebrau)
- [CHANGED] - New Contact - Renamed DXCC Tab to DX Stats contains DXCC and Station Statistics (issue #477)
- [CHANGED] - QSL Import dialog - Detail text is selectable by mouse and keyboard
- [CHANGED] - Removed Main Menu Layout; Activity Manager is in the bottom-left corner
- [CHANGED] - Removed Keep Options from the Equipment Menu - use Activity Manager for it
- Fixed issue when CW is always selected after Settings exiting or connecting the Rig
- Updated Timezone definition file - version 2024b

* Sat Oct 5 2024 Ladislav Foldyna - 0.39.0-1
- [NEW] - DXC - Added Full-text search
- [NEW] - Select S in RST Edit when focused (issue #454)
- [NEW] - Alerts - Added Member Column
- [CHANGED] - HamlibDrv Rig/Rot- Added multiplatform reliable sleep
- [CHANGED] - Changed Backup policy
- [CHANGED] - Logbook page size - improved performance
- [CHANGED] - Logbook - CTRL-A (Select All) is disabled
- [CHANGED] - Awards - Bands are displayed based on the Settings (issue #452)
- [CHANGED] - WSJTX - More reliable detection of CQ stations (PR #471 @aa5sh)
- [CHANGED] - WSJTX - SOTA/POTA/WWFF/SIG are being added to the logged QSO (PR #463 @aa5sh)
- [CHANGED] - Stats - Add a confirmation dialog for displaying over 50k QSOs on the map
- [CHANGED] - New Contact - Starting QSO Timer when Rig online and WSJTX Update Callsign Status is received
- [CHANGED] - Added a postponed handling for Rig soft errors (issue #472)
- Fixed WSJT-X does not emit band change if rig is disconnected (issue #447)
- Fixed Wrong import of ADIF file of another log program (issue #455)
- Fixed WSJTX log record is stored incorrectly if it contains non-ASCII chars(issue #458)
- Fixed ADIF import does not import records with old DXCC Entities (issue #459)
- Fixed ADIF import incorrectly uses Station Profile parameters (issue #461)
- Fixed Logbook - QSO Table Column Width Does Not Stick (issue #464)
- Fixed Alerts Window displays OOB Spots (issue #469)
- Fixed Field values from past QSOs are used incorrectly in case of WSJTX QSOs (#issue 470)

* Thu Aug 29 2024 Ladislav Foldyna - 0.38.0-1
- [NEW] - Logbook - Added Send DX Spot to the QSO Context Menu
- [NEW] - DX Filter - Added Dedup Time/Freq difference setting (@aa5sh)
- [NEW] - Rig Setting - Added RTS/DTR PTT Type support (issue #353)
- [NEW] - Bandmap - Scrollbar position is saved per band (issue #415)
- [NEW] - New Contact - Added a dynamic value completer for SIG field (issue #425)
- [NEW] - Awards - Added SOTA/POTA/WWFF (@aa5sh issue #311)
- [NEW] - Awards - Added Not-Worked Filter
- [NEW] - New Contact - Added Long Path Azimuth info
- [NEW] - POTA Fields allow a comma-delimited list of one or more POTA Refs
- [NEW] - WSJTX tunes freq/mode like Rig if rig is disconnected
- [CHANGED] - Alert Widget is a Dock Widget (issue #399)
- [CHANGED] - QLog adds more information from callbook for WSJTX QSOs (issues #403 #405 #420)
- [CHANGED] - File Open dialogs are not a native dialog under Linux (issue #427)
- [CHANGED] - Profiles transferred to DB
- [CHANGED] - LOV last_dates transferred to DB
- [CHANGED] - DX Cluster - Login Callsign is always the base Callsign
- [REMOVED] - Setting DXCC Date
- Fix for MacOS Layout Geometry Restore (@aa5sh)
- Fixed TQSL does not block GUI thread
- Fixed MacOS build process (@aa5sh)

* Fri Jul 26 2024 Ladislav Foldyna - 0.37.2-1
- Fixed Field QSL Send Via should be retained (issue #413)
- Fixed Set rotator position fails if azimuth > 180 (issue #417)
- Fixed Windows State/Size does not save in case of fullscreen (issue #418)
- Fixed Significant rounding during azimuth calculation (issue #422)
- Updated Simplified Chinese translation
- Updated Spanish translaction
- Added Italian translation (thx IK1VQY)

* Wed Jul 10 2024 Ladislav Foldyna - 0.37.1-1
- Fixed QSO Table Callsign filter is not filled properly (issue #401)
- Fixed DXC zero frequency for last QSO in case of FT8 QSOs (issue #404)
- Fixed Callsign Context Menu does not work (issue #409)
- Fixed QSO Detail Save and Edit buttons are not translated (issue #410)

* Mon Jul 1 2024 Ladislav Foldyna - 0.37.0-1
- [NEW] - Added Shortcuts Editor (issue #293)
- [NEW] - Added QO100 Bandplan to correctly categorize the DX Spots
- [NEW] - Improveded detection of SH/DX SHF Spots
- [NEW] - Online Map - Added WSJTX CQ Spots
- [NEW] - WSJTX - Sortable View
- [NEW] - Alerts - Sortable View
- [NEW] - Added Spanish translation (thx LU1IDC)
- [NEW[ - Added Search Callsign Clear Button (issue #396)
- [CHANGED] - QRZ auth should be over POST with form data (issue #389)
- [CHANGED] - Big CTY file is used
- [CHANGED] - Callbook Country DXCC ID is used in case of difference from Big CTY
- [CHANGED] - Removed ALT+W and CTRL+DEL shortcuts
- [CHANGED] - Removed New Contact and Save Contact from Logbook Main Menu
- Fixed Guantanamo KG4 Issue (issue #372)
- Fixed QRZ Lookup Not Including Full Name - History (issue #388)
- Fixed Spot Last QSO contains TX freq, should contain RX freq (issue #390)
- Fixed Spot Last QSO must contain Freq in kHz (issue #391)
- Fixed Bandmap select previous selected callsign issue (issue #394)
- Fixed Malfunctioning tuning of WSJTX Alert spot
- Fixed DXCC Status for FT4 Spots incorrectly identified in WSJTX

* Fri Jun 7 2024 Ladislav Foldyna - 0.36.0-1
- [NEW] - WSJTX: Added support to received ADIF QSO Log record
- [NEW] - Sat mode is derived from RX/TX Freq
- [NEW] - Logbook filters change color when enabled
- [NEW] - Frequency input boxes PageUp/Dn switches the band (issue #360)
- [NEW] - CTRL + PgUp/Dn switch band on the connected rig - global shortcut (issue #360)
- [NEW] - Added number of filtered QSOs (issue #374)
- Fixed Callbook query does not work (issue #377)
- Fixed Logbook columns are reordered after Delete (issue #383)
- Fixed Missing Republic of Kosovo flag (issue #384)

* Tue May 21 2024 Ladislav Foldyna - 0.35.2-1
- Improved delete performance; added delete progress bar (issue #351)
- Fixed Password with plus is incorrectly sent to online services (issue #366)
- Fixed Compilation issue under v4.6 (issue #368)
- Fixed Network Rig configuration is not saved (issue #370)

* Mon May 6 2024 Ladislav Foldyna - 0.35.1-1
- Fixed Free QRZ callbook - Name is not populating (issue #363)
- Fixed Incorrect CW segment freqs (issue #365)

* Fri May 3 2024 Ladislav Foldyna - 0.35.0-1
- [NEW] - Added Rot Interface PSTRotator Network
- [NEW] - Added QSO/QSL Since option to eQSL Dialog
- [NEW] - Bandmap - Current Mode segment visualisation
- [NEW] - CW Console - Added Word/Whole mode switch
- [NEW] - Added Callbook Profile Image Widget
- [NEW] - ASCII conversion based on Text-Unidecode/iconv algorithm (issue #316 #350)
- [NEW] - ITU/CQ Zones can be defined in Station Profile (issue #358)
- [CHANGED] - Spacebar is used as a focus changer for fields where space is not allowed
- [CHANGED] - Focus does not select text in the input fields
- [CHANGED] - Force XCB under Linux Wayland
- [CHANGED] - Bandmap - Added Callsign/Freq/Mode to tooltip (issue #355)
- Fixed incorrect ADIF date format for clublog_qso_upload_date (issue #342)
- Fixed The last name from Callbooks queries (issue #346)

* Mon Mar 25 2024 Ladislav Foldyna - 0.34.0-1
- [NEW] - Rotator Widget - Azimuth by Clicking
- [NEW] - Rotator Widget - QSO button provides Short/Long Path (issue #330)
- [NEW] - Equipment Menu - Added Keep Options between application restart (issue #331)
- Fixed TCI - Thetis Connection issue (issue #327)
- Fixed TCI - Spots To Rig are not displayed (issue #328)
- Fixed Bandmap unintentionally emits frequency labels (issue #333)
- Fixed Failing to load grid square for G and EI SOTA summits (issue #336)
- Fixed HRDLog On-Air message is not sent (issue #337)
- Fixed Offline Map - Improved Path drawing

* Sat Mar 9 2024 Ladislav Foldyna - 0.33.1-1
- Fixed Rotator offline map is incorrectly centered (issue #324)
- Fixed Hamlib integration not working (issue #325)
- Fixed issue when Hamlib reopen rig caused Initialization Error
- Fixed issue when Omnirig Drv did not emit rigIsReady signal

* Fri Mar 8 2024 Ladislav Foldyna - 0.33.0-1
- [NEW] - Added Rig Interface TCI
- [NEW] - Callbook search can be temporarily paused
- Improved DXC Mode recognition
- Fixed Modal dialog blinks - Windows platform (issue #315)
- Fixed LoTW and eQSL download are only QSLs dowloads - button label changed (issue #318)
- Fixed i18n: Country Names and Prop-modes are translated (issue #322)

* Sat Feb 10 2024 Ladislav Foldyna - 0.32.0-1
- [NEW] - Added Rig Interface Omnirig v1 (Windows only)
- [NEW] - Added Rig Interface Omnirig v2 (Windows only)
- [NEW] - Clublog - Added Clear Clublog and reupload QSOs
- [NEW] - Clublog - Added Real-time Insert/Update/Delete
- [CHANGED] - Clublog - Upload callsign is derived from the Current Profile Callsign
- Fixed clang linker failed issue (issue #301)
- Fixed SAT Mode U/U missing (issue #308 PR #309 thanks ea5wa)
- Fixed Multiple QSO selection. Callsigns modified by mistake (issue #310)
- Fixed Callbook query cache is not properly cleared when Callbook settings change (issue #313)

* Fri Jan 5 2024 Ladislav Foldyna - 0.31.0-1
- [NEW] - DXC - Improved Mode recognition
- [NEW] - DXC - Switch Rig mode based on DXC Spot Mode (issue #217)
- [NEW] - DXC - Added Spot Country Column (issue #273)
- [NEW] - DXC - Added Menu for server management
- [NEW] - DXC - Added Auto-connect to server
- [NEW] - DXC - Added Keep QSOs Context Menu
- [NEW] - DXC - Added Clear QSO Context Menu
- [NEW] - DXC - Added support for SH/DX response parsing
- [NEW] - DXC - Added support for username, password for connection
- [CHANGED] - DXC - Commands Combo changed to function button with selectable function
- [CHANGED] - DXC - DX Spot is prepared via DXC Command Line, Remark dialog was removed
- [NEW] - Online Map - IBP station double-click tunes freq and switch Rig mode
- [NEW] - Main Window - Current profile name is shown (issue #282)
- [NEW] - Import - Details can be saved to file (issue #284)
- [NEW] - Added Simplified Chinese translation (PR #285 thank BG7JAF)
- [NEW] - New Contact - Enter saves QSO if QSO time is running (issue #293 - partial)
- [NEW] - New Contact - Callsign Enter event saves QSO if no Callbook is active - Pileup Mode (issue #293)
- [NEW] - RIG Widget - RIT/XIT are displayed with user-friendly units (issue #294)
- [CHANGED] - SAT List download - Shortened download period for SAT list from 30 to 7 days
- Fixed ADI Import is too slow (issue #270)
- Improved Import/Export Performance
- Fixed Missing Satellite Mode SX (issue #291)
- Fixed QSO Detail - Issue when Sat-Name field was always disabled
- Fixed RPM build - Installed (but unpackaged) metainfo file issue

* Fri Dec 1 2023 Ladislav Foldyna - 0.30.0-1
- [NEW] - QSL Images are stored in the database
- [NEW] - Added AppStream Metainfo File (PR #262 thanks AsciiWolf)
- [NEW] - Added (WPX) prefix (issue #263)
- [NEW] - Added WPX Award statistics
- [NEW] - Added support for external translation files(issue #275)
- [CHANGED] - Removed QSOID from Export dialog column setting (issue #258)
- Fixed Date editor does not support NULL value in Logbook Direct Editor (issue #256)
- Fixed duplicate entry in Windows Add or Remove - only Window platform (issue #260)
- Fixed RST fields revert to 59 after changing them (issue #261)
- Fixed Cannot change TQSL Path in Settings - flatpak (issue #271)

* Mon Nov 13 2023 Ladislav Foldyna - 0.29.2-1
- Fixed QLog is already running error popup on MacOS (issue #257 thanks rjesson)

* Fri Nov 10 2023 Ladislav Foldyna - 0.29.1-1
- Fixed QSL cards tooltip are not displayed under qt6.5 (issue #248)
- Fixed Distance unit is not displayed in QSO Info under Windows (issue #250)
- Fixed Editing STATION_CALLSIGN can cause unwanted change in QSO Detail (issue #251)
- Fixed QSO Detail Operator Name containes an incorrect value (issue #252)
- Fixed Calls with VE, VA are coding as Amsterdam & St Paul Islands instead of Canada (issue #253)
- Fixed LoTW QSL import reports unmatched QSOs sometime (issue #254)

* Fri Oct 20 2023 Ladislav Foldyna - 0.29.0-1
- [NEW] - Added user-defined layout for New QSO Detail widget
- [NEW] - Main window State and Geometry can be saved to layout profile
- [NEW] - Awards - Added WAS
- [NEW] - Awards - WAZ/ITU/WAC show all possible values
- [NEW] - Distance unit (km/miles) is controlled by OS Locale
- [CHANGED] - Removed SAT Tab - field can be added via Layout Editor
- Improved Import QSO performance
- Fixed QLog crashes if POTA, SOTA or WWFF contain incorrect values (issue #245)
- Fixed QSOs are not uploaded to QRZ and HRDLog if fields contain HTML delimiter strings (issue #247)

* Fri Sep 22 2023 Ladislav Foldyna - 0.28.0-1
- [NEW] - Added ON4KST Chat Support
- [NEW] - Added Az BeamWidth and Az Offset to Antenna Profile
- [NEW] - Double-Clicking the IBP callsign in the online map tunes the frequency
- Fixed Browse button should open an expecting folder (issue #241)
- Fixed Reword QSL buttons and Settings in QSO Details and Settings (issue #242)

* Mon Aug 21 2023 Ladislav Foldyna - 0.27.0-1
- [NEW] - Added HRDLog Support
- Fixed Text field alignment (issue #233)
- Fixed Rig/Rot Connection port type selection (issue #235)
- Fixed Incorrect Distance Value in WSJTX Widget (issue #236)
- Fixed Incorrect WSJTX locator target on the map (issue #237)

* Sun Jul 30 2023 Ladislav Foldyna - 0.26.0-1
- [NEW] - Added user-defined layout for New QSO widget
- [NEW] - Pressing Spacebar in Callsign field skips RST fields
- [NEW] - Added user-defined URL for web lookup (issue #230)
- Fixed WSJTX QSOs should have an Operator Name from Callbook (issue #223)
- Fixed US call area suffixes not handled correctly (issue #226 thanks Florian)
- Fixed QSO Filter Detail allows to save an empty Filter Name (issue #228)

* Mon Jul 17 2023 Ladislav Foldyna 0.25.1-1
- Fixed Unexpected mode change when Setting Dialog is saved (issue #222)
- Fixed QSL_SENT field has an incorrect ADIF name (issue #225)

* Tue Jul 4 2023 Ladislav Foldyna - 0.25.0-1
- [NEW] - Export - Added CSV Format
- [NEW] - Export - Added Type of Export Generic/QSLs (issue #209)
- [NEW] - Export - Added Exported Columns Setting
- [NEW] - Export - All export formats use the ADIF field name convention
- [CHANGED] - Export - JSON format contains a header - JSON format change
- [CHANGED] - Default Statistics Interval is curr_date-1 and curr_day
- Fixed Errors from Secure Storage are not shown (issue #216)
- Fixed RX/TX Bands are uneditable when RX/TX freqs are missing (issue #220)

* Fri Jun 16 2023 Ladislav Foldyna - 0.24.0-1
- Fixed Incorrect FT4 mode-submode (issue #212)
- Fixed CONTESTIA mode should be CONTESTI (issue #213)
- Fixed Context menu deselects NewContactEditLine (issue #215)
- Fixed incorrect WSJTX Filter initialization (issue #218)

* Fri Jun 9 2023 Ladislav Foldyna - 0.23.0-1
- [NEW] - Added CWDaemon Keyer Support
- [NEW] - Added FLDigi Keyer Support
- [NEW] - Online Map - based on locale, the map language is selected (Only EN, FR, GE supported - issue #180)
- Fixed After entering longer QTH, the field content is not left-aligned (issue #157)
- Fixed wrong QSO Time in case of JTDX (issue #204)
- Fixed QSL Sent Date fields are not filled if QSL Sent Status fields are Y (issue #207)

* Sun May 7 2023 Ladislav Foldyna - 0.22.0-1
- [NEW] - ADIF Import - My Profile is used to define default values
- [NEW] - ADIF Import - Checking a minimal set of input fields (start_time, call, band, mode, station_callsign)
- [NEW] - ADIF Import - Added Import Result Summary + Import Detail Info
- [NEW] - Main Menu - Added Help -> Mailing List.
- [NEW] - Export - Filter for the exported QSOs
- [CHANGE] - Renamed Locator to Gridsquare
- Fixed Some anomalies in the input and processing of QSLr Date (issue #192)
- Fixed User unfriedly CW Keyer Error (issue #194)
- Fixed ADIF import (issue #196)
- Fixed Operator field is incorrectly used  (issue #197)
- Fixed Crash if an unknown POTA & SOTA/WWFF Setting is entered (issue #198)
- Fixed FLDIGI cannot connect QLog (issue #199)
- Fixed if ADIF record is missing band info, add this from freq field (thx DJ5CW)

* Sun Apr 16 2023 Ladislav Foldyna - 0.21.0-1
- [NEW] - Rotator - Added Used-Defined Buttons
- [NEW] - Rotator - Added Destination Azimuth Needle
- [NEW] - Online Map - Added Antenna Beam Path
- [NEW] - Rig - Combos are disbled when disconnected
- [NEW] - Club Member Lists (issue #60)
- [NEW] - Alert Table shows rule names
- [CHANGED] - Alerts, DXC and WSJTX Network Notifications
- Fixed Antenna Azimuth Negative Value (issue #191)
- Fixed CTY file is not loaded when duplicate record (issue #193)

* Tue Mar 14 2023 Ladislav Foldyna - 0.20.0-1
- [NEW] - Added MUF Layer to online map
- [NEW] - Added International Beacon Project (IBP) Beacons to online map
- [NEW] - Centering the map on the current profile at start (issue #185)
- Fixed incorrect ADIF interpretation of _SENT fields (issue #176)
- Fixed Awards Dialog, Table double click for ITU/CQZ/WAZ/IOTA shows incorrect QSOs (issue #177)
- Fixed ADIF double-type fields when 0.0 is currently mapped to NULL (issue #178)
- Fixed QSO Detail to save NULL instead of empty string (issue #179)
- Fixed ADIF Import default _INTL values are now stored correctly (issue #183)
- Fixed Maps show an incorrect path if from/to grids are the same (issue #186)
- Fixed Online Maps incorrect Bounds if Bandmap callsign double-click (issue #188)
- Updated German translation (thx DL2KI)

* Fri Feb 17 2023 Ladislav Foldyna - 0.19.0-1
- [NEW] - Added Aurora Layer to online map
- [NEW] - Logbook - filter options are saved and restored
- [NEW] - Map Setting is saved and restored (issue #140)
- [NEW] - QSO Duration (issue #158)
- [NEW] - DX Cluster uses monospace font (issue #164)
- [NEW] - Awards - if click on the Entity/band the logbook filter is set (issue #168)
- [NEW] - WSJTX - Added Multicast support (issue #172)
- Fixed WWFF LOV Download (issue #169)

* Sun Jan 15 2023 Ladislav Foldyna - 0.18.0-1
- [NEW] - ADIF 3.1.4 updates
-   Added new modes FREEDV and M17
-   Added new band (submm)
-   Adopted Altitude (for SOTA only)
-   Adopted POTA (includes POTA List)
-   Adopted Gridsquare_ext (only import/export)
-   Adopted Hamlogeu_* (only import/export)
-   Adopted HamQTH_* (only import/export)
- [NEW] - Added new DXCC Status and color for it - Confirmed
- [NEW] - New Contact - Tab selection is saved
- [NEW] - Grid can contain 8-characters
- [NEW] - User filter can contain NULL value
- [NEW] - Compilation - added variables for external sources
- [NEW] - My DXCC/CQZ/ITUZ/Country is filled
- [NEW] - Alerts - Added Aging (issue #153)
- [NEW] - Alerts - Added DXCC Status Color (issue #153)
- [NEW] - DXC - Added Log Status to filter (issue #154)
- [NEW] - DXC - Added Spot deduplication to filter (issue #154)
- [NEW] - WSJTX - Added CQ-Spot Filter (issue #155)
- [NEW] - QSO Detail contains DXCC Tab (issue #156)
- [CHANGED] - New QSO DXCC Tab reworked (issue #144)
- [CHANGED] - All DXCC Stats are computed based on My DXCC instead of My Callsign
- [CHANGED] - Station Profile Setting layout

* Sun Dec 18 2022 Ladislav Foldyna - 0.17.0-1
- [NEW] - NetPort and Polling interval can be defined for NET Rigs
- [NEW] - NetPort can be defined for NET Rots
- [NEW] - Added Saving Bandmap Zoom per band (issue #137)
- [NEW] - CW speed synchronisation (issue #139)
- Fixed Missing callbook data when callsign has prefix (issue #133)
- Fixed Winkey2 echo chars are incorrectly displayed in CW Console (issue #141)
- [CHANGED] - Online Map - Gray-Line is enabled by default
- Update Timezone database

* Sun Nov 20 2022 Ladislav Foldyna - 0.16.0-1
- [NEW] - SOTA/IOTA lists updated regularly
- [NEW] - Added WWFF list, updated regularly
- [NEW] - QTH/Grid are filled based on SOTA/WWFF
- [NEW] - DXC/WSJTX columns are movable, added column visibility setting
- [NEW] - DXC/WSJTX columns layout is saved
- [NEW] - Added Wiki&Report Issue links to Help section
- [NEW] - About dialog contains run-time information
- [NEW] - Solar Info as a ToolTip
- [NEW] - QSO Manual Entry Mode
- Fixed Bandmap unlogical animation when band is changed (issue #128)
- Fixed Bandmap marks are not displayed correctly when RIT/XI (issue #131)
- Fixed Setting Dialog size
- Update Timezone database

* Sun Oct 16 2022 Ladislav Foldyna - 0.15.0-1
- Fixed Keeping the Bandmap RX mark always visible when centre RX is disabled (issue #115)
- Fixed Equipment Menu: Swapped Connect Keyer and Rig (issue #122)
- Fixed Callsign is deleted when clicking bandmap (issue #126)
- Fixed typo in the Map layer menu (issue #127)
- Fixed compilation issues & warning under QT6 - preparation for QT6 migration

* Sun Oct 2 2022 Ladislav Foldyna - 0.14.1-1
- Fixed CW Console - HALT Button is not enabled under Ubuntu flavours (issue #124)

* Thu Sep 29 2022 Ladislav Foldyna - 0.14.0-1
- [NEW] CW Console (Winkey2, Morse over CAT support)
- [NEW] DX Cluster pre-defined commands (send last spot, get stats)
- [NEW] Added DX Cluster Views (Spots, WCY, WWV, ToALL)
- [NEW] Implemented DX Cluster Reconnection
- [NEW] Remember last used DX Cluster
- [CHANGED] - UI unifications - Rot/Rig/DXC
- Fixed COM port validation for Windows platform
- Fixed Reconnecting (DXC/Callbook) (issue #110)
- Fixed DX Cluster crashes when DXC server is not connected and a command is sent (issue #111)
- Fixed Bandmap callsign selection not fully works (issue #116)

* Sat Aug 6 2022 Ladislav Foldyna - 0.13.0-1
- [NEW] QSY Contact Wiping (issue #100)
- [NEW] Timeoff is highlighted when QSO timer is running (issue #100)
- [NEW] Callsign whisperer
- [NEW] Bandmap - Spot's color is recalculated when QSO is saved
- [NEW] BandMap - CTRL + Wheel zooming
- [NEW] BandMap - Zooming via buttons keeps a focus on centre freq
- [NEW] BandMap - DX Spot's Comment as a tooltip
- [CHANGED] BandMap - UI Layout
- Fixed MacOS builds (PR #102) (thx gerbert)
- Fixed templates under MacOS (PR #101) (thx gerbert)
- Fixed WindowsOS Installer - Unable to upgrade version

* Fri Jul 15 2022 Ladislav Foldyna - 0.12.0-1
- [NEW] Statistics - Show ODX on the map
- [EXPERIMENTAL] Support for QT Styles (issue #88)
- [CHANGED] - Removed F2 as a shortcut for QSO field editing
- Next fixing of a high CPU load when DXC is processed (issue #52)
- Fixed QSO fields from prev QSOs when Prefix - Callsign - Suffix (issue #90)
- Fixed Chaotic QSO start time (issue #93)
- Offline maps - Lighter colors, night sky removed, Sun position removed (issue #97)
- Fixed incorrect A-Index colort (issue #98)
- Fixed Stats Widget - percents - does not reflect date range (issue #99)
- Fixed potential LogParam Cache issue
- Import/Export polishing

* Sun Jun 26 2022 Ladislav Foldyna - 0.11.0-1
- [NEW] QSO Detail/Edit Dialog
- [NEW] Added mW power Support
- [NEW] Implemented ADIF 3.1.3
- [NEW] Rigwidget saves last used freq for bands
- Fixed Rig Combo size when Rig Profile name is long (issue #31)
- Fixed CQZ, ITUZ do not validate whether their entered value is a number (issue #75)
- Fixed vucc, myvucc must be uppercase - Edit mode (issue #76)
- Fixed Greyline-Map is very dark (issue #78)
- Fixed DX Country is not saved properly when name is between S-Z (issue #79)
- Fixed Bandmap call selection - only left mouse button (issue #82)
- Fixed My Notes Copy & Paste - Rich Text (issue #83)
- Fixed Font appearance in the context menu (issue #84)

* Sun Jun 5 2022 Ladislav Foldyna - 0.10.0-1
- [NEW] Bandmap shows XIT/RIT Freq
- [NEW] Bandmap RX Mark Center (issue #69)
- [NEW] Getting PTT State from RIG - only for CAT-controlled rigs
- [NEW] PTT Shortchut - only for CAT-controlled rigs
- Fixed Lost internet conneciton is not detected properly (issue #56)
- Fixed Cannot manually edit QSO Date&Time (issue #66)
- Fixed Field contents in capital letters (issue #67)
- Fixed Band RX is not updated when RX Freq is edited (issue #72)
- Fixed Stat Windget does not handle a date range correctly (issue #73)
- Fixed eQSL card is incorreclty handled when a callsign contains special characters (issue #74)

* Fri May 20 2022 Ladislav Foldyna - 0.9.0-1
- [NEW] User-defined Spot Alerts
- [NEW] User filter contains a new operator "Starts with"
- [NEW] a real local time is shown for the DX callsign (issue #45)
- [NEW] Lotw/eQSL registration info is showed from callbooks
- [NEW] Added shortcuts for menu and tabs
- [NEW] Bandmap - Switching a band view via Bandmap context menu (issue #57)
- [CHANGED] - Network Notification format
- Fixed issue with My Notes multiple lines edit/show mode (issue #39)
- Fixed issue when GUI froze when Rig disconnect was called (issue #50)
- Partially fixed a high CPU load when DXC is processed (issue #52)
- Fixed crashes under Debian "bullseye" - 32bit (issue #55)
- Fixed Bandmap Callsign selection margin (issue #61)
- Fixed issue when it was not possible to enter RX/TX freq manually

* Fri Apr 22 2022 Ladislav Foldyna - 0.8.0-1
- RIT/XIT offset enable/disable detection (issue #26)
- Fixed Rig Setting, Data Bits (issue #28)
- Added default PWR for Rig profile (issue #30)
- Fixed issue when GUI freezes during Rig connection (issue #32 & #33)
- Fixed issue with an incorrect value of A-Index (issue #34)
- Fixed ADI Import - incorrect _INTL fields import (issue #35)
- Fixed isuue with an editing of bands in Setting dialog (#issue 36)
- Fixed issue with hamlib when get_pwr crashes for a network rig (issue #37)
- Improved new QSO fields are filled from prev QSO (issue #40)
- Added mode for a network Rig (issue #41)
- Fixed warning - processing a new request but the previous one hasn't been completed (issue #42)
- Fixed Info widget when Country name is long (issue #43)
- Reordered column visibility Tabs (issue #46)
- Improved Rig tunning when XIT/RIT is enabled (issue #47)

* Fri Apr 8 2022 Ladislav Foldyna - 0.7.0-1
- [NEW] Ant/Rig/Rot Profiles
- [NEW] Rig widget shows additional information
- [NEW] Rig widget Band/Mode/Profile Changer
- [NEW] Rot profile Changer
- [NEW] AZ/EL are stored when Rot is connected
- Fixed an issue with Statistic widget (issue #25)
- Fixed Rot AZ current value (issue #22)

* Thu Mar 10 2022 Ladislav Foldyna - 0.6.5-1
- Fixed missing modes in Setting Dialog (issue #11)
- Fixed Station Profile text color in dark mode (issue #10)
- Fixed DXCluster Server Combo (issue #12)
- Fixed TAB focus on QSO Fields (issue #14)

* Sun Mar 6 2022 Ladislav Foldyna - 0.6.0-1
- [NEW] QSL - added import a file with QSL - QSLr column
- Fixed QLog start when Band is 3cm (too long start time due to the Bandmap drawing) (issue #6)
- Fixed Rotator Widget Warning - map transformation issue (issue #8)
- Changed Bandmap window narrow size (issue #3)
- Changed User Filter Widget size
- Removed Units from Logbook widget
- Removed UTC string
- Renamed RSTs, RSTr etc. (issue #4)
- Renamed Main Menu Services->Service and Station->Equipment
- Internal - reworked Service networking signal handling

* Sat Feb 19 2022 Ladislav Foldyna - 0.5.0-1
- DB: Started to use *_INTL fields
- DB: Added all ADIF-supported modes/submodes
- GUI: Dark Mode
- GUI: TIme format controlled by Locale
- Import/Export: ADI do not export UTF-8 characters and *_INTL fields
- Import/Export: ADX exports UTF-8 characters and *_INTL fields
- Import/Export: Added Import of ADX file format
- Logbook: Shows QSO summary as a Callsign's tooltip
- Logbook: QSO time is shown with seconds; added timezone
- New QSO: Added My notes - free text for your personal notes
- Backup: Change backup format form ADI to ADX (ADX supports UTF-8)
- Settings: WSJTX Port is changable

* Sun Jan 9 2022 Ladislav Foldyna - 0.4.0-1
- Stats: Added Show on Map - QSOs and Worked&Confirmed Grids
- Stats: Stats are refreshed after every QSO
- WSJTX: Remove TRX/Monitoring Status
- Added Split mode - RX/TX RIG Offset
- Added export of selected QSOs
- Fixed FLdigi interface
- CPPChecks & Clazy cleanup

* Sun Dec 19 2021 Ladislav Foldyna - 0.3.0-1
- Rework Station Profile - stored in DB, new fields
- Added VUCC fields support
- Added BandMap marks (CTRL+M)
- Clublog is uploaded the same way as EQSL and LOTW (modified QSO are resent)
- Clublog real-time upload is temporary disabled
- Added QRZ suppor - upload QSO and Callsign query
- Callbook cooperation - Primary&Secondary - Secondary used when Primary did not find

* Sat Nov 27 2021 Ladislav Foldyna - 0.2.0-1
- Initial version of the package based on v0.2.0
