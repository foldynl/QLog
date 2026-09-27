#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QLoggingCategory>
#include <memory>

#define private public
#include "core/AlertEvaluator.h"
#undef private

#include "notification/AlertNotification.h"
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
    int dxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    int dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    bool dxAlertGoalSet = false;
    bool dxAlertContextSet = false;
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
    QString modeGroupString = QStringLiteral("FTx");
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
    rule->dxAlertGoal = spec.dxAlertGoal;
    rule->dxAlertContext = spec.dxAlertContext;
    rule->dxAlertGoalSet = spec.dxAlertGoalSet;
    rule->dxAlertContextSet = spec.dxAlertContextSet;
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
    entry.modeGroupString = spec.modeGroupString;
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
    void newDxccConditions();
    void newDxccConditionWsjtx();
    void newDxccConditionFailures();
    void newDxccConditionSatellite();
    void notificationsAreAggregatedAndRepeatedPerBandMode();
    void confirmationResolverIsSharedAcrossRules();
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

void AlertEvaluatorTest::newDxccConditions()
{
    RuleSpec ruleSpec;
    ruleSpec.dxAlertGoalSet = true;
    ruleSpec.dxAlertContextSet = true;

    DxSpotSpec spotSpec;
    int resolverCalls = 0;
    DxccConfirmationResult resolverResult = DxccConfirmationResult::NotConfirmed;
    const DxccConfirmationResolver resolver =
        [&resolverCalls, &resolverResult](int, const QString &, const QString &, AlertDxccContext)
        {
            ++resolverCalls;
            return resolverResult;
        };

    ruleSpec.dxAlertGoal = static_cast<int>(AlertDxccGoal::Worked);
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    spotSpec.status = DxccStatus::NewEntity;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    spotSpec.status = DxccStatus::Worked;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));

    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Band);
    spotSpec.status = DxccStatus::NewBand;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));

    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Mode);
    spotSpec.status = DxccStatus::NewMode;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));

    spotSpec.status = DxccStatus::NewSlot;
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Band);
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Mode);
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::BandMode);
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    spotSpec.status = DxccStatus::Worked;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    QCOMPARE(resolverCalls, 0);

    ruleSpec.dxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);
    spotSpec.status = DxccStatus::NewBand;
    DxSpot spot = makeDxSpot(spotSpec);
    resolverResult = DxccConfirmationResult::NotConfirmed;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(spot, resolver));
    QCOMPARE(spot.status, DxccStatus::NewBand);
    QCOMPARE(resolverCalls, 1);

    resolverResult = DxccConfirmationResult::Confirmed;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(spot, resolver));
    QCOMPARE(resolverCalls, 2);

    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Band);
    spotSpec.status = DxccStatus::NewBand;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    QCOMPARE(resolverCalls, 2);

    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Mode);
    spotSpec.status = DxccStatus::NewMode;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    QCOMPARE(resolverCalls, 2);

    spotSpec.status = DxccStatus::Worked;
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Band);
    resolverResult = DxccConfirmationResult::NotConfirmed;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    resolverResult = DxccConfirmationResult::Confirmed;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));

    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Mode);
    resolverResult = DxccConfirmationResult::NotConfirmed;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    resolverResult = DxccConfirmationResult::Confirmed;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));

    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::BandMode);
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    spotSpec.status = DxccStatus::Confirmed;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), resolver));
    QCOMPARE(resolverCalls, 6);
}

void AlertEvaluatorTest::newDxccConditionWsjtx()
{
    RuleSpec ruleSpec;
    ruleSpec.dxAlertGoalSet = true;
    ruleSpec.dxAlertContextSet = true;
    ruleSpec.dxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Mode);

    WsjtxSpec spotSpec;
    spotSpec.status = DxccStatus::Worked;
    spotSpec.modeGroupString = BandPlan::MODE_GROUP_STRING_FTx;

    int resolverCalls = 0;
    int resolvedDxcc = 0;
    QString resolvedBand;
    QString resolvedMode;
    AlertDxccContext resolvedContext = AlertDxccContext::Anywhere;
    const DxccConfirmationResolver resolver =
        [&resolverCalls, &resolvedDxcc, &resolvedBand, &resolvedMode, &resolvedContext]
        (int dxcc, const QString &band, const QString &mode, AlertDxccContext context)
        {
            ++resolverCalls;
            resolvedDxcc = dxcc;
            resolvedBand = band;
            resolvedMode = mode;
            resolvedContext = context;
            return DxccConfirmationResult::NotConfirmed;
        };

    QVERIFY(makeRule(ruleSpec, SpotAlert::WSJTXCQSPOT)->match(makeWsjtxEntry(spotSpec), resolver));
    QCOMPARE(resolverCalls, 1);
    QCOMPARE(resolvedDxcc, 123);
    QCOMPARE(resolvedBand, QStringLiteral("20m"));
    QCOMPARE(resolvedMode, BandPlan::MODE_GROUP_STRING_FTx);
    QCOMPARE(resolvedContext, AlertDxccContext::Mode);
}

