#ifndef QLOG_UI_QSLPRINTLABELDIALOG_H
#define QLOG_UI_QSLPRINTLABELDIALOG_H

#include <QDialog>

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
    void templateChanged(int index);
    void skipChanged(int value);
    void zoomChanged(int value);
    void customTemplateFieldChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::QSLPrintLabelDialog *ui;
    LogLocale locale;
    QSLPrintLabelRenderer renderer;
    QList<QSLLabelData> labelsData;
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
    void populateExtraColumnCombo();
    void populateQSLSentCombo();
};

#endif // QLOG_UI_QSLPRINTLABELDIALOG_H
