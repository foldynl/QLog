#ifndef QLOG_UI_SPOTCOMMENTDIALOG_H
#define QLOG_UI_SPOTCOMMENTDIALOG_H

#include <QDialog>
#include "core/MacroExpander.h"

namespace Ui
{
class SpotCommentDialog;
}

class QPushButton;

class SpotCommentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpotCommentDialog(const MacroExpander::Context &context, bool currentQSO,
                               QWidget *parent = nullptr);
    ~SpotCommentDialog() override;

public slots:
    void accept() override;

private slots:
    void testMacro();
    void clearWarning();

private:
    void showWarning(QPushButton *button, const QString &warning);

    Ui::SpotCommentDialog *ui;
    const MacroExpander::Context context;
    const QString previewTitle;
};

#endif // QLOG_UI_SPOTCOMMENTDIALOG_H
