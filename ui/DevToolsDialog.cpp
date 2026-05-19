#include "DevToolsDialog.h"
#include "ui_DevToolsDialog.h"
#include "ui/component/SqlHighlighter.h"
#include "ui/ExportDialog.h"
#include "core/LogDatabase.h"
#include "core/debug.h"

#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>

MODULE_IDENTIFICATION("qlog.ui.devtoolsdialog");

const QString DevToolsDialog::READ_ONLY_CONNECTION("DevToolsDialog_readonly");

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DevToolsDialog::DevToolsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::DevToolsDialog),
      highlighter(nullptr),
      queryModel(new QSqlQueryModel(this)),
      sortProxy(new QSortFilterProxyModel(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    // Open a separate read-only connection to the database.
    // SQLite itself enforces read-only access - no keyword blocklist needed.
    {
        QSqlDatabase roDb = QSqlDatabase::addDatabase("QSQLITE", READ_ONLY_CONNECTION);
        roDb.setDatabaseName(LogDatabase::dbFilename());
        roDb.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_ENABLE_REGEXP");
        if ( !roDb.open() )
            qCWarning(runtime) << "Cannot open read-only DB connection:" << roDb.lastError().text();
    }

    // Restore geometry & splitter state
    QSettings settings;
    restoreGeometry(settings.value("devtools/geometry").toByteArray());
    ui->splitter->restoreState(settings.value("devtools/splitter").toByteArray());
    if ( ui->splitter->sizes().value(0, 0) == 0 )
    {
        // First run - default split
        ui->splitter->setSizes({250, 450});
    }

    // Monospace font for the editor
    QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editorFont.setPointSize(10);
    ui->sqlEditor->setFont(editorFont);

    // Syntax highlighter
    highlighter = new SqlHighlighter(ui->sqlEditor->document());

    // Results model + sortable proxy
    sortProxy->setSourceModel(queryModel);
    ui->resultsTable->setModel(sortProxy);
    ui->resultsTable->horizontalHeader()->setStretchLastSection(true);
    ui->resultsTable->verticalHeader()->setVisible(false);

    // Export drop-down menu
    QMenu *exportMenu = new QMenu(this);
    connect(exportMenu->addAction(tr("TXT")), &QAction::triggered,
            this, &DevToolsDialog::exportAsTxt);
    connect(exportMenu->addAction(tr("CSV")), &QAction::triggered,
            this, &DevToolsDialog::exportAsCsv);
    connect(exportMenu->addAction(tr("ADI")), &QAction::triggered,
            this, &DevToolsDialog::exportAsAdif);
    ui->exportButton->setMenu(exportMenu);

    ui->sqlEditor->installEventFilter(this);

    // Load db schema into highlighter
    loadSchema();

    // Debug log controls
    ui->logToFileCheckBox->setChecked(isLogToFileEnabled());
    updateDebugLogFileLabel();
}

DevToolsDialog::~DevToolsDialog()
{
    QSettings settings;
    settings.setValue("devtools/geometry",  saveGeometry());
    settings.setValue("devtools/splitter",  ui->splitter->saveState());
    delete ui;

    // Must be removed after all QSql* objects using it are destroyed
    QSqlDatabase::removeDatabase(READ_ONLY_CONNECTION);
}

// ---------------------------------------------------------------------------
// Schema loading
// ---------------------------------------------------------------------------

void DevToolsDialog::loadSchema()
{
    FCT_IDENTIFICATION;

    QStringList schemaIds;
    QSqlDatabase roDb = QSqlDatabase::database(READ_ONLY_CONNECTION);

    QSqlQuery q(roDb);
    if ( q.exec("SELECT name FROM sqlite_master WHERE type IN ('table','view') ORDER BY name") )
    {
        while ( q.next() )
        {
            const QString tableName = q.value(0).toString();
            schemaIds.append(tableName);

            QSqlQuery colQ(roDb);
            if ( colQ.exec(QString("PRAGMA table_info(\"%1\")").arg(tableName)) )
            {
                while ( colQ.next() )
                {
                    const QString col = colQ.value(1).toString();
                    if ( !schemaIds.contains(col, Qt::CaseInsensitive) )
                        schemaIds.append(col);
                }
            }
            else
            {
                qCWarning(runtime) << "PRAGMA table_info failed for" << tableName
                                   << colQ.lastError().text();
            }
        }
    }
    else
    {
        qCWarning(runtime) << "Failed to query sqlite_master:" << q.lastError().text();
    }

    // Highlight schema names (tables, columns) in a distinct color
    highlighter->setUserIdentifiers(schemaIds);
}

// ---------------------------------------------------------------------------
// Event filter - keyboard shortcuts
// ---------------------------------------------------------------------------

bool DevToolsDialog::eventFilter(QObject *obj, QEvent *event)
{
    FCT_IDENTIFICATION;

    if ( obj != ui->sqlEditor || event->type() != QEvent::KeyPress )
        return QDialog::eventFilter(obj, event);

    QKeyEvent *ke = static_cast<QKeyEvent *>(event);

    // Ctrl+Return - run query
    if ( ke->key() == Qt::Key_Return
         && ( ke->modifiers() & Qt::ControlModifier ) )
    {
        runQuery();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void DevToolsDialog::openQuery()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    const QString lastDir = settings.value("devtools/lastDir", QDir::homePath()).toString();

    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open SQL Query"), lastDir,
        tr("SQL Files (*.sql);;All Files (*)"));

    if ( filename.isEmpty() )
        return;

    QFile file(filename);
    if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) )
    {
        QMessageBox::warning(this, tr("Open Error"),
            tr("Cannot open file:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    ui->sqlEditor->setPlainText(in.readAll());
    settings.setValue("devtools/lastDir", QFileInfo(filename).absolutePath());
}

void DevToolsDialog::saveQuery()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    const QString lastDir = settings.value("devtools/lastDir", QDir::homePath()).toString();

    QString filename = QFileDialog::getSaveFileName(
        this, tr("Save SQL Query"), lastDir,
        tr("SQL Files (*.sql);;All Files (*)"));

    if ( filename.isEmpty() )
        return;

    if ( !filename.endsWith(".sql", Qt::CaseInsensitive) )
        filename += ".sql";

    QFile file(filename);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Text) )
    {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Cannot save file:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << ui->sqlEditor->toPlainText();
    settings.setValue("devtools/lastDir", QFileInfo(filename).absolutePath());
    ui->statusLabel->setText(tr("Query saved to %1").arg(QFileInfo(filename).fileName()));
}

