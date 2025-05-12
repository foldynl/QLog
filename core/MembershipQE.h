#ifndef QLOG_CORE_MEMBERSHIPQE_H
#define QLOG_CORE_MEMBERSHIPQE_H

#include <QObject>
#include <QDate>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include <QSqlQuery>

class ClubInfo
{
public:
    explicit ClubInfo(const QString &callsign,
                    const QString &ID,
                    const QDate &validFrom,
                    const QDate &validTo,
                    const QString &club);
    const QString& getCallsign() const;
    const QString& getID() const;
    const QDate& getValidFrom() const;
    const QDate& getValidTo() const;
    const QString& getClubInfo() const;

private:
    QString callsign;
    QString id;
    QDate validFrom;
    QDate validTo;
    QString club;
};

class ClubStatusQuery : public QObject
{
    Q_OBJECT

public:
    explicit ClubStatusQuery(QObject *parent = nullptr);
    ~ClubStatusQuery();

    enum ClubStatus
    {
        NewClub           = 0b1,
        NewBandClub       = 0b10,
        NewModeClub       = 0b100,
        NewBandModeClub   = 0b110,
        NewSlotClub       = 0b1000,
        WorkedClub        = 0b10000,
        ConfirmedClub     = 0b100000,
        UnknownStatusClub = 0b1000000,
        AllClub           = 0b1111111
    };

    Q_ENUM(ClubStatus);

    struct ClubInfo
    {
        ClubInfo(){status = UnknownStatusClub;};
        ClubInfo(const ClubStatus status, const QString &memberID) :
            status(status), membershipID(memberID) {};

        ClubStatus status;
        QString membershipID;
    };

public slots:
    void getClubStatus(const QString &callsign,
                       const QString &band,
                       const QString &mode,
                       bool lowtConfirmed,
                       bool paperConfirmed,
                       bool eqslConfirmed);

signals:
    void status(QString, QMap<QString, ClubStatusQuery::ClubInfo>);

private:
    const QString dbConnectionName;
    bool dbConnected;
    QMutex dbLock;

    ClubStatus determineClubStatus(bool, bool, bool, bool, unsigned long);
};

Q_DECLARE_METATYPE(ClubStatusQuery::ClubInfo)

class MembershipQE : public QObject
{
    Q_OBJECT

public:
    static MembershipQE *instance()
    {
        static MembershipQE instance;
        return &instance;
    };
    static void saveEnabledClubLists(const QStringList &enabledLists);
    static QStringList getEnabledClubLists();

    // return only list of clubs where callsign is a member.
    QList<ClubInfo> query(const QString &callsign);

    // return Status for each club
    // Membership status details can take a long time (depend on the number of records in the log and membership lists)
    // therefore qlog runs this query in an isolated thread (do not block the main thread). The result is returned via clubStatusResult signal - if exists
    void asyncQueryDetails(const QString &callsign, const QString &band, const QString &mode);

    void updateLists();

signals:
    void clubStatusResult(QString, QMap<QString, ClubStatusQuery::ClubInfo>);

private:
    MembershipQE(QObject *parent = nullptr);
    ~MembershipQE();

private slots:
    void statusQueryFinished(const QString &,
                             QMap<QString, ClubStatusQuery::ClubInfo>);
    void onFinishedListDownload(QNetworkReply *);

private:
    ClubStatusQuery statusQuery;
    QThread statusQueryThread;

    bool removeDisabled(const QStringList &enabledLists);
    bool planDownloads(const QStringList &enabledLists);
    void startPlannedDownload();
    bool importData(const QString &clubid, const QByteArray &data);
    void removeClubsFromEnabledClubLists(const QList<QPair<QString, QString>> &toRemove);

    QSqlQuery clubQuery;
    bool idClubQueryValid;
    QList<QPair<QString, QString>> updatePlan;
    QScopedPointer<QNetworkAccessManager> nam;

    static QString CONFIG_MEMBERLIST_ENABLED;
};

#endif // QLOG_CORE_MEMBERSHIPQE_H
