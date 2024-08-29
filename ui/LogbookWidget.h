#ifndef QLOG_UI_LOOKBOOKWIDGET_H
#define QLOG_UI_LOOKBOOKWIDGET_H

#include <QWidget>
#include <QProxyStyle>
#include <QComboBox>
#include "models/SqlListModel.h"

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
    void saveBandFilter();
    void restoreBandFilter();
    void modeFilterChanged();
    void saveModeFilter();
    void restoreModeFilter();
    void countryFilterChanged();
    void saveCountryFilter();
    void restoreCountryFilter();
    void userFilterChanged();
    void saveUserFilter();
    void restoreUserFilter();
    void clubFilterChanged();
    void refreshClubFilter();
    void saveClubFilter();
    void restoreclubFilter();
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

private:
    ClubLog* clublog;
    LogbookModel* model;
    Ui::LogbookWidget *ui;
    SqlListModel* countryModel;
    SqlListModel* userFilterModel;
    QString externalFilter;
    bool blockClublogSignals;

    void colorsFilterWidget(QComboBox *widget);
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
