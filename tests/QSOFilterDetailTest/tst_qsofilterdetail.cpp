#include <QtTest>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QPointer>
#include <QStackedWidget>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include "ui/QSOFilterDetail.h"
#include "ui/component/QSOFilterDateRangeEdit.h"
#include "data/Data.h"

// Only Data's actual enum definitions are needed by this dialog. Avoid loading
// external DXCC, park and timezone resources in a widget test.
Data::Data(QObject *parent) : QObject(parent) {}
Data::~Data() {}
void Data::invalidateDXCCStatusCache(const QSqlRecord &) {}
void Data::invalidateSetOfDXCCStatusCache(const QSet<uint> &) {}
void Data::clearDXCCStatusCache() {}
const QMetaObject LogbookModel::staticMetaObject = QSqlTableModel::staticMetaObject;
QMap<LogbookModel::ColumnID, QString> LogbookModel::fieldNameTranslationMap = {
    {LogbookModel::COLUMN_TIME_ON, "Time On"},
    {LogbookModel::COLUMN_CALL, "Callsign"},
    {LogbookModel::COLUMN_MODE, "Mode"},
    {LogbookModel::COLUMN_QSL_RCVD, "QSL received"},
    {LogbookModel::COLUMN_QSL_RCVD_DATE, "QSL received date"}
};

class QSOFilterDetailTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void oldValuesSurviveEditor_data();
    void oldValuesSurviveEditor();
    void temporaryParametersDoNotSave();
    void resetParametersUsesSavedDefaults();
    void emptyFilterSurvivesEditor();
    void newDateConditionOffersPeriod();
    void periodSurvivesEditor();
    void lastDaysAreEditable();
    void cancelCustomPeriodPreservesValue();
    void customPeriodEditsBothBoundaries();
    void parameterPopupHasAllActions();
    void relativePresetsAreCaseInsensitive();
    void dismissingParameterPopupDiscardsChanges();
    void invalidPeriodKeepsParameterPopupOpen();
};

void QSOFilterDetailTest::initTestCase()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QCoreApplication::setOrganizationName("QLogTests");
    QCoreApplication::setApplicationName("QSOFilterDetailTest");
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
    QStringList fields;
    for ( int i = 0; i <= LogbookModel::COLUMN_QSL_RCVD_DATE; ++i ) fields << QString("field%1 TEXT").arg(i);
    QSqlQuery query;
    QVERIFY(query.exec("CREATE TABLE contacts (" + fields.join(',') + ")"));
    QVERIFY(query.exec("CREATE TABLE qso_filters (filter_name TEXT PRIMARY KEY, matching_type INTEGER)"));
    QVERIFY(query.exec("CREATE TABLE qso_filter_rules (filter_name TEXT, table_field_index INTEGER, operator_id INTEGER, value TEXT)"));
    QVERIFY(query.exec("CREATE TABLE qso_filter_operators (operator_id INTEGER PRIMARY KEY, sql_operator TEXT)"));
    QVERIFY(query.exec("CREATE TABLE qso_filter_matching_types (matching_id INTEGER PRIMARY KEY, sql_operator TEXT)"));
    QVERIFY(query.exec("INSERT INTO qso_filter_matching_types VALUES (0,'AND'),(1,'OR')"));
    QVERIFY(query.exec("INSERT INTO qso_filter_operators VALUES (0,'='),(1,'<>'),(2,'like'),(3,'not like'),(4,'>'),(5,'<'),(6,'starts with'),(7,'regexp'),(8,'in date range'),(9,'outside date range'),(10,'before date range'),(11,'after date range')"));
    QSOFilterManager::instance();
}

void QSOFilterDetailTest::cleanup()
{
    QSqlQuery query;
    QVERIFY(query.exec("DELETE FROM qso_filter_rules"));
    QVERIFY(query.exec("DELETE FROM qso_filters"));
}

