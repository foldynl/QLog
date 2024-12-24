#ifndef QLOG_UI_LOOKBOOKWIDGET_H
#define QLOG_UI_LOOKBOOKWIDGET_H

#include <QWidget>
#include <QProxyStyle>
#include <QComboBox>
#include "models/SqlListModel.h"
#include "core/CallbookManager.h"
#include <QSqlRecord>

namespace Ui {
class LogbookWidget;
}

class ClubLog;
class LogbookModel;

class LogbookWidget : public QWidget {
    Q_OBJECT

public:
    explicit LogbookWidget(QWidget *parent = nullptr);
    ~LogbookWidget();

signals:
    void logbookUpdated();
    void contactUpdated(QSqlRecord&);
    void contactDeleted(const QSqlRecord&);
    void deletedEntities(const QSet<uint> &entities);
    void sendDXSpotContactReq(const QSqlRecord&);

    // Clublog special signals
    // unfortunately, special rules are applied for uploading to Clublog.
    // The Clublog's RT Interface only accepts low-rate QSO uploading.
    // Therefore, it is necessary to send only selected QSO manipulations.
    // That is why these 2 special signals are emitted.
    // contactUpdated, contactDeleted signals are also emitted in these cases
    void clublogContactUpdated(QSqlRecord&);
    void clublogContactDeleted(const QSqlRecord&);

public slots:
    void filterCallsign(const QString &call);
    void filterSelectedCallsign();
    void filterCountryBand(const QString&, const QString&, const QString&);
    void lookupSelectedCallsign();
    void callsignFilterChanged();
    void bandFilterChanged();
    void modeFilterChanged();
    void countryFilterChanged();
    void userFilterChanged();
    void setUserFilter(const QString &filterName);
    void clubFilterChanged();
    void refreshClubFilter();
    void refreshUserFilter();
    void restoreFilters();
    void updateTable();
    void uploadClublog();
    void deleteContact();
    void exportContact();
    void editContact();
    void displayedColumns();
    void saveTableHeaderState();
    void showTableHeaderContextMenu(const QPoint& point);
    void doubleClickColumn(QModelIndex);
    void handleBeforeUpdate(int, QSqlRecord&);
    void handleBeforeDelete(int);
    void focusSearchCallsign();
    void reloadSetting();
    void sendDXCSpot();
    void actionCallbookLookup();
    void callsignFound(const QMap<QString, QString>& data);
    void callsignNotFound(const QString&);
    void callbookLoginFailed(const QString&);
    void callbookError(const QString&);
    void updateQSOsCallbook(QList<QSqlRecord> qsos);
    void updateQSOCallbook(QSqlRecord qso);

private:
    ClubLog* clublog;
    LogbookModel* model;
    Ui::LogbookWidget *ui;
    SqlListModel* countryModel;
    SqlListModel* userFilterModel;
    QString externalFilter;
    bool blockClublogSignals;
    bool eventFilter(QObject *obj, QEvent *event);

    void colorsFilterWidget(QComboBox *widget);
    void filterTable();
    void saveBandFilter();
    void restoreBandFilter();
    void saveModeFilter();
    void restoreModeFilter();
    void saveCountryFilter();
    void restoreCountryFilter();
    void saveUserFilter();
    void restoreUserFilter();
    void saveClubFilter();
    void restoreClubFilter();
    void reselectModel();
    void scrollToIndex(const QModelIndex& index, bool select = true);
    void adjusteComboMinSize(QComboBox * combo);
    void UpdateQSORecordFromCallbook(QSqlRecord qso, const QMap<QString, QString>& data);
    QList<QSqlRecord> TempQSOsCallbookLookup;
    QSqlRecord TempQSOCallbookLookup;
    CallbookManager callbookManager;
};

/* https://forum.qt.io/topic/90403/show-tooltip-immediatly/7/ */
class ProxyStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr, QStyleHintReturn* returnData = nullptr) const override
    {
        if (hint == QStyle::SH_ToolTip_WakeUpDelay)
            return 0;  // show tooltip immediately
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

#endif // QLOG_UI_LOGBOOKWIDGET_H