// ---------------------------------------------------------------------------
// Run query
// ---------------------------------------------------------------------------

void DevToolsDialog::runQuery()
{
    FCT_IDENTIFICATION;

    const QString sql = ui->sqlEditor->toPlainText().trimmed();

    if ( sql.isEmpty() )
        return;

    QElapsedTimer timer;
    timer.start();

    // Execute via the read-only connection; SQLite rejects any write attempt
    queryModel->setQuery(sql, QSqlDatabase::database(READ_ONLY_CONNECTION));

    if ( queryModel->lastError().isValid() )
    {
        ui->statusLabel->setText(
            tr("Error: %1").arg(queryModel->lastError().text()));
        ui->statusLabel->setStyleSheet("QLabel { color : red; }");
        return;
    }
    ui->statusLabel->setStyleSheet("");

    // Fetch rows in batches; cap at a reasonable limit to avoid blocking UI
    while ( queryModel->canFetchMore() && queryModel->rowCount() < MAX_FETCH_ROWS )
        queryModel->fetchMore();

    const qint64 elapsed = timer.elapsed();
    const int rows = queryModel->rowCount();
    const bool truncated = queryModel->canFetchMore();

    ui->resultsTable->resizeColumnsToContents();
    if ( truncated )
        ui->statusLabel->setText(
            tr("%1 row(s) shown (truncated at %2) in %3 ms")
            .arg(rows).arg(MAX_FETCH_ROWS).arg(elapsed));
    else
        ui->statusLabel->setText(
            tr("%1 row(s) returned in %2 ms").arg(rows).arg(elapsed));
}

// ---------------------------------------------------------------------------
// Export helpers
// ---------------------------------------------------------------------------

