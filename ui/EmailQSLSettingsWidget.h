#ifndef QLOG_UI_EMAILQSLSETTINGSWIDGET_H
#define QLOG_UI_EMAILQSLSETTINGSWIDGET_H

#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QSqlRecord>
#include <QStandardItemModel>

#include "service/emailqsl/EmailQSLService.h"

namespace Ui {
class EmailQSLSettingsWidget;
}

class EmailQSLSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EmailQSLSettingsWidget(QWidget *parent = nullptr);
    ~EmailQSLSettingsWidget();

    void readSettings();
    void writeSettings();

    // Render card using the current UI state (not saved QSettings).
    // Used by the preview dialog so the user can see results before saving.
    QPixmap renderPreviewPixmap(const QSqlRecord &record);

public slots:
    void browseCardImage();
    void onCardImageChanged(const QString &path);
    void onSmtpPresetChanged(int index);
    void testConnection();
    void onTestFinished(bool success, const QString &message);
    void addOverlay();
    void addBox();
    void addLabel();
    void addDefaultOverlays();
    void removeOverlay();
    void insertMergeFieldBody();
    void insertMergeFieldSubject();
    void previewCard();

    // Bidirectional sync between the overlay table and the card editor widget
    void onTableDataChanged();
    void onEditorPositionChanged(int index, int newX, int newY);
    void onEditorSizeChanged(int index, int newW, int newH);
    void onEditorOverlaySelected(int index);

private:
    void overlayTableToList();
    void listToOverlayTable();

    Ui::EmailQSLSettingsWidget *ui;
    QList<EmailQSLFieldOverlay> m_overlays;
    QStandardItemModel         *m_overlayModel;
    EmailQSLService            *m_testService;
    bool                        m_syncing;
};

#endif // QLOG_UI_EMAILQSLSETTINGSWIDGET_H
