#include <QtTest>
#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QWheelEvent>

#include "ui/component/FreqQSpinBox.h"

class FreqQSpinBoxTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void exactFrequencyUsesDerivedBand();
    void selectedBandHasNoFrequency();
    void mhzActionRestoresLastFrequency();
    void mhzActionUsesSelectedBandFrequency();
    void exactFrequencyReplacesSelectedBand();
    void bandTextMustBeSelectedFromMenu();
    void selectedBandStepsThroughActiveBands();
    void frequencyBandShortcutsStayInMhzMode();
    void bandMenuContainsOnlyActiveBands();
    void bandReloadPreservesCurrentSelection();
    void disabledCurrentBandChangesToFrequency();
    void buttonsFollowInputMode();
    void selectorRemainsCompact();
};

void FreqQSpinBoxTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());

    QSqlQuery query;
    QVERIFY2(query.exec(QStringLiteral("CREATE TABLE bands ("
                                       "name TEXT PRIMARY KEY,"
                                       "start_freq FLOAT,"
                                       "end_freq FLOAT,"
                                       "enabled BOOLEAN,"
                                       "sat_designator TEXT)")),
             qPrintable(query.lastError().text()));
    QVERIFY2(query.exec(QStringLiteral("INSERT INTO bands VALUES "
                                       "('20m', 14.0, 14.35, 1, 'H'),"
                                       "('2m', 144.0, 148.0, 1, 'V'),"
                                       "('70cm', 420.0, 450.0, 1, 'U'),"
                                       "('23cm', 1240.0, 1300.0, 0, 'L')")),
             qPrintable(query.lastError().text()));
}

void FreqQSpinBoxTest::cleanupTestCase()
{
    const QString connectionName = QSqlDatabase::defaultConnection;
    {
        QSqlDatabase db = QSqlDatabase::database();
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void FreqQSpinBoxTest::exactFrequencyUsesDerivedBand()
{
    FreqQSpinBox edit;
    edit.setDecimals(5);
    edit.setMaximum(7500000.0);
    edit.setSuffix(QStringLiteral(" MHz"));
    edit.enableBandSelection();
    edit.setValue(14.074);

    QVERIFY(!edit.hasBand());
    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("20m"));
    QCOMPARE(edit.text(), QStringLiteral("14.07400 MHz"));
}

void FreqQSpinBoxTest::selectedBandHasNoFrequency()
{
    FreqQSpinBox edit;
    edit.enableBandSelection();
    QSignalSpy spy(&edit, &FreqQSpinBox::bandSelected);

    edit.setValue(14.074);
    edit.setBand(QStringLiteral("20m"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(edit.hasBand());
    QVERIFY(!edit.hasFrequency());
    QCOMPARE(edit.value(), 0.0);
    QCOMPARE(edit.selectedBand(), QStringLiteral("20m"));
    QCOMPARE(edit.text(), QStringLiteral("20m"));
}

void FreqQSpinBoxTest::mhzActionRestoresLastFrequency()
{
    FreqQSpinBox edit;
    edit.setDecimals(5);
    edit.setMaximum(7500000.0);
    edit.enableBandSelection();
    edit.setValue(14.074);
    edit.setBand(QStringLiteral("20m"));

    QMenu *menu = edit.findChild<QMenu *>();
    QVERIFY(menu);
    QVERIFY(!menu->actions().isEmpty());
    QCOMPARE(menu->actions().first()->text(), QStringLiteral("MHz"));

    menu->actions().first()->trigger();
    QCoreApplication::processEvents();

    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.value(), 14.074);
    QCOMPARE(edit.selectedBand(), QStringLiteral("20m"));
}

void FreqQSpinBoxTest::mhzActionUsesSelectedBandFrequency()
{
    FreqQSpinBox edit;
    edit.setDecimals(5);
    edit.setMaximum(7500000.0);
    edit.enableBandSelection();
    edit.setValue(14.074);
    edit.setBand(QStringLiteral("2m"));

    QMenu *menu = edit.findChild<QMenu *>();
    QVERIFY(menu);
    menu->actions().first()->trigger();
    QCoreApplication::processEvents();

    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.value(), 144.0);
    QCOMPARE(edit.selectedBand(), QStringLiteral("2m"));
}

void FreqQSpinBoxTest::exactFrequencyReplacesSelectedBand()
{
    FreqQSpinBox edit;
    edit.setMaximum(7500000.0);
    edit.enableBandSelection();
    edit.setBand(QStringLiteral("20m"));

    edit.setValue(145.800);

    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("2m"));
}

void FreqQSpinBoxTest::bandTextMustBeSelectedFromMenu()
{
    const QStringList invalidInputs = {QStringLiteral("20m"),
                                       QStringLiteral("a20m")};

    for ( const QString &input : invalidInputs )
    {
        FreqQSpinBox edit;
        edit.setDecimals(5);
        edit.setMaximum(7500000.0);
        edit.setKeyboardTracking(false);
        edit.enableBandSelection();
        edit.setValue(14.074);
        edit.show();

        QLineEdit *lineEdit = edit.findChild<QLineEdit *>();
        QVERIFY(lineEdit);
        lineEdit->selectAll();
        QTest::keyClicks(lineEdit, input);
        QTest::keyClick(lineEdit, Qt::Key_Return);

        QVERIFY(edit.hasFrequency());
        QCOMPARE(edit.value(), 14.074);
    }
}

