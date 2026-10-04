#include <QtTest>
#include <QSqlField>
#include "core/MacroExpander.h"

class MacroExpanderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void existingCWMacros();
    void spotComment();
    void missingValues();
    void literalText();
    void savedQSO();
    void spotCommentIsSingleLine();
    void currentAndSavedQSOStaySeparate();
};

void MacroExpanderTest::initTestCase()
{
    QLoggingCategory::setFilterRules("*.debug=false");
}

void MacroExpanderTest::existingCWMacros()
{
    const MacroExpander::Context context = {
        {"callsign", "dl1abc"}, {"name_intl", "Hans"}, {"rst_sent", "599"},
        {"greeting", "gm"}, {"qth_intl", "Munich"}, {"station_callsign", "ok1abc"},
        {"my_name_intl", "Petr"}, {"my_city_intl", "Praha"}, {"my_gridsquare", "jo70aa"},
        {"my_sig_intl", "pota"}, {"my_sig_info_intl", "cz-0001"}, {"my_iota", "eu-001"},
        {"my_sota_ref", "ok/st-001"}, {"my_wwff_ref", "okff-0001"},
        {"my_vucc_grids", "jo70,jo80"}, {"tx_pwr", "0.5"}, {"stx_string", "test"}, {"stx", "9"}
    };
    const auto result = MacroExpander::expand(
        "<DXCALL>|<NAME>|<RST>|<RSTN>|<GREETING>|<QTH>|<MYCALL>|<MYNAME>|<MYQTH>|"
        "<MYLOCATOR>|<MYGRID>|<MYSIG>|<MYSIGINFO>|<MYIOTA>|<MYSOTA>|<MYWWFT>|<MYVUCC>|"
        "<MYPWR>|<EXCHSTR>|<EXCHNR>|<EXCHNRN>", context);
    QCOMPARE(result.text, QString("DL1ABC|HANS|599|5NN|GM|Munich|OK1ABC|PETR|PRAHA|"
                                  "JO70AA|JO70AA|POTA|CZ-0001|EU-001|OK/ST-001|OKFF-0001|"
                                  "JO70,JO80|0.5|TEST|009|TTN"));
    QVERIFY(result.missingMacros.isEmpty());
    QCOMPARE(MacroExpander::expand("<EXCHNR> <EXCHNRN>", MacroExpander::Context()).text,
             QString("000 TTT"));
}

void MacroExpanderTest::spotComment()
{
    const MacroExpander::Context context = {
        {"gridsquare", "jn58td"}, {"prop_mode", "es"}, {"my_gridsquare", "jo70aa"},
        {"mode", "SSB"}, {"rst_sent", "59"}
    };
    const auto result = MacroExpander::expand("<DXLOCATOR><PROP><MYLOCATOR> <MODE> <RST>", context);
    QCOMPARE(result.text, QString("JN58TD<ES>JO70AA SSB 59"));
    QVERIFY(result.missingMacros.isEmpty());
    QCOMPARE(MacroExpander::expand("<DXGRID> <PROPMODE>", context).text, QString("JN58TD ES"));
    QCOMPARE(MacroExpander::expand("<<PROPMODE>>", context).text, QString("<ES>"));
    QVERIFY(MacroExpander::helpText().contains("<PROP>"));
    QVERIFY(MacroExpander::helpText().contains("<DXLOCATOR>"));
}

void MacroExpanderTest::missingValues()
{
    const auto result = MacroExpander::expand("<DXLOCATOR><PROP><MYLOCATOR> <PROP>",
                                             {{"my_gridsquare", "JO70AA"}});
    QCOMPARE(result.text, QString("JO70AA "));
    QCOMPARE(result.missingMacros, QStringList({"<DXLOCATOR>", "<PROP>"}));
    QVERIFY(MacroExpander::expand("", MacroExpander::Context()).missingMacros.isEmpty());
}

