#include <QtTest>
#include <QLoggingCategory>
#include <memory>

#define private public
#include "core/AlertEvaluator.h"
#undef private

#include "data/SpotAlert.h"
#include "data/WsjtxEntry.h"
#include "data/DxSpot.h"

ClubInfo::ClubInfo(const QString &callsign,
                   const QString &ID,
                   const QDate &validFrom,
                   const QDate &validTo,
                   const QString &club) :
    callsign(callsign),
    id(ID),
    validFrom(validFrom),
    validTo(validTo),
    club(club)
{
}

const QString& ClubInfo::getCallsign() const
{
    return callsign;
}

const QString& ClubInfo::getID() const
{
    return id;
}

const QDate& ClubInfo::getValidFrom() const
{
    return validFrom;
}

const QDate& ClubInfo::getValidTo() const
{
    return validTo;
}

const QString& ClubInfo::getClubInfo() const
{
    return club;
}

struct RuleSpec {
    int source = SpotAlert::WSJTXCQSPOT;
    bool enabled = true;
    int dxLogStatusMap = DxccStatus::Worked;
    int dxLogStatusScope = static_cast<int>(DxccStatusScope::BandMode);
    QString mode = QStringLiteral("*");
    QString band = QStringLiteral("*");
    int dxCountry = 0;
    int ituz = 0;
    int cqz = 0;
    bool pota = false;
    bool sota = false;
    bool iota = false;
    bool wwff = false;
    QString dxContinent = QStringLiteral("*");
    int spotterCountry = 0;
    QString spotterContinent = QStringLiteral("*");
    QStringList dxMember = {QStringLiteral("*")};
    QString callsignRe = QStringLiteral(".*");
    QString commentRe = QStringLiteral(".*");
};

struct WsjtxSpec {
    QString callsign = QStringLiteral("OK1TEST");
    QString message = QStringLiteral("CQ TEST");
    QString decodedMode = QStringLiteral("FT8");
    QString band = QStringLiteral("20m");
    int dxcc = 123;
    int ituz = 28;
    int cqz = 15;
    QString cont = QStringLiteral("EU");
    int spotterDxcc = 45;
    QString spotterCont = QStringLiteral("EU");
    int status = DxccStatus::Worked;
    bool containsPOTA = false;
    bool containsSOTA = false;
    bool containsIOTA = false;
    bool containsWWFF = false;
    QStringList members;
};

struct DxSpotSpec {
    QString callsign = QStringLiteral("OK1TEST");
    QString comment = QStringLiteral("CQ TEST");
    QString modeGroupString = QStringLiteral("DIGITAL");
    QString band = QStringLiteral("20m");
    int dxcc = 123;
    int ituz = 28;
    int cqz = 15;
    QString cont = QStringLiteral("EU");
    int spotterDxcc = 45;
    QString spotterCont = QStringLiteral("EU");
    int status = DxccStatus::Worked;
    bool containsPOTA = false;
    bool containsSOTA = false;
    bool containsIOTA = false;
    bool containsWWFF = false;
    QStringList members;
};

Q_DECLARE_METATYPE(RuleSpec)
Q_DECLARE_METATYPE(WsjtxSpec)
Q_DECLARE_METATYPE(DxSpotSpec)

namespace {

std::unique_ptr<AlertRule> makeRule(const RuleSpec &spec, int source)
{
    std::unique_ptr<AlertRule> rule(new AlertRule());
    rule->ruleName = QStringLiteral("rule");
    rule->enabled = spec.enabled;
    rule->sourceMap = source;
    rule->dxCountry = spec.dxCountry;
    rule->dxLogStatusMap = spec.dxLogStatusMap;
    rule->dxLogStatusScope = AlertRule::toLogStatusScope(spec.dxLogStatusScope);
    rule->dxContinent = spec.dxContinent;
    rule->dxComment = spec.commentRe;
    rule->dxMember = spec.dxMember;
    rule->dxMemberSet = QSet<QString>(spec.dxMember.begin(), spec.dxMember.end());
    rule->mode = spec.mode;
    rule->band = spec.band;
    rule->spotterCountry = spec.spotterCountry;
    rule->spotterContinent = spec.spotterContinent;
    rule->ituz = spec.ituz;
    rule->cqz = spec.cqz;
    rule->pota = spec.pota;
    rule->sota = spec.sota;
    rule->iota = spec.iota;
    rule->wwff = spec.wwff;
    rule->ruleValid = true;

    rule->callsignRE.setPattern(spec.callsignRe);
    rule->callsignRE.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    rule->commentRE.setPattern(spec.commentRe);
    rule->commentRE.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    return rule;
}

WsjtxEntry makeWsjtxEntry(const WsjtxSpec &spec)
{
    WsjtxEntry entry;
    entry.callsign = spec.callsign;
    entry.decode.message = spec.message;
    entry.decodedMode = spec.decodedMode;
    entry.band = spec.band;
    entry.dxcc.dxcc = spec.dxcc;
    entry.dxcc.ituz = spec.ituz;
    entry.dxcc.cqz = spec.cqz;
    entry.dxcc.cont = spec.cont;
    entry.dxcc_spotter.dxcc = spec.spotterDxcc;
    entry.dxcc_spotter.cont = spec.spotterCont;
    entry.status = static_cast<DxccStatus>(spec.status);
    entry.containsPOTA = spec.containsPOTA;
    entry.containsSOTA = spec.containsSOTA;
    entry.containsIOTA = spec.containsIOTA;
    entry.containsWWFF = spec.containsWWFF;
    for (const QString &member : spec.members)
    {
        entry.callsign_member.append(ClubInfo(entry.callsign, QString(), QDate(), QDate(), member));
    }
    return entry;
}

DxSpot makeDxSpot(const DxSpotSpec &spec)
{
    DxSpot spot;
    spot.callsign = spec.callsign;
    spot.comment = spec.comment;
    spot.modeGroupString = spec.modeGroupString;
    spot.band = spec.band;
    spot.dxcc.dxcc = spec.dxcc;
    spot.dxcc.ituz = spec.ituz;
    spot.dxcc.cqz = spec.cqz;
    spot.dxcc.cont = spec.cont;
    spot.dxcc_spotter.dxcc = spec.spotterDxcc;
    spot.dxcc_spotter.cont = spec.spotterCont;
    spot.status = static_cast<DxccStatus>(spec.status);
    spot.containsPOTA = spec.containsPOTA;
    spot.containsSOTA = spec.containsSOTA;
    spot.containsIOTA = spec.containsIOTA;
    spot.containsWWFF = spec.containsWWFF;
    for (const QString &member : spec.members)
    {
        spot.callsign_member.append(ClubInfo(spot.callsign, QString(), QDate(), QDate(), member));
    }
    return spot;
}
}

class AlertEvaluatorTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void match_wsjtx_data();
    void match_wsjtx();
    void match_dxspot_data();
    void match_dxspot();
    void cross_valid_data();
    void cross_valid();
    void cross_valid2_data();
    void cross_valid2();
    void scope_dxspot_data();
    void scope_dxspot();
    void scope_wsjtx_data();
    void scope_wsjtx();
    void scope_resolver_arguments();
    void scope_conversion();
    void logstatus_sentence_data();
    void logstatus_sentence();
    void alarm_command_line();
    void alarm_backoff();
};

void AlertEvaluatorTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void AlertEvaluatorTest::match_wsjtx_data()
{
    QTest::addColumn<RuleSpec>("rule");
    QTest::addColumn<WsjtxSpec>("input");
    QTest::addColumn<bool>("expected");

    /* Rule is disabled */
    /* Test DXCC STATUS */
    for (int bit = DxccStatus::NewEntity; bit <= DxccStatus::UnknownStatus; bit <<= 1)
    {
        RuleSpec rule;
        rule.enabled = false;
        rule.dxLogStatusMap = static_cast<DxccStatus>(bit);

        for (int bit2 = DxccStatus::NewEntity; bit2 <= DxccStatus::UnknownStatus; bit2 <<= 1)
        {
            WsjtxSpec input;
            input.status = static_cast<DxccStatus>(bit2);
            QTest::addRow("wsjtx_disabled_%d_%d", bit, bit2) << rule << input << false;
        }
    }

    /* Rule is enable */
    /* Test DXCC STATUS */
    for (int bit = DxccStatus::NewEntity; bit <= DxccStatus::UnknownStatus; bit <<= 1)
    {
        RuleSpec rule;
        rule.dxLogStatusMap = static_cast<DxccStatus>(bit);

        for (int bit2 = DxccStatus::NewEntity; bit2 <= DxccStatus::UnknownStatus; bit2 <<= 1)
        {
            WsjtxSpec input;
            input.status = static_cast<DxccStatus>(bit2);
            QTest::addRow("wsjtx_DXstatus_%d_%d", bit, bit2) << rule << input << (bit == bit2);
        }
    }

    /* Test Country */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.dxCountry = x;

        for (int y = 0; y <= 10; y++)
        {
            WsjtxSpec input;
            input.dxcc = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("wsjtx_DXCountry_%d_%d", x, y) << rule << input << result;
        }
    }

    /* Test Country */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.spotterCountry = x;

        for (int y = 0; y <= 10; y++)
        {
            WsjtxSpec input;
            input.spotterDxcc = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("wsjtx_SpotterCountry_%d_%d", x, y) << rule << input << result;
        }
    }

    /* Test ITU  */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.ituz = x;

        for (int y = 0; y <= 10; y++)
        {
            WsjtxSpec input;
            input.ituz = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("wsjtx_ITU_%d_%d", x, y) << rule << input << result;
        }
    }

    /* Test ITU  */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.cqz = x;

        for (int y = 0; y <= 10; y++)
        {
            WsjtxSpec input;
            input.cqz = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("wsjtx_CQZ_%d_%d", x, y) << rule << input << result;
        }
    }

    /* POTA/SOTA/IOTA/WWFF setting */
    for (int ruleMask = 0; ruleMask < 16; ++ruleMask)
    {
        for (int inputMask = 0; inputMask < 16; ++inputMask)
        {

            RuleSpec rule;
            rule.pota = (ruleMask & 1) != 0;
            rule.sota = (ruleMask & 2) != 0;
            rule.iota = (ruleMask & 4) != 0;
            rule.wwff = (ruleMask & 8) != 0;

            WsjtxSpec in;
            in.containsPOTA = (inputMask & 1) != 0;
            in.containsSOTA = (inputMask & 2) != 0;
            in.containsIOTA = (inputMask & 4) != 0;
            in.containsWWFF = (inputMask & 8) != 0;

            const bool expected =
                (ruleMask == 0) ? true : ((inputMask & ruleMask) != 0);

            QTest::newRow(
                QString("wsjtx_ref_rule_0x%1_input_0x%2")
                    .arg(ruleMask, 2, 16, QLatin1Char('0'))
                    .arg(inputMask, 2, 16, QLatin1Char('0'))
                    .toUtf8().constData()
            ) << rule << in << expected;
        }
    }

    /* Callsign match */
    {
        RuleSpec rule;
        rule.callsignRe = "OK2T";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TE";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK2T.*";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TE.*";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TEST";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TEST";

        WsjtxSpec in;
        in.callsign="OK1TEST/P";
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TEST.*";

        WsjtxSpec in;
        in.callsign="OK1TEST/P";
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = ".*TEST";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    /* DX Continent */
    {
        QStringList conts = {"NA", "SA", "EU", "AF", "OC", "AS", "AN"};

        for ( const QString &c : conts )
        {
            RuleSpec rule;
            rule.dxContinent = "|" + c;
            for ( const QString &c1: conts )
            {
               WsjtxSpec in;
               in.cont = c1;
               QTest::newRow(QString("wsjtx_continents_%1_%2").arg(rule.dxContinent,
                                                                 in.cont).toUtf8().constData()
                             ) << rule << in << (rule.dxContinent.contains(in.cont));
            }
        }
    }

    {
        RuleSpec rule;
        rule.dxContinent = "|EU|NA";

        WsjtxSpec in;
        in.cont = "NA";
        QTest::newRow(QString("wsjtx_continents_%1_%2").arg(rule.dxContinent,
                                                          in.cont).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.dxContinent = "|EU|NA";

        WsjtxSpec in;
        in.cont = "OC";
        QTest::newRow(QString("wsjtx_continents_%1_%2").arg(rule.dxContinent,
                                                          in.cont).toUtf8().constData()
                      ) << rule << in << false;

    }

    /* Spotter Continent */
    {
        QStringList conts = {"NA", "SA", "EU", "AF", "OC", "AS", "AN"};

        for ( const QString &c : conts )
        {
            RuleSpec rule;
            rule.spotterContinent = "|" + c;
            for ( const QString &c1: conts )
            {
               WsjtxSpec in;
               in.spotterCont = c1;
               QTest::newRow(QString("wsjtx_spottercontinents_%1_%2").arg(rule.spotterContinent,
                                                                 in.spotterCont).toUtf8().constData()
                             ) << rule << in << (rule.spotterContinent.contains(in.spotterCont));
            }
        }
    }

    {
        RuleSpec rule;
        rule.spotterContinent = "|EU|NA";

        WsjtxSpec in;
        in.cont = "NA";
        QTest::newRow(QString("wsjtx_spottercontinents_%1_%2").arg(rule.spotterContinent,
                                                          in.spotterCont).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.spotterContinent = "|EU|NA";

        WsjtxSpec in;
        in.spotterCont = "OC";
        QTest::newRow(QString("wsjtx_spottercontinents_%1_%2").arg(rule.spotterContinent,
                                                          in.spotterCont).toUtf8().constData()
                      ) << rule << in << false;

    }

    // message
    {
        RuleSpec rule;
        rule.commentRe = "TEST";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_comment_%1_%2").arg(rule.commentRe,
                                                          in.message).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.commentRe = "^TEST";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_comment_%1_%2").arg(rule.commentRe,
                                                          in.message).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.commentRe = ".*TEST.*";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_comment_%1_%2").arg(rule.commentRe,
                                                          in.message).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.commentRe = ".*TAST.*";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_comment_%1_%2").arg(rule.commentRe,
                                                          in.message).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.commentRe = "C.*TEST.*";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_comment_%1_%2").arg(rule.commentRe,
                                                          in.message).toUtf8().constData()
                      ) << rule << in << true;

    }

    // mode
    {
        RuleSpec rule;
        rule.mode = "|FTx";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.mode = "|FTx";

        WsjtxSpec in;
        in.decodedMode = "FT4";
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << true;
    }

    {
        RuleSpec rule;
        rule.mode = "|FTx";

        WsjtxSpec in;
        in.decodedMode = "FT2";
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << true;
    }

    {
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        WsjtxSpec in;
        in.decodedMode = "FT8";
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        WsjtxSpec in;
        in.decodedMode = "FT4";
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        WsjtxSpec in;
        in.decodedMode = "FT2";
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        WsjtxSpec in;
        in.decodedMode = "DIGITAL";
        QTest::newRow(QString("wsjtx_mode_%1_%2").arg(rule.mode,
                                                          in.decodedMode).toUtf8().constData()
                      ) << rule << in << true;
    }
    // members
    {
        QStringList m1 = {"A1", "A2", "B1"};
        QStringList m2 = {"B2"};

        RuleSpec rule;
        rule.dxMember = m1;
        WsjtxSpec in;
        in.members = m2;

        QTest::newRow("wsjtx_members_notmatch") << rule << in << false;
    }

    {
        QStringList m1 = {"A1", "A2", "B1"};
        QStringList m2 = {"B2", "B1"};

        RuleSpec rule;
        rule.dxMember = m1;
        WsjtxSpec in;
        in.members = m2;

        QTest::newRow("wsjtx_members_match") << rule << in << true;
    }

    {
        QStringList m1 = {"B1"};
        QStringList m2 = {"B1", "B2"};

        RuleSpec rule;
        rule.dxMember = m1;
        WsjtxSpec in;
        in.members = m2;

        QTest::newRow("wsjtx_members_match2") << rule << in << true;
    }
    // band
    {
        RuleSpec rule;
        rule.band = "|20m|60m";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << true;
    }

    {
        RuleSpec rule;
        rule.band = "|60m";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.band = "|120m";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.band = "|10m|20m|60m|80m|";

        WsjtxSpec in;
        QTest::newRow(QString("wsjtx_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << true;
    }
}

void AlertEvaluatorTest::match_wsjtx()
{
    QFETCH(RuleSpec, rule);
    QFETCH(WsjtxSpec, input);
    QFETCH(bool, expected);

    auto alertRule = makeRule(rule, SpotAlert::WSJTXCQSPOT);
    WsjtxEntry entry = makeWsjtxEntry(input);

    QCOMPARE(alertRule->match(entry), expected);
}

void AlertEvaluatorTest::match_dxspot_data()
{
    QTest::addColumn<RuleSpec>("rule");
    QTest::addColumn<DxSpotSpec>("input");
    QTest::addColumn<bool>("expected");

    /* Rule is disabled */
    /* Test DXCC STATUS */
    for (int bit = DxccStatus::NewEntity; bit <= DxccStatus::UnknownStatus; bit <<= 1)
    {
        RuleSpec rule;
        rule.enabled = false;
        rule.dxLogStatusMap = static_cast<DxccStatus>(bit);

        for (int bit2 = DxccStatus::NewEntity; bit2 <= DxccStatus::UnknownStatus; bit2 <<= 1)
        {
            DxSpotSpec input;
            input.status = static_cast<DxccStatus>(bit2);
            QTest::addRow("dxspot_disabled_%d_%d", bit, bit2) << rule << input << false;
        }
    }

    /* Rule is enable */
    /* Test DXCC STATUS */
    for (int bit = DxccStatus::NewEntity; bit <= DxccStatus::UnknownStatus; bit <<= 1)
    {
        RuleSpec rule;
        rule.dxLogStatusMap = static_cast<DxccStatus>(bit);

        for (int bit2 = DxccStatus::NewEntity; bit2 <= DxccStatus::UnknownStatus; bit2 <<= 1)
        {
            DxSpotSpec input;
            input.status = static_cast<DxccStatus>(bit2);
            QTest::addRow("dxspot_DXstatus_%d_%d", bit, bit2) << rule << input << (bit == bit2);
        }
    }

    /* Test Country */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.dxCountry = x;

        for (int y = 0; y <= 10; y++)
        {
            DxSpotSpec input;
            input.dxcc = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("dxspot_DXCountry_%d_%d", x, y) << rule << input << result;
        }
    }

    /* Test Country */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.spotterCountry = x;

        for (int y = 0; y <= 10; y++)
        {
            DxSpotSpec input;
            input.spotterDxcc = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("dxspot_SpotterCountry_%d_%d", x, y) << rule << input << result;
        }
    }

    /* Test ITU  */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.ituz = x;

        for (int y = 0; y <= 10; y++)
        {
            DxSpotSpec input;
            input.ituz = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("dxspot_ITU_%d_%d", x, y) << rule << input << result;
        }
    }

    /* Test ITU  */
    for (int x = 0; x <= 10; x++)
    {
        RuleSpec rule;
        rule.cqz = x;

        for (int y = 0; y <= 10; y++)
        {
            DxSpotSpec input;
            input.cqz = y;
            bool result = (x == 0) ? true : (x == y);    // country == 0 ->no filter;
            QTest::addRow("dxspot_CQZ_%d_%d", x, y) << rule << input << result;
        }
    }

    /* POTA/SOTA/IOTA/WWFF setting */
    for (int ruleMask = 0; ruleMask < 16; ++ruleMask)
    {
        for (int inputMask = 0; inputMask < 16; ++inputMask)
        {

            RuleSpec rule;
            rule.pota = (ruleMask & 1) != 0;
            rule.sota = (ruleMask & 2) != 0;
            rule.iota = (ruleMask & 4) != 0;
            rule.wwff = (ruleMask & 8) != 0;

            DxSpotSpec in;
            in.containsPOTA = (inputMask & 1) != 0;
            in.containsSOTA = (inputMask & 2) != 0;
            in.containsIOTA = (inputMask & 4) != 0;
            in.containsWWFF = (inputMask & 8) != 0;

            const bool expected =
                (ruleMask == 0) ? true : ((inputMask & ruleMask) != 0);

            QTest::newRow(
                QString("dxspot_ref_rule_0x%1_input_0x%2")
                    .arg(ruleMask, 2, 16, QLatin1Char('0'))
                    .arg(inputMask, 2, 16, QLatin1Char('0'))
                    .toUtf8().constData()
            ) << rule << in << expected;
        }
    }

    /* Callsign match */
    {
        RuleSpec rule;
        rule.callsignRe = "OK2T";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TE";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK2T.*";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TE.*";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TEST";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TEST";

        DxSpotSpec in;
        in.callsign="OK1TEST/P";
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = "OK1TEST.*";

        DxSpotSpec in;
        in.callsign="OK1TEST/P";
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.callsignRe = ".*TEST";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_callsign_%1_%2").arg(rule.callsignRe,
                                                          in.callsign).toUtf8().constData()
                      ) << rule << in << true;

    }

    /* DX Continent */
    {
        QStringList conts = {"NA", "SA", "EU", "AF", "OC", "AS", "AN"};

        for ( const QString &c : conts )
        {
            RuleSpec rule;
            rule.dxContinent = "|" + c;
            for ( const QString &c1: conts )
            {
               DxSpotSpec in;
               in.cont = c1;
               QTest::newRow(QString("dxspot_continents_%1_%2").arg(rule.dxContinent,
                                                                 in.cont).toUtf8().constData()
                             ) << rule << in << (rule.dxContinent.contains(in.cont));
            }
        }
    }

    {
        RuleSpec rule;
        rule.dxContinent = "|EU|NA";

        DxSpotSpec in;
        in.cont = "NA";
        QTest::newRow(QString("dxspot_continents_%1_%2").arg(rule.dxContinent,
                                                          in.cont).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.dxContinent = "|EU|NA";

        DxSpotSpec in;
        in.cont = "OC";
        QTest::newRow(QString("dxspot_continents_%1_%2").arg(rule.dxContinent,
                                                          in.cont).toUtf8().constData()
                      ) << rule << in << false;

    }

    /* Spotter Continent */
    {
        QStringList conts = {"NA", "SA", "EU", "AF", "OC", "AS", "AN"};

        for ( const QString &c : conts )
        {
            RuleSpec rule;
            rule.spotterContinent = "|" + c;
            for ( const QString &c1: conts )
            {
               DxSpotSpec in;
               in.spotterCont = c1;
               QTest::newRow(QString("dxspot_spottercontinents_%1_%2").arg(rule.spotterContinent,
                                                                 in.spotterCont).toUtf8().constData()
                             ) << rule << in << (rule.spotterContinent.contains(in.spotterCont));
            }
        }
    }

    {
        RuleSpec rule;
        rule.spotterContinent = "|EU|NA";

        DxSpotSpec in;
        in.spotterCont = "NA";
        QTest::newRow(QString("dxspot_spottercontinents_%1_%2").arg(rule.spotterContinent,
                                                          in.spotterCont).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.spotterContinent = "|EU|NA";

        DxSpotSpec in;
        in.spotterCont = "OC";
        QTest::newRow(QString("dxspot_spottercontinents_%1_%2").arg(rule.spotterContinent,
                                                          in.spotterCont).toUtf8().constData()
                      ) << rule << in << false;

    }

    // comment
    {
        RuleSpec rule;
        rule.commentRe = "TEST";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_comment_%1_%2").arg(rule.commentRe,
                                                          in.comment).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.commentRe = "^TEST";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_comment_%1_%2").arg(rule.commentRe,
                                                          in.comment).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.commentRe = ".*TEST.*";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_comment_%1_%2").arg(rule.commentRe,
                                                          in.comment).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        RuleSpec rule;
        rule.commentRe = ".*TAST.*";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_comment_%1_%2").arg(rule.commentRe,
                                                          in.comment).toUtf8().constData()
                      ) << rule << in << false;

    }

    {
        RuleSpec rule;
        rule.commentRe = "C.*TEST.*";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_comment_%1_%2").arg(rule.commentRe,
                                                          in.comment).toUtf8().constData()
                      ) << rule << in << true;

    }

    // mode
    {
        RuleSpec rule;
        rule.mode = "|FTx";

        DxSpotSpec in;
        in.modeGroupString = "FTx";
        QTest::newRow(QString("dxspot_mode_%1_%2").arg(rule.mode,
                                                          in.modeGroupString).toUtf8().constData()
                      ) << rule << in << true;

    }

    {
        // FT4 spot: at a FT4 frequency the band plan assigns modeGroupString "FTx"
        RuleSpec rule;
        rule.mode = "|FTx";

        DxSpotSpec in;
        in.modeGroupString = "FTx";
        QTest::newRow("dxspot_mode_|FTx_FT4") << rule << in << true;
    }

    {
        // FT2 spot: at a FT2 frequency the band plan assigns modeGroupString "FTx"
        RuleSpec rule;
        rule.mode = "|FTx";

        DxSpotSpec in;
        in.modeGroupString = "FTx";
        QTest::newRow("dxspot_mode_|FTx_FT2") << rule << in << true;
    }

    {
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        DxSpotSpec in;
        in.modeGroupString = "FTx";
        QTest::newRow(QString("dxspot_mode_%1_%2").arg(rule.mode,
                                                          in.modeGroupString).toUtf8().constData()
                      ) << rule << in << false;
    }

    {
        // FT4 spot (modeGroupString "FTx") does not match DIGITAL filter
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        DxSpotSpec in;
        in.modeGroupString = "FTx";
        QTest::newRow("dxspot_mode_|DIGITAL_FT4") << rule << in << false;
    }

    {
        // FT2 spot (modeGroupString "FTx") does not match DIGITAL filter
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        DxSpotSpec in;
        in.modeGroupString = "FTx";
        QTest::newRow("dxspot_mode_|DIGITAL_FT2") << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.mode = "|DIGITAL";

        DxSpotSpec in;
        in.modeGroupString = "DIGITAL";
        QTest::newRow(QString("dxspot_mode_%1_%2").arg(rule.mode,
                                                          in.modeGroupString).toUtf8().constData()
                      ) << rule << in << true;
    }
    // members
    {
        QStringList m1 = {"A1", "A2", "B1"};
        QStringList m2 = {"B2"};

        RuleSpec rule;
        rule.dxMember = m1;
        DxSpotSpec in;
        in.members = m2;

        QTest::newRow("dxspot_members_notmatch") << rule << in << false;
    }

    {
        QStringList m1 = {"A1", "A2", "B1"};
        QStringList m2 = {"B2", "B1"};

        RuleSpec rule;
        rule.dxMember = m1;
        DxSpotSpec in;
        in.members = m2;

        QTest::newRow("dxspot_members_match") << rule << in << true;
    }

    {
        QStringList m1 = {"B1"};
        QStringList m2 = {"B1", "B2"};

        RuleSpec rule;
        rule.dxMember = m1;
        DxSpotSpec in;
        in.members = m2;

        QTest::newRow("dxspot_members_match2") << rule << in << true;
    }
    // band
    {
        RuleSpec rule;
        rule.band = "|20m|60m";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << true;
    }

    {
        RuleSpec rule;
        rule.band = "|60m";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.band = "|120m";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << false;
    }

    {
        RuleSpec rule;
        rule.band = "|10m|20m|60m|80m|";

        DxSpotSpec in;
        QTest::newRow(QString("dxspot_band_%1_%2").arg(rule.band,
                                                      in.band).toUtf8().constData()) << rule << in << true;
    }
}

