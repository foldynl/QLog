#ifndef QLOG_UI_QSODETAILDIALOG_H
#define QLOG_UI_QSODETAILDIALOG_H

#include <QDialog>
#include <QDataWidgetMapper>
#include <QItemDelegate>
#include <QLabel>
#include <QPointer>
#include <QCompleter>
#include <QWebChannel>

#include "models/LogbookModel.h"
#include "core/Gridsquare.h"
#include "core/CallbookManager.h"
#include "ui/MapWebChannelHandler.h"
#include "ui/WebEnginePage.h"
#include "core/MembershipQE.h"
#include "core/LogLocale.h"
#include "core/MultiselectCompleter.h"

namespace Ui {
class QSODetailDialog;
}

class QSOEditMapperDelegate : public QItemDelegate
{
        Q_OBJECT
public:
    QSOEditMapperDelegate(QObject *parent = 0) : QItemDelegate(parent) {};
    void setEditorData(QWidget *editor,
                           const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                          QAbstractItemModel *model,
                          const QModelIndex &index) const override;
signals:
    void keyEscapePressed(QObject *);

private:
    bool eventFilter(QObject *object, QEvent *event) override;
    LogLocale locale;
};

class QSODetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QSODetailDialog(const QSqlRecord &qso,
                             QWidget *parent = nullptr);
    void accept() override;
    void keyPressEvent(QKeyEvent *evt) override;
    ~QSODetailDialog();

signals:
    void contactUpdated(QSqlRecord&);

private slots:
    void editButtonPressed();
    void resetButtonPressed();
    void lookupButtonPressed();
    void resetKeyPressed(QObject *);
    void setReadOnlyMode(bool);
    void modeChanged(QString);
    void showPaperButton();
    void showEQSLButton();
    void dateTimeOnChanged(const QDateTime &);
    void dateTimeOffChanged(const QDateTime &);
    void freqTXChanged(double);
    void freqRXChanged(double);
    void timeLockToggled(bool);
    void freqLockToggled(bool);
    void callsignChanged(const QString&);
    void callsignEditFinished();
    void queryMemberList();
    void propagationModeChanged(const QString &);
    bool doValidation();
    void doValidationDateTime(const QDateTime&);
    void doValidationDouble(double);
    void mapLoaded(bool);
    void myGridChanged(const QString&);
    void DXGridChanged(const QString&);
    void callsignFound(const QMap<QString, QString>& data);
    void callsignNotFound(const QString&);
    void callbookLoginFailed(const QString&);
    void callbookError(const QString&);
    void handleBeforeUpdate(int, QSqlRecord&);
    void sotaChanged(const QString&);
    void potaChanged(const QString&);
    void wwffChanged(const QString&);
    void mySotaChanged(const QString&);
    void myPOTAChanged(const QString&);
    void myWWFFChanged(const QString&);
    void clubQueryResult(const QString &in_callsign,
                         QMap<QString, ClubStatusQuery::ClubInfo> data);

private:
    /* It is modified logbook model when only basic
     * validation are done. The extended validations
     * are done in Form itself */
    class LogbookModelPrivate : public LogbookModel
    {

    public:
        explicit LogbookModelPrivate(QObject* parent = nullptr, QSqlDatabase db = QSqlDatabase());

        QVariant data(const QModelIndex &index, int role) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    };

    enum SubmitError
    {
        SubmitOK = 0,
        SubmitCancelledByUser = 1,
        SubmitMapperError = 2,
        SubmitModelError = 3
    };

    bool highlightInvalid(QLabel *, bool, const QString&);
    void blockMappedWidgetSignals(bool);
    void drawDXOnMap(const QString &label, const Gridsquare &dxGrid);
    void drawMyQTHOnMap(const QString &label, const Gridsquare &myGrid);
    void setStaticMapTime(const QDateTime &dateTime);
    void enableWidgetChangeHandlers();
    void lookupButtonWaitingStyle(bool);
    SubmitError submitAllChanges();
    void callbookLookupFinished();
    void callbookLookupStart();
    void refreshDXStatTabs();
    const QString getButtonText(int index) const;

    Ui::QSODetailDialog *ui;
    QPointer<QDataWidgetMapper> mapper;
    QPointer<LogbookModelPrivate> model;
    QSqlRecord *editedRecord;
    QPointer<QPushButton> editButton;
    QPointer<QPushButton> resetButton;
    QPointer<QPushButton> lookupButton;
    QPointer<QMovie> lookupButtonMovie;
    qint64 timeLockDiff;
    double freqLockDiff;
    bool isMainPageLoaded;
    QPointer<WebEnginePage> main_page;
    QString postponedScripts;
    CallbookManager callbookManager;
    QScopedPointer<QCompleter> iotaCompleter;
    QScopedPointer<QCompleter> myIotaCompleter;
    QScopedPointer<QCompleter> sotaCompleter;
    QScopedPointer<QCompleter> mySotaCompleter;
    QScopedPointer<MultiselectCompleter> potaCompleter;
    QScopedPointer<QCompleter> myPotaCompleter;
    QScopedPointer<QCompleter> wwffCompleter;
    QScopedPointer<QCompleter> myWWFFCompleter;
    QScopedPointer<QCompleter> sigCompleter;
    QWebChannel channel;
    MapWebChannelHandler layerControlHandler;
    LogLocale locale;
};

#endif // QLOG_UI_QSODETAILDIALOG_H
