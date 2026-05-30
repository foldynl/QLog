#include <QtTest>
#include <QLoggingCategory>
#include "data/Data.h"

namespace {
QString expectedExtendedChar(unsigned char value)
{
    switch (value)
    {
    case 0xA0: return " ";
    case 0xA1: return "!";
    case 0xA2: return "C/";
    case 0xA3: return "PS";
    case 0xA4: return "$?";
    case 0xA5: return "Y=";
    case 0xA6: return "|";
    case 0xA7: return "SS";
    case 0xA8: return "\"";
    case 0xA9: return "(c)";
    case 0xAA: return "a";
    case 0xAB: return "<<";
    case 0xAC: return "!";
    case 0xAE: return "(r)";
    case 0xAF: return "-";
    case 0xB0: return "deg";
    case 0xB1: return "+-";
    case 0xB2: return "2";
    case 0xB3: return "3";
    case 0xB4: return "'";
    case 0xB5: return "u";
    case 0xB6: return "P";
    case 0xB7: return "*";
    case 0xB8: return ",";
    case 0xB9: return "1";
    case 0xBA: return "o";
    case 0xBB: return ">>";
    case 0xBC: return " 1/4 ";
    case 0xBD: return " 1/2 ";
    case 0xBE: return " 3/4 ";
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC3:
    case 0xC4:
    case 0xC5: return "A";
    case 0xC6: return "AE";
    case 0xC7: return "C";
    case 0xC8:
    case 0xC9:
    case 0xCA:
    case 0xCB: return "E";
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF: return "I";
    case 0xD0: return "D";
    case 0xD1: return "N";
    case 0xD2:
    case 0xD3:
    case 0xD4:
    case 0xD5:
    case 0xD6: return "O";
    case 0xD7: return "x";
    case 0xD8: return "O";
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC: return "U";
    case 0xDD: return "Y";
    case 0xDE: return "Th";
    case 0xDF: return "ss";
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5: return "a";
    case 0xE6: return "ae";
    case 0xE7: return "c";
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEB: return "e";
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF: return "i";
    case 0xF0: return "d";
    case 0xF1: return "n";
    case 0xF2:
    case 0xF3:
    case 0xF4:
    case 0xF5:
    case 0xF6: return "o";
    case 0xF7: return "/";
    case 0xF8: return "o";
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC: return "u";
    case 0xFD: return "y";
    case 0xFE: return "th";
    default:
        return "?";
    }
}
}

class DataTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void removeAccents_basic();
    void removeAccents_asciiPrintable();
    void removeAccents_nonPrintable();
    void removeAccents_preservesLineBreaks();
    void removeAccents_limits();
    void removeAccents_benchmark();
};

void DataTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void DataTest::removeAccents_basic()
{
    struct Case { const char *input; const char *expected; };
    const Case cases[] = {
        {"ěščřžýáíé", "escrzyaie"},
        {"ĚŠČŘŽÝÁÍÉ", "ESCRZYAIE"},
        {"äöüß", "aouss"},
        {"ÄÖÜẞ", "AOUSs"},
        {"âêîôûàèùéëïü", "aeiouaeueeiu"},
        {"鍖椾喊", "Chen Zhan Han "},
        {"Питер. Лето. Любов", "Piter. Leto. Liubov"},
        {"10°C", "10degC"},
        {"⾦", "?"}
    };

    for (const Case &c : cases)
    {
        QCOMPARE(Data::removeAccents(QString::fromUtf8(c.input)), QString::fromUtf8(c.expected));
    }
}

void DataTest::removeAccents_asciiPrintable()
{
    for (int i = 32; i <= 127; ++i)
    {
        const QString input{QChar(i)};
        QCOMPARE(Data::removeAccents(input), input);
    }
}

void DataTest::removeAccents_nonPrintable()
{
    for (int i = 128; i <= 254; ++i)
    {
        const QString input{QChar(i)};
        const QString expected = expectedExtendedChar(static_cast<unsigned char>(i));
        QCOMPARE(Data::removeAccents(input), expected);
    }
}

void DataTest::removeAccents_preservesLineBreaks()
{
    QCOMPARE(Data::removeAccents(QStringLiteral("řádek 1\nřádek 2\r\nřádek 3")),
             QStringLiteral("radek 1\nradek 2\r\nradek 3"));
}

void DataTest::removeAccents_limits()
{
    const QString lastInput = QString::fromUtf8("\uFFFE");
    QVERIFY(!lastInput.isEmpty());
    QCOMPARE(Data::removeAccents(lastInput), QString("?"));

    const QString firstInput = QString::fromUtf8("\u0080");
    QVERIFY(!firstInput.isEmpty());
    QCOMPARE(Data::removeAccents(firstInput), QString("?"));
}

void DataTest::removeAccents_benchmark()
{
    QBENCHMARK
    {
        Data::removeAccents(QStringLiteral("ĚŠČŘŽÝÁÍÉdkfj dfj dlkfj dslfkj dslfj dlfkjdfljdslfd lasdjkas jdlkasj dlkasj dlksajd lsajdlas jdls"));
    };
}

QTEST_APPLESS_MAIN(DataTest)

#include "tst_data.moc"
