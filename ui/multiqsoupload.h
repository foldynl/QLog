#ifndef MULTIQSOUPLOAD_H
#define MULTIQSOUPLOAD_H

#include <QDialog>

namespace Ui {
class MultiQSOUpload;
}

class MultiQSOUpload : public QDialog
{
    Q_OBJECT

public:
    explicit MultiQSOUpload(QWidget *parent = nullptr);
    ~MultiQSOUpload();

private slots:
    void on_uploadButton_clicked();

    void on_cancelButton_clicked();

private:
    Ui::MultiQSOUpload *ui;
    void saveDialogState();
    void loadDialogState();

    LogLocale locale;
};

#endif // MULTIQSOUPLOAD_H
