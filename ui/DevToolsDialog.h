#ifndef QLOG_UI_DEVTOOLSDIALOG_H
#define QLOG_UI_DEVTOOLSDIALOG_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QSortFilterProxyModel>

namespace Ui {
class DevToolsDialog;
}

class SqlHighlighter;

class DevToolsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DevToolsDialog(QWidget *parent = nullptr);
    ~DevToolsDialog();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void openQuery();
    void saveQuery();
    void runQuery();
    void exportAsTxt();
    void exportAsCsv();
    void exportAsAdif();
    void logToFileToggled(bool checked);
    void applyLoggingRules();
    void saveDebugLog();

private:
    static const QString READ_ONLY_CONNECTION;
    static const int MAX_FETCH_ROWS = 50000;

    Ui::DevToolsDialog *ui;
    SqlHighlighter     *highlighter;
    QSqlQueryModel     *queryModel;
    QSortFilterProxyModel *sortProxy;

    void loadSchema();
    void updateDebugLogFileLabel();
    void exportModel(const QString &title,
                     const QString &filter,
                     const QString &defaultExt,
                     const QString &separator,
                     std::function<QString(const QString&)> formatter);
};

#endif // QLOG_UI_DEVTOOLSDIALOG_H