void FreqQSpinBoxTest::selectedBandStepsThroughActiveBands()
{
    FreqQSpinBox edit;
    edit.enableBandSelection();
    edit.setBand(QStringLiteral("20m"));

    QTest::keyClick(&edit, Qt::Key_Up);
    QVERIFY(!edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("2m"));

    QWheelEvent wheelDown(QPointF(1, 1), QPointF(1, 1), QPoint(), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&edit, &wheelDown);
    QVERIFY(!edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("20m"));

    QTest::keyClick(&edit, Qt::Key_PageUp);
    QVERIFY(!edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("2m"));

    QTest::keyClick(&edit, Qt::Key_PageDown);
    QVERIFY(!edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("20m"));
}

void FreqQSpinBoxTest::frequencyBandShortcutsStayInMhzMode()
{
    FreqQSpinBox edit;
    edit.setDecimals(5);
    edit.setMaximum(7500000.0);
    edit.setSingleStep(0.001);
    edit.enableBandSelection();
    edit.setValue(14.074);

    QTest::keyClick(&edit, Qt::Key_Up);
    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.value(), 14.075);

    QTest::keyClick(&edit, Qt::Key_PageUp);
    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.value(), 144.0);

    QWheelEvent wheelDown(QPointF(1, 1), QPointF(1, 1), QPoint(), QPoint(0, -120),
                          Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&edit, &wheelDown);
    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.value(), 14.0);
}

void FreqQSpinBoxTest::bandMenuContainsOnlyActiveBands()
{
    FreqQSpinBox edit;
    edit.enableBandSelection();

    QMenu *menu = edit.findChild<QMenu *>();
    QVERIFY(menu);

    bool foundActive = false;
    bool foundInactive = false;
    for ( const QAction *action : menu->actions() )
    {
        foundActive = foundActive || action->text() == QStringLiteral("20m");
        foundInactive = foundInactive || action->text() == QStringLiteral("23cm");
    }

    QVERIFY(foundActive);
    QVERIFY(!foundInactive);
}

void FreqQSpinBoxTest::bandReloadPreservesCurrentSelection()
{
    FreqQSpinBox edit;
    edit.enableBandSelection();
    QVERIFY(edit.setBand(QStringLiteral("2m")));

    QSqlQuery query;
    QVERIFY2(query.exec(QStringLiteral("UPDATE bands SET enabled = 0 WHERE name = '20m'")),
             qPrintable(query.lastError().text()));
    edit.loadBands();

    QVERIFY(edit.hasBand());
    QVERIFY(!edit.hasFrequency());
    QCOMPARE(edit.selectedBand(), QStringLiteral("2m"));

    QVERIFY2(query.exec(QStringLiteral("UPDATE bands SET enabled = 1 WHERE name = '20m'")),
             qPrintable(query.lastError().text()));
}

void FreqQSpinBoxTest::disabledCurrentBandChangesToFrequency()
{
    FreqQSpinBox edit;
    edit.setDecimals(5);
    edit.setMaximum(7500000.0);
    edit.enableBandSelection();
    edit.setValue(145.800);
    QVERIFY(edit.setBand(QStringLiteral("2m")));

    QSqlQuery query;
    QVERIFY2(query.exec(QStringLiteral("UPDATE bands SET enabled = 0 WHERE name = '2m'")),
             qPrintable(query.lastError().text()));
    edit.loadBands();

    QVERIFY(!edit.hasBand());
    QVERIFY(edit.hasFrequency());
    QCOMPARE(edit.value(), 145.800);
    QCOMPARE(edit.selectedBand(), QStringLiteral("2m"));

    QVERIFY2(query.exec(QStringLiteral("UPDATE bands SET enabled = 1 WHERE name = '2m'")),
             qPrintable(query.lastError().text()));
}

void FreqQSpinBoxTest::buttonsFollowInputMode()
{
    FreqQSpinBox edit;
    edit.enableBandSelection();

    QCOMPARE(edit.buttonSymbols(), QAbstractSpinBox::UpDownArrows);

    QVERIFY(edit.setBand(QStringLiteral("20m")));
    QCOMPARE(edit.buttonSymbols(), QAbstractSpinBox::NoButtons);

    QVERIFY(edit.setBand(QString()));
    QCOMPARE(edit.buttonSymbols(), QAbstractSpinBox::UpDownArrows);
}

void FreqQSpinBoxTest::selectorRemainsCompact()
{
    FreqQSpinBox frequencyEdit;
    frequencyEdit.setDecimals(5);
    frequencyEdit.setMaximum(7500000.0);
    frequencyEdit.setSuffix(QStringLiteral(" MHz"));

    FreqQSpinBox bandFrequencyEdit;
    bandFrequencyEdit.setDecimals(5);
    bandFrequencyEdit.setMaximum(7500000.0);
    bandFrequencyEdit.setSuffix(QStringLiteral(" MHz"));
    bandFrequencyEdit.enableBandSelection();
    bandFrequencyEdit.setBand(QStringLiteral("70cm"));

    QVERIFY(bandFrequencyEdit.sizeHint().width()
            <= frequencyEdit.sizeHint().width() + 8);
}

QTEST_MAIN(FreqQSpinBoxTest)

#include "tst_freqqspinbox.moc"
