#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include <QDialog>
#include <QSqlRecord>
#include <logformat/LogFormat.h>
#include "core/LogLocale.h"

namespace Ui {
class ImportDialog;
}

class ImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportDialog(QWidget *parent = 0);
    ~ImportDialog();

public slots:
    void browse();
    void toggleAll();
    void toggleMyGrid();
    void toggleMyRig();
    void toggleComment();
    void adjustLocatorTextColor();
    void runImport();
    void computeProgress(qint64 position);

private:
    Ui::ImportDialog *ui;
    qint64 size;
    LogLocale locale;

    static LogFormat::duplicateQSOBehaviour showDuplicateDialog(QSqlRecord *, QSqlRecord *);
};

#endif // IMPORTDIALOG_H