static bool openFileForWrite(QWidget *parent,
                             const QString &caption,
                             const QString &filter,
                             const QString &defaultSuffix,
                             QString &outPath)
{
    QSettings settings;
    const QString lastDir = settings.value("devtools/lastDir", QDir::homePath()).toString();

    QString path = QFileDialog::getSaveFileName(parent, caption, lastDir, filter);

    if ( path.isEmpty() )
        return false;

    if ( !defaultSuffix.isEmpty()
         && !path.endsWith('.' + defaultSuffix, Qt::CaseInsensitive) )
        path += '.' + defaultSuffix;

    settings.setValue("devtools/lastDir", QFileInfo(path).absolutePath());
    outPath = path;
    return true;
}

void DevToolsDialog::exportModel( const QString &title,
                                  const QString &filter,
                                  const QString &defaultExt,
                                  const QString &separator,
                                  std::function<QString(const QString&)> formatter)
{
    FCT_IDENTIFICATION;

    if ( queryModel->rowCount() == 0 )
    {
        QMessageBox::information(this, tr("Export"),
                                 tr("No results to export."));
        return;
    }

    QString filename;
    if ( !openFileForWrite(this, title, filter, defaultExt, filename) )
        return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Export Error"),
                             tr("Cannot open file for writing:\n%1")
                             .arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    const int rows = queryModel->rowCount();
    const int cols = queryModel->columnCount();

    std::function<void(std::function<QString(int)>)>
            writeRow = [&](std::function<QString(int)> dataProvider)
    {
        QStringList row;
        row.reserve(cols);

        for (int c = 0; c < cols; ++c)
            row << formatter(dataProvider(c));

        out << row.join(separator) << "\n";
    };

    // header
    writeRow([&](int c)
    {
        return queryModel->headerData(c, Qt::Horizontal).toString();
    });

    // data
    for ( int r = 0; r < rows; ++r )
    {
        writeRow([&](int c)
        {
            return queryModel->data(queryModel->index(r, c)).toString();
        });
    }

    ui->statusLabel->setText(tr("Exported %1 row(s) to %2").arg(rows)
                                                           .arg(QFileInfo(filename).fileName()));
}

void DevToolsDialog::exportAsTxt()
{
    FCT_IDENTIFICATION;

    exportModel(tr("TXT"),
                tr("Text Files (*.txt);;All Files (*)"),
                "txt",
                "\t",
                [](const QString &s) { return s; });
}

void DevToolsDialog::exportAsCsv()
{
    FCT_IDENTIFICATION;

    auto csvEscape = [](const QString &s) -> QString
    {
        if (s.contains(',') || s.contains('"') || s.contains('\n') || s.contains('\r'))
            return '"' + QString(s).replace('"', "\"\"") + '"';
        return s;
    };

    exportModel(tr("CSV"),
                tr("CSV Files (*.csv);;All Files (*)"),
                "csv",
                ",",
                csvEscape);
}

void DevToolsDialog::exportAsAdif()
{
    FCT_IDENTIFICATION;

    if ( queryModel->rowCount() == 0 )
    {
        QMessageBox::information(this, tr("Export"),
            tr("No results to export."));
        return;
    }

    // Locate an 'id' column in the result set
    int idCol = -1;
    for ( int c = 0; c < queryModel->columnCount(); ++c )
    {
        if ( queryModel->headerData(c, Qt::Horizontal)
                .toString().compare("id", Qt::CaseInsensitive) == 0 )
        {
            idCol = c;
            break;
        }
    }

    if ( idCol < 0 )
    {
        QMessageBox::warning(this, tr("ADIF Export"),
            tr("ADIF export requires the query to include the contacts 'id' column.\n\n"
               "Example:\n"
               "  SELECT id, callsign FROM contacts WHERE ..."));
        return;
    }

    // Collect all contact IDs from the result (validated as numbers)
    QList<qlonglong> ids;
    for ( int r = 0; r < queryModel->rowCount(); ++r )
    {
        const QVariant id = queryModel->data(queryModel->index(r, idCol));
        bool ok = false;
        const qlonglong numId = id.toLongLong(&ok);
        if ( ok )
            ids << numId;
    }

    if ( ids.isEmpty() )
    {
        QMessageBox::information(this, tr("ADIF Export"),
            tr("No valid contact IDs found in the result set."));
        return;
    }

    // Build parameterized placeholders for the ID list
    QStringList placeholders;
    for ( int i = 0; i < ids.size(); ++i )
        placeholders << "?";

    QSqlQuery fetchQ;
    if ( ! fetchQ.prepare(QString("SELECT * FROM contacts WHERE id IN (%1) ORDER BY start_time ASC").arg(placeholders.join(','))))
    {
        qWarning() << "Cannot prepare select statement for contacts table";
        return;
    }
    for ( int i = 0; i < ids.size(); ++i )
        fetchQ.addBindValue(ids.at(i));

    if ( !fetchQ.exec() )
    {
        QMessageBox::warning(this, tr("ADIF Export"),
            tr("Failed to retrieve contact records:\n%1")
            .arg(fetchQ.lastError().text()));
        return;
    }

    QList<QSqlRecord> records;
    while ( fetchQ.next() )
        records.append(fetchQ.record());

    if ( records.isEmpty() )
    {
        QMessageBox::information(this, tr("ADIF Export"),
            tr("No matching contacts found in the database."));
        return;
    }

    // Hand off to the standard ExportDialog (same path as LogbookWidget right-click)
    ExportDialog dialog(records, this);
    dialog.exec();
}