void QSOFilterDetailTest::oldValuesSurviveEditor_data()
{
    QTest::addColumn<int>("field");
    QTest::addColumn<QString>("value");
    QTest::newRow("timestamp") << int(LogbookModel::COLUMN_TIME_ON) << QString("2026-09-05T12:34:56");
    QTest::newRow("timestamp-zone") << int(LogbookModel::COLUMN_TIME_ON) << QString("2026-09-05T12:34:56Z");
    QTest::newRow("timestamp-offset") << int(LogbookModel::COLUMN_TIME_ON) << QString("2026-09-05T12:34:56+00:00");
    QTest::newRow("timestamp-null") << int(LogbookModel::COLUMN_TIME_ON) << QString();
    QTest::newRow("date-null") << int(LogbookModel::COLUMN_QSL_RCVD_DATE) << QString();
    QTest::newRow("date-empty-text") << int(LogbookModel::COLUMN_QSL_RCVD_DATE) << QStringLiteral("");
    QTest::newRow("date-pattern") << int(LogbookModel::COLUMN_QSL_RCVD_DATE) << QString("2026-09");
    QTest::newRow("enum-null") << int(LogbookModel::COLUMN_QSL_RCVD) << QString();
    QTest::newRow("enum-unknown") << int(LogbookModel::COLUMN_QSL_RCVD) << QString("old value");
    QTest::newRow("literal-today") << int(LogbookModel::COLUMN_CALL) << QString("TODAY");
    QTest::newRow("literal-empty-text") << int(LogbookModel::COLUMN_CALL) << QStringLiteral("");
}

void QSOFilterDetailTest::oldValuesSurviveEditor()
{
    QFETCH(int, field); QFETCH(QString, value);
    QSOFilter original;
    original.filterName = "Original";
    original.machingType = 1;
    original.addRule(QSOFilterRule(field, 0, value));
    QVERIFY(QSOFilterManager::instance()->save(original));
    QSOFilterDetail dialog(original.filterName);
    dialog.save();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    const auto saved = QSOFilterManager::instance()->getFilter(original.filterName);
    QCOMPARE(saved.rules, original.rules);
    QCOMPARE(saved.machingType, original.machingType);
}

void QSOFilterDetailTest::temporaryParametersDoNotSave()
{
    QSOFilter original;
    original.filterName = "Unconfirmed QSOs";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_QSL_RCVD, 0, "N"));
    QSOFilterDateRange range;
    range.from = "TODAY-29";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 8, range.toString()));
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_CALL, 0, "OK1ABC"));
    QVERIFY(QSOFilterManager::instance()->save(original));
    QSOFilterDetail dialog(original);
    dialog.show();
    QCoreApplication::processEvents();
    auto *line = dialog.findChild<QLineEdit *>("valueLineEdit2");
    QVERIFY(line);
    line->setText("OK2XYZ");
    QVERIFY(!dialog.findChild<QComboBox *>("fieldNameCombo2"));
    QVERIFY(!dialog.findChild<QComboBox *>("conditionCombo2"));
    if ( qEnvironmentVariableIsSet("QLOG_FILTER_PREVIEW") )
        dialog.grab().save(qEnvironmentVariable("QLOG_FILTER_PREVIEW"));
    dialog.save();
    QCOMPARE(dialog.filter().rules.at(2).value, QString("OK2XYZ"));
    QCOMPARE(dialog.filter().rules.at(0), original.rules.at(0));
    QCOMPARE(dialog.filter().rules.at(1), original.rules.at(1));
    QCOMPARE(QSOFilterManager::instance()->getFilter(original.filterName).rules, original.rules);
}

void QSOFilterDetailTest::resetParametersUsesSavedDefaults()
{
    QSOFilter original;
    original.filterName = "Original";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_CALL, 0, "OK1ABC"));
    QVERIFY(QSOFilterManager::instance()->save(original));
    QSOFilter temporary = original;
    temporary.rules[0].value = "OK2XYZ";
    QSOFilterDetail dialog(temporary);
    dialog.findChild<QDialogButtonBox *>("parameterButtonBox")->button(QDialogButtonBox::RestoreDefaults)->click();
    dialog.save();
    QCOMPARE(dialog.filter().rules, original.rules);
    QCOMPARE(QSOFilterManager::instance()->getFilter(original.filterName).rules, original.rules);
}

