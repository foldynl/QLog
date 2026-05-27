#ifndef QLOG_UI_QSLPRINTLABELDIALOG_H
#define QLOG_UI_QSLPRINTLABELDIALOG_H

#include <QColor>
#include <QDialog>
#include <QImage>

#include "core/LogLocale.h"
#include "core/QSLPrintLabelRenderer.h"

class QSqlQuery;

namespace Ui {
class QSLPrintLabelDialog;
}

class QSLPrintLabelDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QSLPrintLabelDialog(QWidget *parent = nullptr);
    ~QSLPrintLabelDialog();

private slots:
    void toggleDateRange();
    void toggleMyCallsign();
    void toggleStationProfile();
    void toggleQslSent();
    void toggleUserFilter();
    void refreshData();
    void updatePreview();
    void prevPage();
    void nextPage();
    void print();
    void exportPdf();
    void exportCardImages();
    void templateChanged(int index);
    void skipChanged(int value);
    void zoomChanged(int value);
    void customTemplateFieldChanged();
    void printModeChanged(int index);
    void cardLayoutChanged();
    void selectLabelTextColor();
    void selectCardLabelBackgroundColor();
    void selectCardBackgroundImage();
    void clearCardBackgroundImage();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::QSLPrintLabelDialog *ui;
    LogLocale locale;
    QSLPrintLabelRenderer renderer;
    QList<QSLLabelData> labelsData;
    QImage cardBackgroundImageData;
    QColor labelTextColor;
    QColor cardLabelBackgroundColor;
    int currentPage = 0;
    int zoomPercent = 100;

    void buildLabels();
    QString buildWhereClause() const;
    void bindWhereClause(QSqlQuery &query) const;
    void askAndMarkQslSent();
    void updatePageNavigation();
    void saveSettings();
    void loadSettings();
    void populateTemplateFields(const LabelTemplate &tmpl);
    void setTemplateFieldsEnabled(bool enabled);
    LabelTemplate buildCustomTemplate() const;
    LabelTemplate currentLabelTemplate() const;
    QSLPrintMode currentPrintMode() const;
    QPageSize::PageSizeId currentOutputPageSize() const;
    QSLCardLayout buildCardLayout() const;
    LabelStyleOptions buildStyleOptions() const;
    QString buttonContrastTextColor(const QColor &backgroundColor) const;
    QString imageExportFileName(const QSLLabelData &label, int index) const;
    void updateRendererOptions();
    void updatePrintModeUi();
    void updateLabelTextColorUi();
    void updateCardLabelBackgroundColorUi();
    void updateCardBackgroundUi();
    void populateExtraColumnCombo();
    void populateQSLSentCombo();
};

#endif // QLOG_UI_QSLPRINTLABELDIALOG_H