// ---------------------------------------------------------------------------
// Debug log controls
// ---------------------------------------------------------------------------

void DevToolsDialog::logToFileToggled(bool checked)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << checked;

    if ( !checked )
    {
        // Close the current log file so that re-enabling creates a new one
        setLogToFile(false);
        closeDebugLogFile();
    }
    else
    {
        setLogToFile(true);
        // Force a log message so that the new log file gets created immediately
        qWarning() << "Debug file logging enabled by user";
    }

    updateDebugLogFileLabel();
}

void DevToolsDialog::applyLoggingRules()
{
    FCT_IDENTIFICATION;

    const QString userRules = ui->loggingRulesEdit->text().trimmed();

    if ( userRules.isEmpty() )
    {
        // Empty rules - back to production defaults
        set_debug_level(LEVEL_PRODUCTION);
        ui->statusLabel->setText(tr("Logging rules cleared (defaults)"));
    }
    else
    {
        // Disable all debug first, then apply user rules on top
        const QString fullRules = "*.debug=false\n" + userRules;
        QLoggingCategory::setFilterRules(fullRules);
        ui->statusLabel->setText(tr("Logging rules applied"));
    }
}

void DevToolsDialog::saveDebugLog()
{
    FCT_IDENTIFICATION;

    const QString logFilename = currentDebugLogFilename();

    if ( logFilename.isEmpty() || !QFile::exists(logFilename) )
    {
        QMessageBox::information(this,
                                 tr("Save Debug Log"),
                                 tr("No debug log file is currently being written"));
        return;
    }

    QSettings settings;
    const QString lastDir = settings.value("devtools/lastDir", QDir::homePath()).toString();

    QString destFilename = QFileDialog::getSaveFileName(
        this, tr("Save Debug Log"),
        lastDir + "/" + QFileInfo(logFilename).fileName(),
        tr("Log Files (*.log);;All Files (*)"));

    if ( destFilename.isEmpty() )
        return;

    if ( !destFilename.endsWith(".log", Qt::CaseInsensitive) )
        destFilename += ".log";

    if ( QFile::exists(destFilename) )
        QFile::remove(destFilename);

    if ( QFile::copy(logFilename, destFilename) )
    {
        settings.setValue("devtools/lastDir", QFileInfo(destFilename).absolutePath());
        ui->statusLabel->setText(
            tr("Debug log saved to %1").arg(QFileInfo(destFilename).fileName()));
    }
    else
    {
        QMessageBox::warning(this, tr("Save Debug Log"),
            tr("Failed to copy the debug log file."));
    }
}

void DevToolsDialog::updateDebugLogFileLabel()
{
    FCT_IDENTIFICATION;

    const QString logFilename = currentDebugLogFilename();

    if ( logFilename.isEmpty() || !isLogToFileEnabled() )
        ui->debugLogFileLabel->setText(tr("File logging is disabled"));
    else
        ui->debugLogFileLabel->setText(tr("Log file: %1").arg(logFilename));

    ui->saveDebugLogButton->setEnabled(
        !logFilename.isEmpty() && QFile::exists(logFilename));
}