void AlertEvaluatorTest::match_dxspot()
{
    QFETCH(RuleSpec, rule);
    QFETCH(DxSpotSpec, input);
    QFETCH(bool, expected);

    auto alertRule = makeRule(rule, SpotAlert::DXSPOT);
    DxSpot spot = makeDxSpot(input);

    QCOMPARE(alertRule->match(spot), expected);
}

void AlertEvaluatorTest::cross_valid_data()
{
    QTest::addColumn<RuleSpec>("rule");
    QTest::addColumn<DxSpotSpec>("input");
    QTest::addColumn<bool>("expected");

    {
        RuleSpec rule;
        DxSpotSpec in;
        QTest::newRow("Cross") << rule << in << false;
    }
}

void AlertEvaluatorTest::cross_valid()
{
    QFETCH(RuleSpec, rule);
    QFETCH(DxSpotSpec, input);
    QFETCH(bool, expected);

    auto alertRule = makeRule(rule, SpotAlert::WSJTXCQSPOT);
    DxSpot spot = makeDxSpot(input);

    QCOMPARE(alertRule->match(spot), expected);
}

void AlertEvaluatorTest::cross_valid2_data()
{
    QTest::addColumn<RuleSpec>("rule");
    QTest::addColumn<WsjtxSpec>("input");
    QTest::addColumn<bool>("expected");

    {
        RuleSpec rule;
        WsjtxSpec in;
        QTest::newRow("Cross2") << rule << in << false;
    }
}

