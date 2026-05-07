#ifndef QLOG_UI_DXCCSUBMISSIONDIALOG_H
#define QLOG_UI_DXCCSUBMISSIONDIALOG_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QCheckBox>
#include <QList>
#include <QSqlRecord>
#include "models/SqlListModel.h"
#include "data/Band.h"

namespace Ui {
class DXCCSubmissionDialog;
}


class DXCCSubmissionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DXCCSubmissionDialog(QWidget *parent = nullptr);
    ~DXCCSubmissionDialog();

    // DXCC band scope presets
    enum class DXCCBandScope
    {
        EntityLevel,   // Any band — one entry per entity (basic DXCC)
        FiveBand,      // 80/40/20/15/10m preset
        AllDXCCBands,  // All enabled DXCC bands, per band
        Custom         // User-selected bands
    };

public slots:
    void refreshTable();
    void onBandScopeChanged(int index);
    void onFiveBandClicked();
    void onAllBandsClicked();
    void exportAsADIF();

private:
    Ui::DXCCSubmissionDialog *ui;
    QSqlQueryModel *tableModel;
    SqlListModel   *entityCallsignModel;

    // Band checkboxes added dynamically
    QList<QCheckBox*> bandCheckBoxes;
    QList<Band>       dxccBands;
    const quint8 MAXCOLUMNS = 14;

    // Helpers
    const QString getSelectedEntity() const;
    QStringList   getSelectedBands(DXCCBandScope scope) const;
    QString       buildModeGroupFilter() const;
    void          selectBandPreset(const QStringList &bands);
    void          updateStatusLabel(int count,
                                    const QStringList &selectedBands,
                                    DXCCBandScope scope);
    void          setBandControlsVisible(bool visible);
    DXCCBandScope currentScope() const;
    void          clearTable();
    const QStringList FIVE_BAND_DXCC = { "80m", "40m", "20m", "15m", "10m" };
};

#endif // QLOG_UI_DXCCSUBMISSIONDIALOG_H