void MacroExpanderTest::literalText()
{
    const auto result = MacroExpander::expand("Thanks <NAME> <ES> <+> <++> <-> <--> <UNKNOWN>",
                                             {{"name_intl", "<MYCALL>"}, {"station_callsign", "OK1ABC"}});
    QCOMPARE(result.text, QString("Thanks <MYCALL> <ES> <+> <++> <-> <--> <UNKNOWN>"));
    QVERIFY(result.missingMacros.isEmpty());
}

void MacroExpanderTest::savedQSO()
{
    QSqlRecord qso;
    const QStringList fields = {"gridsquare", "my_gridsquare", "prop_mode", "mode", "rst_sent",
                                "name_intl", "name", "tx_pwr"};
    const QStringList values = {"JN58TD", "JO70AA", "ES", "SSB", "59", "", "Hans", "0.5"};
    for ( int i = 0; i < fields.size(); ++i )
    {
        QSqlField field;
        field.setName(fields[i]);
        field.setValue(values[i]);
        qso.append(field);
    }
    const auto context = MacroExpander::fromQSO(qso);
    QCOMPARE(MacroExpander::expand("<DXLOCATOR><PROP><MYLOCATOR> <MODE> <RST> <NAME> <MYPWR>", context).text,
             QString("JN58TD<ES>JO70AA SSB 59 HANS 0.5"));
    QVERIFY(MacroExpander::expand("<GREETING>", context).missingMacros.contains("<GREETING>"));
    qso.setValue("name_intl", "Petr");
    qso.setValue("tx_pwr", 1.9);
    QCOMPARE(MacroExpander::expand("<NAME> <MYPWR>", MacroExpander::fromQSO(qso)).text,
             QString("PETR 2"));
    qso.setValue("tx_pwr", 0);
    const auto zeroPower = MacroExpander::expand("<MYPWR>", MacroExpander::fromQSO(qso));
    QCOMPARE(zeroPower.text, QString("0"));
    QVERIFY(zeroPower.missingMacros.isEmpty());
}

void MacroExpanderTest::spotCommentIsSingleLine()
{
    const auto result = MacroExpander::expandSpotComment("  <QTH> <MODE>\r\n  ",
                                                        {{"qth_intl", "Prague\r\nsh/dx"}, {"mode", "ssb"}});
    QCOMPARE(result.text, QString("Prague  sh/dx SSB"));
    QVERIFY(result.missingMacros.isEmpty());
    QCOMPARE(MacroExpander::expand("<QTH>", {{"qth_intl", "Prague\n"}}).text, QString("Prague\n"));
    QCOMPARE(MacroExpander::expandSpotComment("<PROP>", {}).missingMacros, QStringList({"<PROP>"}));
}

void MacroExpanderTest::currentAndSavedQSOStaySeparate()
{
    QSqlRecord saved;
    const QStringList fields = {"callsign", "gridsquare", "my_gridsquare", "mode", "prop_mode", "rst_sent"};
    const QStringList values = {"DL1ABC", "JN58TD", "JO70AA", "SSB", "ES", "59"};
    for ( int i = 0; i < fields.size(); ++i )
    {
        QSqlField field;
        field.setName(fields[i]);
        field.setValue(values[i]);
        saved.append(field);
    }
    const MacroExpander::Context current = {
        {"callsign", "OK2XYZ"}, {"gridsquare", "JN89AA"}, {"my_gridsquare", "JN79AA"},
        {"mode", "CW"}, {"prop_mode", "TR"}, {"rst_sent", "599"}
    };
    const QString text = "<DXCALL> <DXLOCATOR><PROP><MYLOCATOR> <MODE> <RST>";
    QCOMPARE(MacroExpander::expandSpotComment(text, current).text,
             QString("OK2XYZ JN89AA<TR>JN79AA CW 599"));
    QCOMPARE(MacroExpander::expandSpotComment(text, MacroExpander::fromQSO(saved)).text,
             QString("DL1ABC JN58TD<ES>JO70AA SSB 59"));
    QCOMPARE(saved.value("callsign").toString(), QString("DL1ABC"));
}

QTEST_APPLESS_MAIN(MacroExpanderTest)
#include "tst_macroexpander.moc"