void AlertEvaluatorTest::cross_valid2()
{
    QFETCH(RuleSpec, rule);
    QFETCH(WsjtxSpec, input);
    QFETCH(bool, expected);

    auto alertRule = makeRule(rule, SpotAlert::DXSPOT);
    WsjtxEntry spot = makeWsjtxEntry(input);

    QCOMPARE(alertRule->match(spot), expected);
}

namespace {

/* Stub resolver: the Band scope status and the Entity scope status
 * are given by the test, everything else is a failure */
AlertRule::LogStatusResolver makeResolver(int bandStatus, int entityStatus, int modeStatus = DxccStatus::UnknownStatus)
{
    return [bandStatus, entityStatus, modeStatus](int, const QString &, const QString &, DxccStatusScope scope) -> DxccStatus
    {
        switch ( scope )
        {
        case DxccStatusScope::Band:   return static_cast<DxccStatus>(bandStatus);
        case DxccStatusScope::Entity: return static_cast<DxccStatus>(entityStatus);
        case DxccStatusScope::Mode:   return static_cast<DxccStatus>(modeStatus);
        default:                      return DxccStatus::UnknownStatus;
        }
    };
}

}

void AlertEvaluatorTest::scope_dxspot_data()
{
    QTest::addColumn<RuleSpec>("rule");
    QTest::addColumn<DxSpotSpec>("input");
    QTest::addColumn<int>("bandStatus");
    QTest::addColumn<int>("entityStatus");
    QTest::addColumn<bool>("useResolver");
    QTest::addColumn<bool>("expected");

    const QList<QPair<int, const char*>> scopes =
    {
        { static_cast<int>(DxccStatusScope::BandMode), "bandmode" },
        { static_cast<int>(DxccStatusScope::Band),     "band" },
        { static_cast<int>(DxccStatusScope::Entity),   "entity" }
    };

    /* the rule waits for Worked; each scope has a different status
     * and only the status of the rule's scope must be evaluated */
    for ( const auto &scope : scopes )
    {
        for ( int spotStatus = DxccStatus::NewEntity; spotStatus <= DxccStatus::UnknownStatus; spotStatus <<= 1 )
        {
            for ( int bandStatus = DxccStatus::NewEntity; bandStatus <= DxccStatus::UnknownStatus; bandStatus <<= 1 )
            {
                for ( int entityStatus = DxccStatus::NewEntity; entityStatus <= DxccStatus::UnknownStatus; entityStatus <<= 1 )
                {
                    RuleSpec rule;
                    rule.dxLogStatusMap = DxccStatus::Worked;
                    rule.dxLogStatusScope = scope.first;

                    DxSpotSpec in;
                    in.status = spotStatus;

                    int evaluated = spotStatus;
                    if ( scope.first == static_cast<int>(DxccStatusScope::Band) )   evaluated = bandStatus;
                    if ( scope.first == static_cast<int>(DxccStatusScope::Entity) ) evaluated = entityStatus;

                    QTest::addRow("dxspot_scope_%s_%d_%d_%d", scope.second, spotStatus, bandStatus, entityStatus)
                        << rule << in << bandStatus << entityStatus << true
                        << (evaluated == DxccStatus::Worked);
                }
            }
        }
    }

    /* without a resolver the spot status is used for every scope */
    for ( const auto &scope : scopes )
    {
        for ( int spotStatus = DxccStatus::NewEntity; spotStatus <= DxccStatus::UnknownStatus; spotStatus <<= 1 )
        {
            RuleSpec rule;
            rule.dxLogStatusMap = DxccStatus::Worked;
            rule.dxLogStatusScope = scope.first;

            DxSpotSpec in;
            in.status = spotStatus;

            QTest::addRow("dxspot_scope_noresolver_%s_%d", scope.second, spotStatus)
                << rule << in << static_cast<int>(DxccStatus::Confirmed) << static_cast<int>(DxccStatus::Confirmed) << false
                << (spotStatus == DxccStatus::Worked);
        }
    }

    /* the typical use-case from issue #913: an alert for an entity
     * that has been worked on some band but not confirmed on any band */
    {
        RuleSpec rule;
        rule.dxLogStatusMap = DxccStatus::NewEntity | DxccStatus::Worked;
        rule.dxLogStatusScope = static_cast<int>(DxccStatusScope::Entity);

        DxSpotSpec in;
        in.status = DxccStatus::NewBandMode;  // never worked on the spot's band/mode

        QTest::addRow("dxspot_scope_913_unconfirmed_entity")
            << rule << in << static_cast<int>(DxccStatus::NewBand) << static_cast<int>(DxccStatus::Worked) << true << true;
        QTest::addRow("dxspot_scope_913_confirmed_entity")
            << rule << in << static_cast<int>(DxccStatus::NewBand) << static_cast<int>(DxccStatus::Confirmed) << true << false;
    }

    /* the second scenario from issue #913: not confirmed on the band in any mode */
    {
        RuleSpec rule;
        rule.dxLogStatusMap = DxccStatus::NewEntity | DxccStatus::NewBand | DxccStatus::Worked;
        rule.dxLogStatusScope = static_cast<int>(DxccStatusScope::Band);

        DxSpotSpec in;
        in.status = DxccStatus::NewMode;

        QTest::addRow("dxspot_scope_913_unconfirmed_band")
            << rule << in << static_cast<int>(DxccStatus::Worked) << static_cast<int>(DxccStatus::Confirmed) << true << true;
        QTest::addRow("dxspot_scope_913_confirmed_band")
            << rule << in << static_cast<int>(DxccStatus::Confirmed) << static_cast<int>(DxccStatus::Confirmed) << true << false;
    }
}

