#ifndef MULTIQSOUPLOAD_H
#define MULTIQSOUPLOAD_H

#include <QDialog>
#include "core/LogLocale.h"

namespace Ui {
class MultiQSOUpload;
}

class MultiQSOUpload : public QDialog
{
    Q_OBJECT

public:
    explicit MultiQSOUpload(QWidget *parent = nullptr);
    ~MultiQSOUpload();

public slots:
    void upload();

private:
    Ui::MultiQSOUpload *ui;
    void saveDialogState();
    void loadDialogState();
    void LoTWUpload();
    void EQSLupload();
    void ClubLogUpload();
    void HRDLogUpload();
    void QRZUpload();

    LogLocale locale;
};

#endif // MULTIQSOUPLOAD_H
