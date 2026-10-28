#ifndef QLOG_UI_QLOGFILEDIALOG_H
#define QLOG_UI_QLOGFILEDIALOG_H

#include <QCoreApplication>
#include <QString>
#include <QMessageBox>

class QWidget;

class QLogFileDialog
{
    Q_DECLARE_TR_FUNCTIONS(QLogFileDialog)

public:
    static void showError(QWidget *parent, const QString &title, const QString &message,
                          const QString &details = QString(), QMessageBox::Icon icon = QMessageBox::Warning);
    static QString getSaveFileName(QWidget *parent, const QString &caption,
                                   const QString &initialPath, const QString &filter,
                                   const QString &suffix, bool forceSuffix = false);
};

#endif // QLOG_UI_QLOGFILEDIALOG_H