void AlertEvaluatorTest::scope_dxspot()
{
    QFETCH(RuleSpec, rule);
    QFETCH(DxSpotSpec, input);
    QFETCH(int, bandStatus);
    QFETCH(int, entityStatus);
    QFETCH(bool, useResolver);
    QFETCH(bool, expected);

    auto alertRule = makeRule(rule, SpotAlert::DXSPOT);
    DxSpot spot = makeDxSpot(input);

    const AlertRule::LogStatusResolver resolver = useResolver ? makeResolver(bandStatus, entityStatus)
                                                              : AlertRule::LogStatusResolver();

    QCOMPARE(alertRule->match(spot, resolver), expected);
}

void AlertEvaluatorTest::scope_wsjtx_data()
{
    QTest::addColumn<RuleSpec>("rule");
    QTest::addColumn<WsjtxSpec>("input");
    QTest::addColumn<int>("bandStatus");
    QTest::addColumn<int>("entityStatus");
    QTest::addColumn<bool>("expected");

    for ( int spotStatus = DxccStatus::NewEntity; spotStatus <= DxccStatus::UnknownStatus; spotStatus <<= 1 )
    {
        for ( int scopedStatus = DxccStatus::NewEntity; scopedStatus <= DxccStatus::UnknownStatus; scopedStatus <<= 1 )
        {
            RuleSpec rule;
            rule.dxLogStatusMap = DxccStatus::Confirmed;

            WsjtxSpec in;
            in.status = spotStatus;

            rule.dxLogStatusScope = static_cast<int>(DxccStatusScope::BandMode);
            QTest::addRow("wsjtx_scope_bandmode_%d_%d", spotStatus, scopedStatus)
                << rule << in << scopedStatus << scopedStatus << (spotStatus == DxccStatus::Confirmed);

            rule.dxLogStatusScope = static_cast<int>(DxccStatusScope::Band);
            QTest::addRow("wsjtx_scope_band_%d_%d", spotStatus, scopedStatus)
                << rule << in << scopedStatus << static_cast<int>(DxccStatus::UnknownStatus) << (scopedStatus == DxccStatus::Confirmed);

            rule.dxLogStatusScope = static_cast<int>(DxccStatusScope::Entity);
            QTest::addRow("wsjtx_scope_entity_%d_%d", spotStatus, scopedStatus)
                << rule << in << static_cast<int>(DxccStatus::UnknownStatus) << scopedStatus << (scopedStatus == DxccStatus::Confirmed);

            /* the Mode scope is served by the mode status of the resolver; band and entity statuses are unknown */
            rule.dxLogStatusScope = static_cast<int>(DxccStatusScope::Mode);
            QTest::addRow("wsjtx_scope_mode_%d_%d", spotStatus, scopedStatus)
                << rule << in << static_cast<int>(DxccStatus::UnknownStatus) << static_cast<int>(DxccStatus::UnknownStatus) << (scopedStatus == DxccStatus::Confirmed);
        }
    }
}

