#ifndef QLOG_UI_QSLPRINTLABELDIALOG_H
#define QLOG_UI_QSLPRINTLABELDIALOG_H

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QPixmap>
#include <QScopedPointer>
#include <QTimer>
#include <QVariantMap>

#include "core/LogLocale.h"
#include "core/QSLLabelBuilder.h"
#include "core/QSLPrintLabelRenderer.h"

class QSqlQuery;
class QToolButton;

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
    void filterChanged();
    void refreshData();
    void refreshLabels();
    void refreshPreview();
    void prevPage();
    void nextPage();
    void print();
    void exportPdf();
    void exportCardImages();
    void templateChanged(int index);
    void skipChanged(int value);
    void zoomChanged(int value);
    void customTemplateFieldChanged();
    void outputPageSizeChanged();
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
    enum class UpdateLevel
    {
        None = 0,
        Preview,
        Labels,
        Data
    };

    struct ContactFilter
    {
        QString whereClause;
        QVariantMap bindings;

        void bind(QSqlQuery &query) const;
    };

    Ui::QSLPrintLabelDialog *ui;
    QScopedPointer<QSLLabelBuilder> labelBuilder;
    LogLocale locale;
    QSLPrintLabelRenderer renderer;
    QList<QSLLabelData> labelsData;
    QImage cardBackgroundImageData;
    QPixmap previewPagePixmap;
    QColor labelTextColor;
    QColor cardLabelBackgroundColor;
    int currentPage = 0;
    int zoomPercent = 100;
    QTimer refreshTimer;
    UpdateLevel pendingUpdate = UpdateLevel::None;

    void loadData();
    void rebuildLabels();
    void scheduleUpdate(UpdateLevel level);
    void applyPendingUpdate();
    void showRefreshState(const QString &message);
    void renderCurrentPage();
    void scalePreview();
    void refreshPreviewFromFirstPage();
    ContactFilter currentFilter() const;
    void askAndMarkQslSent();
    void updatePageNavigation();
    void saveSettings();
    void loadSettings();
    void populateTemplateFields(const LabelTemplate &tmpl);
    void setTemplateFieldsEnabled(bool enabled);
    LabelTemplate buildCustomTemplate() const;
    LabelTemplate currentLabelTemplate() const;
    QSLPrintMode currentPrintMode() const;
    QSLLabelOrder currentLabelOrder() const;
    QPageSize::PageSizeId currentOutputPageSizeId() const;
    QPageSize currentOutputPageSize() const;
    QSLCardLayout buildCardLayout() const;
    LabelStyleOptions buildStyleOptions() const;
    QString buttonContrastTextColor(const QColor &backgroundColor) const;
    QString imageExportFileName(const QSLLabelData &label, int index) const;
    void updateRendererOptions();
    void updateOutputPageSizeUi();
    void updatePrintModeUi();
    void updateLabelTextColorUi();
    void updateCardLabelBackgroundColorUi();
    void updateCardBackgroundUi();
    void updateFilterUi();
    void setupCollapsibleSection(QToolButton *button, QWidget *content);
    void populateExtraColumnCombo();
    void populateQSLSentCombo();
};

#endif // QLOG_UI_QSLPRINTLABELDIALOG_H