void QSOFilterDetailTest::emptyFilterSurvivesEditor()
{
    QSOFilter original;
    original.filterName = "Empty";
    original.machingType = 1;
    QVERIFY(QSOFilterManager::instance()->save(original));
    QSOFilterDetail dialog(original.filterName);
    dialog.save();
    QCOMPARE(QSOFilterManager::instance()->getFilter(original.filterName).machingType, 1);
    QVERIFY(QSOFilterManager::instance()->getFilter(original.filterName).rules.isEmpty());
}

void QSOFilterDetailTest::newDateConditionOffersPeriod()
{
    QSOFilterDetail dialog;
    dialog.addCondition();
    auto *field = dialog.findChild<QComboBox *>("fieldNameCombo0");
    auto *op = dialog.findChild<QComboBox *>("conditionCombo0");
    auto *stack = dialog.findChild<QStackedWidget *>("stackedValueEdit0");
    field->setCurrentIndex(field->findData(int(LogbookModel::COLUMN_TIME_ON)));
    QCOMPARE(op->currentIndex(), int(QSOFilterRule::InDateRange));
    QVERIFY(qobject_cast<QSOFilterDateRangeEdit *>(stack->currentWidget()));
    op->setCurrentIndex(0);
    QVERIFY(qobject_cast<QDateTimeEdit *>(stack->currentWidget()));
}

void QSOFilterDetailTest::periodSurvivesEditor()
{
    QSOFilter original;
    original.filterName = "Period";
    QSOFilterDateRange range;
    range.from = "TODAY-45"; range.to = "TODAY-15";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 8, range.toString()));
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_CALL, 0, "OK1ABC"));
    QVERIFY(QSOFilterManager::instance()->save(original));
    QSOFilterDetail dialog(original.filterName);
    dialog.save();
    QCOMPARE(QSOFilterManager::instance()->getFilter(original.filterName).rules, original.rules);
}

void QSOFilterDetailTest::lastDaysAreEditable()
{
    QSOFilterDateRange initial;
    initial.from = "TODAY-29";
    QSOFilterDateRangeEdit editor(initial.toString());
    auto *days = editor.findChild<QSpinBox *>("periodDays");
    QCOMPARE(days->value(), 30);
    days->setValue(90);
    QSOFilterDateRange range;
    QVERIFY(QSOFilterDateRange::fromString(editor.value(), range));
    QCOMPARE(range.from, QString("TODAY-89"));
    QCOMPARE(range.to, QString("TODAY"));
}

void QSOFilterDetailTest::cancelCustomPeriodPreservesValue()
{
    QSOFilterDateRange range;
    range.from = "TODAY-45";
    range.to = "TODAY-15";
    QSOFilterDateRangeEdit editor(range.toString());
    const QString original = editor.value();
    QSignalSpy changed(&editor, &QSOFilterDateRangeEdit::valueChanged);
    QTimer::singleShot(0, []()
    {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->reject();
    });
    editor.findChild<QPushButton *>("customPeriodButton")->click();
    QCOMPARE(editor.value(), original);
    QCOMPARE(changed.count(), 0);
}