void AlertEvaluatorTest::newDxccConditionFailures()
{
    RuleSpec ruleSpec;
    ruleSpec.dxAlertGoalSet = true;
    ruleSpec.dxAlertContextSet = true;
    ruleSpec.dxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Anywhere);

    DxSpotSpec spotSpec;
    spotSpec.status = DxccStatus::Worked;

    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec)));

    const DxccConfirmationResolver errorResolver =
        [](int, const QString &, const QString &, AlertDxccContext)
        {
            return DxccConfirmationResult::Error;
        };
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), errorResolver));

    int resolverCalls = 0;
    const DxccConfirmationResolver countingResolver =
        [&resolverCalls](int, const QString &, const QString &, AlertDxccContext)
        {
            ++resolverCalls;
            return DxccConfirmationResult::Confirmed;
        };
    ruleSpec.dxAlertGoalSet = false;
    ruleSpec.dxAlertContextSet = false;
    ruleSpec.dxLogStatusMap = DxccStatus::All;
    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), countingResolver));
    QCOMPARE(resolverCalls, 0);

    ruleSpec.dxAlertGoalSet = true;
    ruleSpec.dxAlertContextSet = false;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(makeDxSpot(spotSpec), countingResolver));
    QCOMPARE(resolverCalls, 0);
}

void AlertEvaluatorTest::newDxccConditionSatellite()
{
    RuleSpec ruleSpec;
    ruleSpec.dxAlertGoalSet = true;
    ruleSpec.dxAlertContextSet = true;
    ruleSpec.dxAlertGoal = static_cast<int>(AlertDxccGoal::Confirmed);
    ruleSpec.dxAlertContext = static_cast<int>(AlertDxccContext::Band);

    DxSpotSpec spotSpec;
    spotSpec.status = DxccStatus::Worked;
    DxSpot spot = makeDxSpot(spotSpec);
    spot.dxccStatusSatellite = true;

    int resolverCalls = 0;
    const DxccConfirmationResolver resolver =
        [&resolverCalls](int, const QString &, const QString &, AlertDxccContext)
        {
            ++resolverCalls;
            return DxccConfirmationResult::Confirmed;
        };

    QVERIFY(makeRule(ruleSpec, SpotAlert::DXSPOT)->match(spot, resolver));
    QCOMPARE(resolverCalls, 0);

    spot.status = DxccStatus::Confirmed;
    QVERIFY(!makeRule(ruleSpec, SpotAlert::DXSPOT)->match(spot, resolver));
    QCOMPARE(resolverCalls, 0);
}