void AlertEvaluatorTest::scope_wsjtx()
{
    QFETCH(RuleSpec, rule);
    QFETCH(WsjtxSpec, input);
    QFETCH(int, bandStatus);
    QFETCH(int, entityStatus);
    QFETCH(bool, expected);

    auto alertRule = makeRule(rule, SpotAlert::WSJTXCQSPOT);
    WsjtxEntry entry = makeWsjtxEntry(input);

    /* the mode status of the resolver mirrors the expectation: Confirmed when the row expects a match */
    const int modeStatus = expected ? DxccStatus::Confirmed : DxccStatus::Worked;
    QCOMPARE(alertRule->match(entry, makeResolver(bandStatus, entityStatus, modeStatus)), expected);
}

void AlertEvaluatorTest::scope_resolver_arguments()
{
    int seenDxcc = -1;
    QString seenBand;
    QString seenMode;
    int seenScope = -1;
    int calls = 0;

    AlertRule::LogStatusResolver resolver = [&](int dxcc, const QString &band, const QString &modeGroup, DxccStatusScope scope) -> DxccStatus
    {
        calls++;
        seenDxcc = dxcc;
        seenBand = band;
        seenMode = modeGroup;
        seenScope = static_cast<int>(scope);
        return DxccStatus::Worked;
    };

    /* DX Cluster spot - the mode group of the spot is passed */
    {
        RuleSpec ruleSpec;
        ruleSpec.dxLogStatusScope = static_cast<int>(DxccStatusScope::Band);
        auto rule = makeRule(ruleSpec, SpotAlert::DXSPOT);

        DxSpotSpec spec;
        spec.dxcc = 230;
        spec.band = QStringLiteral("40m");
        spec.modeGroupString = QStringLiteral("CW");
        spec.status = DxccStatus::NewEntity;
        DxSpot spot = makeDxSpot(spec);

        QVERIFY(rule->match(spot, resolver));
        QCOMPARE(calls, 1);
        QCOMPARE(seenDxcc, 230);
        QCOMPARE(seenBand, QStringLiteral("40m"));
        QCOMPARE(seenMode, QStringLiteral("CW"));
        QCOMPARE(seenScope, static_cast<int>(DxccStatusScope::Band));
    }

    /* WSJTX spot - FTx mode group is derived from the decoded mode */
    {
        RuleSpec ruleSpec;
        ruleSpec.dxLogStatusScope = static_cast<int>(DxccStatusScope::Entity);
        auto rule = makeRule(ruleSpec, SpotAlert::WSJTXCQSPOT);

        WsjtxSpec spec;
        spec.dxcc = 5;
        spec.band = QStringLiteral("6m");
        spec.decodedMode = QStringLiteral("FT4");
        spec.status = DxccStatus::Confirmed;
        WsjtxEntry entry = makeWsjtxEntry(spec);

        QVERIFY(rule->match(entry, resolver));
        QCOMPARE(calls, 2);
        QCOMPARE(seenDxcc, 5);
        QCOMPARE(seenBand, QStringLiteral("6m"));
        QCOMPARE(seenMode, BandPlan::MODE_GROUP_STRING_FTx);
        QCOMPARE(seenScope, static_cast<int>(DxccStatusScope::Entity));
    }

    /* Band & Mode scope never calls the resolver */
    {
        RuleSpec ruleSpec;
        ruleSpec.dxLogStatusScope = static_cast<int>(DxccStatusScope::BandMode);
        auto rule = makeRule(ruleSpec, SpotAlert::DXSPOT);

        DxSpotSpec spec;
        DxSpot spot = makeDxSpot(spec);

        QVERIFY(rule->match(spot, resolver));
        QCOMPARE(calls, 2);
    }
}

