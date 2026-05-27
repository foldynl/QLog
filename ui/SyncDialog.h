#ifndef QLOG_UI_SYNCDIALOG_H
#define QLOG_UI_SYNCDIALOG_H

#include <QDialog>

namespace Ui {
class SyncDialog;
}

class SyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncDialog(QWidget *parent = nullptr);
    ~SyncDialog();

private slots:
    void onEnabledToggled(bool checked);
    void onBrowseClicked();
    void onFolderEdited();
    void onSaveNodeIdClicked();
    void onDupePolicyChanged(int index);
    void onFlushNowClicked();
    void onPullNowClicked();
    void refreshStatus();

private:
    void applyEnabledUiState();

    Ui::SyncDialog *ui;
};

#endif // QLOG_UI_SYNCDIALOG_H