void AlertEvaluatorTest::notificationsAreAggregatedAndRepeatedPerBandMode()
{
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    database.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY2(database.open(), qPrintable(database.lastError().text()));

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE alert_rules (rule_name TEXT PRIMARY KEY, "
        "notification_enabled INTEGER, notification_sound_enabled INTEGER, "
        "notification_repeat_minutes INTEGER)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO alert_rules VALUES "
        "('New DXCC', 1, 0, 5), ('CW Wanted', 1, 1, 5), "
        "('Muted rule', 0, 1, 5)")));

    {
        AlertNotification notifier;
        notifier.setEnabled(true);

        DxSpot spot;
        spot.callsign = QStringLiteral("3Y0K");
        spot.band = QStringLiteral("20m");
        spot.modeGroupString = QStringLiteral("CW");
        spot.dxcc.country = QStringLiteral("Bouvet Island");
        spot.freq = 14.025;
        SpotAlert alert({QStringLiteral("New DXCC"),
                         QStringLiteral("CW Wanted"),
                         QStringLiteral("Muted rule")}, spot);

        qRegisterMetaType<DesktopNotificationCategory>("DesktopNotificationCategory");
        QSignalSpy notificationSpy(&notifier, &AlertNotification::notificationRequested);
        notifier.processSpotAlert(alert);
        QCOMPARE(notificationSpy.size(), 1);
        const QList<QVariant> notification = notificationSpy.takeFirst();
        QCOMPARE(notification.at(0).value<DesktopNotificationCategory>(),
                 DesktopNotificationCategory::ALERT);
        QVERIFY(notification.at(1).toString().contains(QStringLiteral("3Y0K")));
        QVERIFY(notification.at(1).toString().contains(QStringLiteral("New DXCC")));
        QVERIFY(notification.at(1).toString().contains(QStringLiteral("CW Wanted")));
        QVERIFY(!notification.at(1).toString().contains(QStringLiteral("Muted rule")));
        QCOMPARE(notification.at(3).toBool(), true);

        notifier.processSpotAlert(alert);
        QCOMPARE(notificationSpy.size(), 0);

        notifier.clearAlertHistory();
        notifier.processSpotAlert(alert);
        QCOMPARE(notificationSpy.size(), 1);
        notificationSpy.clear();

        alert.spot.band = QStringLiteral("15m");
        notifier.processSpotAlert(alert);
        QCOMPARE(notificationSpy.size(), 1);
        notificationSpy.clear();

        alert.spot.modeGroupString = QStringLiteral("PHONE");
        notifier.processSpotAlert(alert);
        QCOMPARE(notificationSpy.size(), 1);
        notificationSpy.clear();

        alert.spot.band = QStringLiteral("10m");
        SpotAlert silentAlert({QStringLiteral("New DXCC")}, alert.spot);
        notifier.processSpotAlert(silentAlert);
        QCOMPARE(notificationSpy.size(), 1);
        QCOMPARE(notificationSpy.takeFirst().at(3).toBool(), false);

        notifier.setEnabled(false);
        alert.spot.band = QStringLiteral("6m");
        notifier.processSpotAlert(alert);
        QCOMPARE(notificationSpy.size(), 0);
    }

    query = QSqlQuery();
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void AlertEvaluatorTest::confirmationResolverIsSharedAcrossRules()
{
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    database.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY2(database.open(), qPrintable(database.lastError().text()));

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE alert_rules ("
        "rule_name TEXT PRIMARY KEY, enabled INTEGER, notification_enabled INTEGER, "
        "notification_sound_enabled INTEGER, notification_repeat_minutes INTEGER, "
        "source INTEGER, dx_callsign TEXT, "
        "dx_country INTEGER, dx_logstatus INTEGER, dx_alert_goal INTEGER, dx_alert_context INTEGER, "
        "dx_continent TEXT, spot_comment TEXT, mode TEXT, band TEXT, spotter_country INTEGER, "
        "spotter_continent TEXT, dx_member TEXT, ituz INTEGER, cqz INTEGER, "
        "pota INTEGER, sota INTEGER, iota INTEGER, wwff INTEGER)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO alert_rules VALUES "
        "('first', 1, 1, 1, 5, 1, '.*', 0, 127, 1, 0, '*', '.*', '*', '*', 0, '*', '*', 0, 0, 0, 0, 0, 0), "
        "('second', 1, 1, 1, 5, 1, '.*', 0, 127, 1, 0, '*', '.*', '*', '*', 0, '*', '*', 0, 0, 0, 0, 0, 0)")));

    int resolverCalls = 0;
    {
        AlertEvaluator evaluator;
        evaluator.setDxccConfirmationResolver(
            [&resolverCalls](int, const QString &, const QString &, AlertDxccContext)
            {
                ++resolverCalls;
                return DxccConfirmationResult::NotConfirmed;
            });

        DxSpotSpec spotSpec;
        spotSpec.status = DxccStatus::Worked;
        evaluator.dxSpot(makeDxSpot(spotSpec));
    }

    QCOMPARE(resolverCalls, 1);

    AlertRule storedRule;
    QVERIFY(storedRule.load(QStringLiteral("first")));
    QCOMPARE(storedRule.notificationEnabled, true);
    QCOMPARE(storedRule.notificationSoundEnabled, true);
    QCOMPARE(storedRule.notificationRepeatMinutes, 5);
    storedRule.notificationEnabled = false;
    storedRule.notificationSoundEnabled = false;
    storedRule.notificationRepeatMinutes = 17;
    QVERIFY(storedRule.save());

    AlertRule updatedRule;
    QVERIFY(updatedRule.load(QStringLiteral("first")));
    QCOMPARE(updatedRule.notificationEnabled, false);
    QCOMPARE(updatedRule.notificationSoundEnabled, false);
    QCOMPARE(updatedRule.notificationRepeatMinutes, 17);

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}


QTEST_APPLESS_MAIN(AlertEvaluatorTest)

#include "tst_alertevaluator.moc"