void AlertEvaluatorTest::scope_conversion()
{
    QCOMPARE(AlertRule::toLogStatusScope(0), DxccStatusScope::BandMode);
    QCOMPARE(AlertRule::toLogStatusScope(1), DxccStatusScope::Band);
    QCOMPARE(AlertRule::toLogStatusScope(2), DxccStatusScope::Entity);
    QCOMPARE(AlertRule::toLogStatusScope(3), DxccStatusScope::Mode);
    /* unknown values (e.g. NULL column of an older rule) fall back to the default */
    QCOMPARE(AlertRule::toLogStatusScope(-1), DxccStatusScope::BandMode);
    QCOMPARE(AlertRule::toLogStatusScope(99), DxccStatusScope::BandMode);
}

void AlertEvaluatorTest::logstatus_sentence_data()
{
    QTest::addColumn<int>("need");
    QTest::addColumn<int>("until");
    QTest::addColumn<int>("expectedMap");
    QTest::addColumn<int>("expectedScope");

    const int entity = static_cast<int>(AlertRule::LogStatusNeed::NewEntity);
    const int band = static_cast<int>(AlertRule::LogStatusNeed::NewBand);
    const int modeNeed = static_cast<int>(AlertRule::LogStatusNeed::NewMode);
    const int slot = static_cast<int>(AlertRule::LogStatusNeed::NewBandMode);
    const int any = static_cast<int>(AlertRule::LogStatusNeed::Any);
    const int worked = static_cast<int>(AlertRule::LogStatusUntil::Worked);
    const int confirmed = static_cast<int>(AlertRule::LogStatusUntil::Confirmed);
    const int allNew = DxccStatus::NewEntity | DxccStatus::NewBand | DxccStatus::NewMode | DxccStatus::NewSlot;

    QTest::newRow("entity_worked")    << entity << worked    << int(DxccStatus::NewEntity)                        << int(DxccStatusScope::Entity);
    QTest::newRow("entity_confirmed") << entity << confirmed << int(DxccStatus::NewEntity | DxccStatus::Worked)   << int(DxccStatusScope::Entity);
    QTest::newRow("band_worked")      << band   << worked    << int(DxccStatus::NewEntity | DxccStatus::NewBand)  << int(DxccStatusScope::Band);
    QTest::newRow("band_confirmed")   << band   << confirmed << int(DxccStatus::NewEntity | DxccStatus::NewBand | DxccStatus::Worked) << int(DxccStatusScope::Band);
    QTest::newRow("mode_worked")      << modeNeed << worked  << int(DxccStatus::NewEntity | DxccStatus::NewMode)  << int(DxccStatusScope::Mode);
    QTest::newRow("mode_confirmed")   << modeNeed << confirmed << int(DxccStatus::NewEntity | DxccStatus::NewMode | DxccStatus::Worked) << int(DxccStatusScope::Mode);
    QTest::newRow("slot_worked")      << slot   << worked    << allNew                                            << int(DxccStatusScope::BandMode);
    QTest::newRow("slot_confirmed")   << slot   << confirmed << (allNew | DxccStatus::Worked)                     << int(DxccStatusScope::BandMode);
    QTest::newRow("any_worked")       << any    << worked    << int(DxccStatus::All)                              << int(DxccStatusScope::BandMode);
    QTest::newRow("any_confirmed")    << any    << confirmed << int(DxccStatus::All)                              << int(DxccStatusScope::BandMode);
}

