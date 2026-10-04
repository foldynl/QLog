#include "SpotCommentDialog.h"
#include "ui_SpotCommentDialog.h"
#include <QMessageBox>
#include <QStyle>
#include <QToolTip>
#include "core/LogParam.h"
#include "core/MacroExpander.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.spotcommentdialog");

SpotCommentDialog::SpotCommentDialog(const MacroExpander::Context &context,
                                     bool currentQSO,
                                     QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SpotCommentDialog),
    context(context),
    previewTitle(currentQSO ? tr("Test Macro — New QSO") : tr("Test Macro — Last QSO"))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    ui->templateEdit->setText(LogParam::getDXCSpotCommentTemplate());
    ui->templateEdit->setToolTip(MacroExpander::helpText());
}

SpotCommentDialog::~SpotCommentDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void SpotCommentDialog::accept()
{
    FCT_IDENTIFICATION;

    if ( !LogParam::setDXCSpotCommentTemplate(ui->templateEdit->text()) )
    {
        showWarning(ui->buttonBox->button(QDialogButtonBox::Ok),
                    tr("The spot comment template could not be saved."));
        return;
    }
    clearWarning();
    QDialog::accept();
}

void SpotCommentDialog::testMacro()
{
    FCT_IDENTIFICATION;

    const MacroExpander::Result result = MacroExpander::expandSpotComment(ui->templateEdit->text(), context);
    const QString warning = context.value(QStringLiteral("callsign")).trimmed().isEmpty()
                            ? tr("No QSO is available. Fill in a callsign in New QSO or log a QSO first.")
                            : (result.missingMacros.isEmpty() ? QString()
                                                              : tr("Missing QSO data for: %1.").arg(result.missingMacros.join(QStringLiteral(", "))));

    if ( !warning.isEmpty() )
    {
        showWarning(ui->testButton, warning);
        return;
    }

    clearWarning();

    QMessageBox preview(QMessageBox::Information, previewTitle,
                       result.text.isEmpty() ? tr("Empty comment.") : result.text,
                       QMessageBox::Ok, this);

    preview.setTextFormat(Qt::PlainText);
    preview.exec();
}

void SpotCommentDialog::showWarning(QPushButton *button, const QString &warning)
{
    FCT_IDENTIFICATION;

    clearWarning();
    const QString tooltip = QStringLiteral("<qt>%1</qt>").arg(warning.toHtmlEscaped());
    button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    button->setToolTip(tooltip);
    QToolTip::showText(button->mapToGlobal(QPoint(0, button->height())),
                       tooltip,
                       button,
                       QRect(),
                       5000);
}

void SpotCommentDialog::clearWarning()
{
    FCT_IDENTIFICATION;

    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);

    for ( QPushButton *button : {ui->testButton, okButton} )
        if ( QToolTip::text() == button->toolTip() )
            QToolTip::hideText();

    ui->testButton->setIcon(QIcon());
    QStyle *buttonStyle = ui->buttonBox->style();
    okButton->setIcon(buttonStyle->styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons, nullptr, ui->buttonBox)
                      ? buttonStyle->standardIcon(QStyle::SP_DialogOkButton, nullptr, ui->buttonBox)
                      : QIcon());
    okButton->setToolTip(QString());
    ui->testButton->setToolTip(tr("Use New QSO when its callsign is filled in; otherwise use the last saved QSO. Nothing is saved or sent."));
}
