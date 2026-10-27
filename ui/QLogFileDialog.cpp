#include "QLogFileDialog.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.qlogfiledialog");

QString QLogFileDialog::getSaveFileName(QWidget *parent, const QString &caption,
                                       const QString &initialPath, const QString &filter,
                                       const QString &suffix, bool forceSuffix)
{
    FCT_IDENTIFICATION;

    QString suggestedPath = initialPath;
    while ( true )
    {
        // Qt's defaultSuffix can change the URI after the portal grants access.
        const QString selected = QFileDialog::getSaveFileName(parent, caption, suggestedPath, filter);
        if ( selected.isEmpty() )
            return QString();

        const bool needsSuffix = forceSuffix ? !selected.endsWith('.' + suffix, Qt::CaseInsensitive)
                                              : !QFileInfo(selected).fileName().contains('.');
        if ( suffix.isEmpty() || !needsSuffix )
            return selected;

        // A different basename may only create a temporary document-portal file.
        suggestedPath = selected + '.' + suffix;
        QMessageBox::information(parent, caption,
                                 tr("Confirm the complete file name, including .%1, in the save dialog.").arg(suffix));
    }
}