void AlertEvaluatorTest::logstatus_sentence()
{
    QFETCH(int, need);
    QFETCH(int, until);
    QFETCH(int, expectedMap);
    QFETCH(int, expectedScope);

    AlertRule rule;
    rule.setLogStatus(static_cast<AlertRule::LogStatusNeed>(need),
                      static_cast<AlertRule::LogStatusUntil>(until));

    QCOMPARE(rule.dxLogStatusMap, expectedMap);
    QCOMPARE(static_cast<int>(rule.dxLogStatusScope), expectedScope);

    /* the sentence can be read back from the stored values */
    QCOMPARE(static_cast<int>(rule.logStatusNeed()), need);

    if ( need != static_cast<int>(AlertRule::LogStatusNeed::Any) )
        QCOMPARE(static_cast<int>(rule.logStatusUntil()), until);

    /* the classic "New One" rule: the entity is worked on 15m but not confirmed anywhere,
     * a spot on 10m must alert only while the entity is not confirmed */
    if ( need == static_cast<int>(AlertRule::LogStatusNeed::NewEntity) )
    {
        RuleSpec ruleSpec;
        auto newOneRule = makeRule(ruleSpec, SpotAlert::DXSPOT);
        newOneRule->setLogStatus(static_cast<AlertRule::LogStatusNeed>(need),
                                 static_cast<AlertRule::LogStatusUntil>(until));

        DxSpotSpec spec;
        spec.band = "10m";
        spec.status = DxccStatus::NewBand;
        DxSpot spot = makeDxSpot(spec);

        const bool untilConfirmed = ( until == static_cast<int>(AlertRule::LogStatusUntil::Confirmed) );
        QCOMPARE(newOneRule->match(spot, makeResolver(DxccStatus::NewBand, DxccStatus::Worked)), untilConfirmed);
        QCOMPARE(newOneRule->match(spot, makeResolver(DxccStatus::NewBand, DxccStatus::Confirmed)), false);
        QCOMPARE(newOneRule->match(spot, makeResolver(DxccStatus::NewBand, DxccStatus::NewEntity)), true);
    }
}

void AlertEvaluatorTest::alarm_command_line()
{
    DxSpotSpec spec;
    spec.callsign = QStringLiteral("6Y9A");
    spec.band = QStringLiteral("10m");
    spec.modeGroupString = QStringLiteral("CW");
    DxSpot spot = makeDxSpot(spec);
    spot.freq = 28.0251;
    spot.dxcc.country = QStringLiteral("Jamaica");

    /* plain command, no placeholders */
    QCOMPARE(AlertRule::alarmCommandLine("morse -w 25 -f 700 -e DX", spot, "New One"),
             QStringList({"morse", "-w", "25", "-f", "700", "-e", "DX"}));

    /* placeholders are replaced per argument; a quoted argument stays one argument */
    QCOMPARE(AlertRule::alarmCommandLine("espeak \"new one {callsign} {country} on {band} {mode} {freq} {rule}\"", spot, "New One"),
             QStringList({"espeak", "new one 6Y9A Jamaica on 10m CW 28.025 New One"}));

    /* a country with a space inserted into an unquoted argument is still one argument */
    spot.dxcc.country = QStringLiteral("United States");
    QCOMPARE(AlertRule::alarmCommandLine("say {country}", spot, "r"),
             QStringList({"say", "United States"}));

    /* nothing to run */
    QVERIFY(AlertRule::alarmCommandLine("", spot, "r").isEmpty());
    QVERIFY(AlertRule::alarmCommandLine("   ", spot, "r").isEmpty());

    /* the list of placeholders documented in the dialog is the list that is replaced */
    QCOMPARE(AlertRule::ALARM_PLACEHOLDERS.size(), 6);
    for ( const QString &placeholder : AlertRule::ALARM_PLACEHOLDERS )
        QVERIFY2(!AlertRule::alarmCommandLine("x " + placeholder, spot, "r").last().contains('{'),
                 qPrintable(placeholder));
}

void AlertEvaluatorTest::alarm_backoff()
{
    QHash<QString, QDateTime> history;
    const QDateTime t0 = QDateTime(QDate(2026, 9, 20), QTime(12, 0, 0), Qt::UTC);

    /* first time always fires */
    QVERIFY(AlertEvaluator::alarmDue(history, "rule", 300, "6Y9A", t0));
    /* same rule and callsign within the backoff: silent */
    QVERIFY(!AlertEvaluator::alarmDue(history, "rule", 300, "6Y9A", t0.addSecs(10)));
    QVERIFY(!AlertEvaluator::alarmDue(history, "rule", 300, "6Y9A", t0.addSecs(299)));
    /* another callsign of the same rule: fires */
    QVERIFY(AlertEvaluator::alarmDue(history, "rule", 300, "ZL7X", t0.addSecs(10)));
    /* the same callsign via another rule: fires */
    QVERIFY(AlertEvaluator::alarmDue(history, "other", 300, "6Y9A", t0.addSecs(10)));
    /* after the backoff: fires again and restarts the window */
    QVERIFY(AlertEvaluator::alarmDue(history, "rule", 300, "6Y9A", t0.addSecs(300)));
    QVERIFY(!AlertEvaluator::alarmDue(history, "rule", 300, "6Y9A", t0.addSecs(400)));
    /* backoff 0: every time */
    QVERIFY(AlertEvaluator::alarmDue(history, "rule", 0, "6Y9A", t0.addSecs(401)));
    QVERIFY(AlertEvaluator::alarmDue(history, "rule", 0, "6Y9A", t0.addSecs(401)));
    /* an empty command line is never started */
    QVERIFY(!AlertEvaluator::startAlarm(QStringList()));

    /* stored alarm types; unknown values (older rules) mean no alarm */
    QCOMPARE(AlertRule::toAlarmType(0), AlertRule::AlarmType::None);
    QCOMPARE(AlertRule::toAlarmType(1), AlertRule::AlarmType::Bell);
    QCOMPARE(AlertRule::toAlarmType(2), AlertRule::AlarmType::Command);
    QCOMPARE(AlertRule::toAlarmType(7), AlertRule::AlarmType::None);
}

QTEST_APPLESS_MAIN(AlertEvaluatorTest)

#include "tst_alertevaluator.moc"