void QSOFilterDetailTest::customPeriodEditsBothBoundaries()
{
    QSOFilterDateRange initial;
    initial.from = "TODAY-30";
    initial.to = "TODAY-2";
    QSOFilterDateRangeEdit editor(initial.toString());
    QTimer::singleShot(0, []()
    {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto offsets = dialog->findChildren<QSpinBox *>("offset");
        QCOMPARE(offsets.size(), 2);
        offsets[0]->setValue(-45);
        offsets[1]->setValue(-15);
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    editor.findChild<QPushButton *>("customPeriodButton")->click();
    QSOFilterDateRange range;
    QVERIFY(QSOFilterDateRange::fromString(editor.value(), range));
    QCOMPARE(range.from, QString("TODAY-45"));
    QCOMPARE(range.to, QString("TODAY-15"));
}

void QSOFilterDetailTest::parameterPopupHasAllActions()
{
    QSOFilter original;
    original.filterName = "Original";
    QSOFilterDateRange range;
    range.from = "TODAY-45";
    range.to = "TODAY-15";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 8, range.toString()));
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_CALL, 0, "OK1ABC"));
    QVERIFY(QSOFilterManager::instance()->save(original));
    QSOFilterDetail popup(original);
    popup.show();
    QCoreApplication::processEvents();
    QCOMPARE(popup.windowType(), Qt::Popup);
    QVERIFY(!popup.isModal());
    auto *buttons = popup.findChild<QDialogButtonBox *>("parameterButtonBox");
    for ( auto button : {QDialogButtonBox::Apply, QDialogButtonBox::Cancel, QDialogButtonBox::RestoreDefaults} )
        QVERIFY(buttons->button(button)->isVisible());
    auto *scroll = popup.findChild<QScrollArea *>();
    QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
    // A custom-period editor must not close the surrounding parameter popup.
    QTimer::singleShot(0, []()
    {
        auto *custom = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(custom);
        const auto offsets = custom->findChildren<QSpinBox *>("offset");
        QCOMPARE(offsets.size(), 2);
        offsets.first()->setValue(1);
        custom->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QVERIFY(custom->isVisible());
        QVERIFY(custom->findChild<QLabel *>("errorLabel")->isVisible());
        custom->reject();
    });
    popup.findChild<QPushButton *>("customPeriodButton")->click();
    QVERIFY(popup.isVisible());
    popup.findChild<QLineEdit *>("valueLineEdit1")->setText("OK2XYZ");
    QSignalSpy accepted(&popup, &QDialog::accepted);
    buttons->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(accepted.count(), 1);
    QCOMPARE(popup.filter().rules.at(1).value, QString("OK2XYZ"));
    QCOMPARE(QSOFilterManager::instance()->getFilter(original.filterName).rules, original.rules);
    QSOFilterDetail canceled(original);
    canceled.findChild<QLineEdit *>("valueLineEdit1")->setText("OK3XYZ");
    canceled.findChild<QDialogButtonBox *>("parameterButtonBox")->button(QDialogButtonBox::Cancel)->click();
    QCOMPARE(canceled.result(), int(QDialog::Rejected));
    QCOMPARE(canceled.filter().rules, original.rules);
}

void QSOFilterDetailTest::relativePresetsAreCaseInsensitive()
{
    QSOFilterDateRange range;
    range.from = "today-29";
    range.to = "ToDaY";
    QSOFilterDateRangeEdit editor(range.toString());
    auto *days = editor.findChild<QSpinBox *>("periodDays");
    QCOMPARE(days->value(), 30);
    QVERIFY(!days->isHidden());
    QVERIFY(editor.findChild<QPushButton *>("customPeriodButton")->isHidden());
    QCOMPARE(editor.value(), range.toString());
}

void QSOFilterDetailTest::dismissingParameterPopupDiscardsChanges()
{
    QSOFilter original;
    original.filterName = "Original";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_CALL, 0, "OK1ABC"));
    QVERIFY(QSOFilterManager::instance()->save(original));
    QPointer<QSOFilterDetail> popup = new QSOFilterDetail(original);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    QSignalSpy accepted(popup, &QDialog::accepted);
    popup->show();
    popup->findChild<QLineEdit *>("valueLineEdit0")->setText("OK2XYZ");
    QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, QPoint(-5, -5));
    QTRY_VERIFY(popup.isNull());
    QCOMPARE(accepted.count(), 0);
    QCOMPARE(QSOFilterManager::instance()->getFilter(original.filterName).rules, original.rules);
}

void QSOFilterDetailTest::invalidPeriodKeepsParameterPopupOpen()
{
    QSOFilter original;
    original.filterName = "Original";
    QSOFilterDateRange range;
    range.from = "TODAY+1";
    original.addRule(QSOFilterRule(LogbookModel::COLUMN_TIME_ON, 8, range.toString()));
    QSOFilterDetail popup(original);
    popup.show();
    popup.findChild<QDialogButtonBox *>("parameterButtonBox")->button(QDialogButtonBox::Apply)->click();
    QVERIFY(popup.isVisible());
    QVERIFY(popup.findChild<QLabel *>("periodErrorLabel")->isVisible());
    QCOMPARE(popup.result(), int(QDialog::Rejected));
}

QTEST_MAIN(QSOFilterDetailTest)
#include "tst_qsofilterdetail.moc"
