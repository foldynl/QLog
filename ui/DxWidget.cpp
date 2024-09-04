#include <QDebug>
#include <QColor>
#include <QSettings>
#include <QMessageBox>
#include <QFontMetrics>
#ifdef Q_OS_WIN
#include <Ws2tcpip.h>
#include <winsock2.h>
#include <Mstcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#include <QMenu>

#include "DxWidget.h"
#include "ui_DxWidget.h"
#include "data/Data.h"
#include "DxFilterDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "data/StationProfile.h"
#include "data/WCYSpot.h"
#include "data/WWVSpot.h"
#include "data/ToAllSpot.h"
#include "ui/ColumnSettingDialog.h"
#include "core/CredentialStore.h"
#include "ui/InputPasswordDialog.h"
#include "data/BandPlan.h"
#include "core/DxServerString.h"
#include "rig/macros.h"
#include "core/Callsign.h"

#define CONSOLE_VIEW 4
#define NUM_OF_RECONNECT_ATTEMPTS 3
#define RECONNECT_TIMEOUT 10000

MODULE_IDENTIFICATION("qlog.ui.dxwidget");

int DxTableModel::rowCount(const QModelIndex&) const
{
    return dxData.count();
}

int DxTableModel::columnCount(const QModelIndex&) const
{
    return 11;
}

QVariant DxTableModel::data(const QModelIndex& index, int role) const
{
    if ( role == Qt::DisplayRole )
    {
        const DxSpot &spot = dxData.at(index.row());
        switch ( index.column() )
        {
        case 0:
            return spot.time.toString(locale.formatTimeLongWithoutTZ());
        case 1:
            return spot.callsign;
        case 2:
            return QString::number(spot.freq, 'f', 4);
        case 3:
            return spot.modeGroupString;
        case 4:
            return spot.spotter;
        case 5:
            return spot.comment;
        case 6:
            return spot.dxcc.cont;
        case 7:
            return spot.dxcc_spotter.cont;
        case 8:
            return spot.band;
        case 9:
            return spot.memberList2StringList().join(", ");
        case 10:
            return QCoreApplication::translate("DBStrings", spot.dxcc.country.toUtf8().constData());
        default:
            return QVariant();
        }
    }
    else if (index.column() == 1 && role == Qt::BackgroundRole)
    {
        const DxSpot &spot = dxData.at(index.row());
        return Data::statusToColor(spot.status, QColor(Qt::transparent));
    }
    else if (index.column() == 1 && role == Qt::ToolTipRole)
    {
        const DxSpot &spot = dxData.at(index.row());
        return QCoreApplication::translate("DBStrings", spot.dxcc.country.toUtf8().constData()) + " [" + Data::statusToText(spot.status) + "]";
    }
    return QVariant();
}

QVariant DxTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch ( section )
    {
    case 0: return tr("Time");
    case 1: return tr("Callsign");
    case 2: return tr("Frequency");
    case 3: return tr("Mode");
    case 4: return tr("Spotter");
    case 5: return tr("Comment");
    case 6: return tr("Continent");
    case 7: return tr("Spotter Continent");
    case 8: return tr("Band");
    case 9: return tr("Member");
    case 10: return tr("Country");

    default: return QVariant();
    }
}

bool DxTableModel::addEntry(DxSpot entry, bool deduplicate,
                            qint16 dedup_interval, double dedup_freq_tolerance)
{
    bool shouldInsert = true;
    if ( deduplicate )
    {
        for (const DxSpot &record : qAsConst(dxData))
        {
            if ( record.time.secsTo(entry.time) > dedup_interval )
                break;

            if ( record.callsign == entry.callsign
                 && qAbs(MHz(record.freq) - MHz(entry.freq)) < kHz(dedup_freq_tolerance) )
            {
                qCDebug(runtime) << "Duplicate spot" << record.callsign << record.freq <<  entry.callsign << entry.freq;
                shouldInsert = false;
                break;
            }
        }
    }

    if ( shouldInsert )
    {
        beginInsertRows(QModelIndex(), 0, 0);
        dxData.prepend(entry);
        endInsertRows();
    }

    return shouldInsert;
}

QString DxTableModel::getCallsign(const QModelIndex& index)
{
    return dxData.at(index.row()).callsign;
}

double DxTableModel::getFrequency(const QModelIndex& index)
{
    return dxData.at(index.row()).freq;
}

BandPlan::BandPlanMode DxTableModel::getBandPlanode(const QModelIndex &index)
{
    return dxData.at(index.row()).bandPlanMode;
}

void DxTableModel::clear()
{
    beginResetModel();
    dxData.clear();
    endResetModel();
}

int WCYTableModel::rowCount(const QModelIndex&) const
{
    return wcyData.count();
}

int WCYTableModel::columnCount(const QModelIndex&) const
{
    return 9;
}

QVariant WCYTableModel::data(const QModelIndex& index, int role) const
{
    if ( role == Qt::DisplayRole )
    {
        const WCYSpot &spot = wcyData.at(index.row());

        switch ( index.column() )
        {
        case 0:
            return spot.time.toString(locale.formatTimeLongWithoutTZ());
        case 1:
            return spot.KIndex;
        case 2:
            return spot.expK;
        case 3:
            return spot.AIndex;
        case 4:
            return spot.RIndex;
        case 5:
            return spot.SFI;
        case 6:
            return spot.SA;
        case 7:
            return spot.GMF;
        case 8:
            return spot.Au;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

QVariant WCYTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch ( section )
    {
    case 0: return tr("Time");
    case 1: return tr("K");
    case 2: return tr("expK");
    case 3: return tr("A");
    case 4: return tr("R");
    case 5: return tr("SFI");
    case 6: return tr("SA");
    case 7: return tr("GMF");
    case 8: return tr("Au");

    default: return QVariant();
    }
}

void WCYTableModel::addEntry(WCYSpot entry)
{
    beginInsertRows(QModelIndex(), 0, 0);
    wcyData.prepend(entry);
    endInsertRows();
}

void WCYTableModel::clear()
{
    beginResetModel();
    wcyData.clear();
    endResetModel();
}

int WWVTableModel::rowCount(const QModelIndex&) const
{
    return wwvData.count();
}

int WWVTableModel::columnCount(const QModelIndex&) const
{
    return 5;
}

QVariant WWVTableModel::data(const QModelIndex& index, int role) const
{

    if ( role == Qt::DisplayRole )
    {
        const WWVSpot &spot = wwvData.at(index.row());

        switch ( index.column() )
        {
        case 0:
            return spot.time.toString(locale.formatTimeLongWithoutTZ());
        case 1:
            return spot.SFI;
        case 2:
            return spot.AIndex;
        case 3:
            return spot.KIndex;
        case 4:
            return spot.info1 + " " + QChar(0x2192) + " " + spot.info2;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

QVariant WWVTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch ( section )
    {
    case 0: return tr("Time");
    case 1: return tr("SFI");
    case 2: return tr("A");
    case 3: return tr("K");
    case 4: return tr("Info");

    default: return QVariant();
    }
}

void WWVTableModel::addEntry(WWVSpot entry)
{
    beginInsertRows(QModelIndex(), 0, 0);
    wwvData.prepend(entry);
    endInsertRows();
}

void WWVTableModel::clear()
{
    beginResetModel();
    wwvData.clear();
    endResetModel();
}

int ToAllTableModel::rowCount(const QModelIndex&) const
{
    return toAllData.count();
}

int ToAllTableModel::columnCount(const QModelIndex&) const
{
    return 3;
}

QVariant ToAllTableModel::data(const QModelIndex& index, int role) const
{

    if ( role == Qt::DisplayRole )
    {
        const ToAllSpot &spot = toAllData.at(index.row());

        switch (index.column()) {
        case 0:
            return spot.time.toString(locale.formatTimeLongWithoutTZ());
        case 1:
            return spot.spotter;
        case 2:
            return spot.message;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

QVariant ToAllTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch ( section )
    {
    case 0: return tr("Time");
    case 1: return tr("Spotter");
    case 2: return tr("Message");

    default: return QVariant();
    }
}

void ToAllTableModel::addEntry(ToAllSpot entry)
{
    beginInsertRows(QModelIndex(), 0, 0);
    toAllData.prepend(entry);
    endInsertRows();
}

void ToAllTableModel::clear()
{
    beginResetModel();
    toAllData.clear();
    endResetModel();
}

bool DeleteHighlightedDXServerWhenDelPressedEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    if ( event->type() == QEvent::KeyPress )
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key::Key_Delete && keyEvent->modifiers() == Qt::ControlModifier)
        {
            if ( dynamic_cast<QComboBox *>(obj) )
            {
                emit deleteServerItem();
                return true;
            }
        }
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}
/****************************************************/
DxWidget::DxWidget(QWidget *parent) :
    QWidget(parent),
    socket(nullptr),
    ui(new Ui::DxWidget),
    deduplicateSpots(false),
    reconnectAttempts(0),
    connectionState(DISCONNECTED),
    connectedServerString(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->serverSelect->setStyleSheet(QStringLiteral("QComboBox {color: red}"));

    dxTableModel = new DxTableModel(this);
    wcyTableModel = new WCYTableModel(this);
    wwvTableModel = new WWVTableModel(this);
    toAllTableModel = new ToAllTableModel(this);

    QAction *separator = new QAction(this);
    separator->setSeparator(true);

    ui->dxTable->setModel(dxTableModel);
    ui->dxTable->addAction(ui->actionFilter);
    ui->dxTable->addAction(ui->actionDisplayedColumns);
    ui->dxTable->addAction(ui->actionClear);
    ui->dxTable->addAction(separator);
    ui->dxTable->addAction(ui->actionKeepSpots);
    ui->dxTable->hideColumn(6);  //continent
    ui->dxTable->hideColumn(7);  //spotter continen
    ui->dxTable->hideColumn(8);  //band
    ui->dxTable->hideColumn(9);  //Memberships
    ui->dxTable->hideColumn(10); //Country
    ui->dxTable->horizontalHeader()->setSectionsMovable(true);

    ui->wcyTable->setModel(wcyTableModel);
    ui->wcyTable->addAction(ui->actionDisplayedColumns);
    ui->wcyTable->addAction(ui->actionClear);
    ui->wcyTable->horizontalHeader()->setSectionsMovable(true);

    ui->wwvTable->setModel(wwvTableModel);
    ui->wwvTable->addAction(ui->actionDisplayedColumns);
    ui->wwvTable->addAction(ui->actionClear);
    ui->wwvTable->horizontalHeader()->setSectionsMovable(true);

    ui->toAllTable->setModel(toAllTableModel);
    ui->toAllTable->addAction(ui->actionDisplayedColumns);
    ui->toAllTable->addAction(ui->actionClear);
    ui->toAllTable->horizontalHeader()->setSectionsMovable(true);

    moderegexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    contregexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    spottercontregexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    bandregexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    reloadSetting();
    serverComboSetup();

    QMenu *commandsMenu = new QMenu(this);
    commandsMenu->addAction(ui->actionSpotQSO);
    commandsMenu->addSeparator();
    commandsMenu->addAction(ui->actionShowHFStats);
    commandsMenu->addAction(ui->actionShowVHFStats);
    commandsMenu->addAction(ui->actionShowWCY);
    commandsMenu->addAction(ui->actionShowWWV);
    ui->commandButton->setMenu(commandsMenu);
    ui->commandButton->setDefaultAction(ui->actionSpotQSO);
    ui->commandButton->setEnabled(false);

    QMenu *mainWidgetMenu = new QMenu(this);
    mainWidgetMenu->addAction(ui->actionDeleteServer);
    mainWidgetMenu->addAction(ui->actionForgetPassword);
    mainWidgetMenu->addSeparator();
    mainWidgetMenu->addAction(ui->actionConnectOnStartup);
    ui->menuButton->setMenu(mainWidgetMenu);

    reconnectTimer.setInterval(RECONNECT_TIMEOUT);
    reconnectTimer.setSingleShot(true);
    connect(&reconnectTimer, &QTimer::timeout, this, &DxWidget::connectCluster);

    restoreWidgetSetting();

    ui->actionConnectOnStartup->setChecked(getAutoconnectServer());
    ui->actionKeepSpots->setChecked(getKeepQSOs());
}

void DxWidget::toggleConnect()
{
    FCT_IDENTIFICATION;

    if ( (socket && socket->isOpen())
         || reconnectAttempts )
    {
        disconnectCluster();
    }
    else
    {
        if ( !DxServerString::isValidServerString(ui->serverSelect->currentText()) )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("DXC Server Name Error"),
                                          QMessageBox::tr("DXC Server address must be in format<p><b>[username@]hostname:port</b> (ex. hamqth.com:7300)</p>"));
            return;
        }
        connectCluster();
    }
}

void DxWidget::connectCluster()
{
    FCT_IDENTIFICATION;

    const Callsign connectCallsign(StationProfilesManager::instance()->getCurProfile1().callsign);
    connectedServerString = new DxServerString(ui->serverSelect->currentText(),
                                               connectCallsign.isValid() ? connectCallsign.getBase().toLower()
                                                                         : QString());

    if ( !connectedServerString )
    {
        qWarning() << "Cannot allocate currServerString";
        return;
    }

    if ( !connectedServerString->isValid() )
    {
        qWarning() << "DX Server address is not valid";
        return;
    }

    qCDebug(runtime) << "username:" << connectedServerString->getUsername()
                     << "host:" << connectedServerString->getHostname()
                     << "port:" << connectedServerString->getPort();

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &DxWidget::receive, Qt::QueuedConnection); // QueuedConnection is needed because error is send together with disconnect
                                                                                             // which causes creash because error destroid object during processing received signal
    connect(socket, &QTcpSocket::connected, this, &DxWidget::connected, Qt::QueuedConnection);
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &DxWidget::socketError, Qt::QueuedConnection);
#else
    connect(socket, &QTcpSocket::errorOccurred, this, &DxWidget::socketError, Qt::QueuedConnection);
#endif
    ui->connectButton->setEnabled(false);
    ui->connectButton->setText(tr("Connecting..."));

    if ( reconnectAttempts == 0 )
    {
        ui->log->clear();
        if ( ! getKeepQSOs() )
        {
            ui->dxTable->clearSelection();
            dxTableModel->clear();
            wcyTableModel->clear();
            wwvTableModel->clear();
            toAllTableModel->clear();
        }
        ui->dxTable->repaint();
    }

    socket->connectToHost(connectedServerString->getHostname(),
                          connectedServerString->getPort());
    connectionState = CONNECTING;
}

void DxWidget::disconnectCluster(bool tryReconnect)
{
    FCT_IDENTIFICATION;

    reconnectTimer.stop();
    ui->commandEdit->clear();
    ui->commandEdit->setEnabled(false);
    ui->commandButton->setEnabled(false);
    ui->connectButton->setEnabled(true);

    if ( socket )
    {
       socket->disconnect();
       socket->close();

       socket->deleteLater();
       socket = nullptr;
    }

    if ( reconnectAttempts < NUM_OF_RECONNECT_ATTEMPTS
         && tryReconnect )
    {
        reconnectAttempts++;
        reconnectTimer.start();
        ui->commandEdit->setPlaceholderText(tr("DX Cluster is temporarily unavailable"));
    }
    else
    {
        reconnectAttempts = 0;
        ui->commandEdit->setPlaceholderText("");
        ui->connectButton->setText(tr("Connect"));
        ui->serverSelect->setStyleSheet(QStringLiteral("QComboBox {color: red}"));
    }
    connectionState = DISCONNECTED;
    if ( connectedServerString )
    {
        delete connectedServerString;
        connectedServerString = nullptr;
    }

    clearAllPasswordIcons();
}

void DxWidget::saveDXCServers()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QStringList serversItems = getDXCServerList();
    const QString &curr_server = ui->serverSelect->currentText();

    if ( DxServerString::isValidServerString(ui->serverSelect->currentText())
         && !serversItems.contains(curr_server)
         && !curr_server.isEmpty() )
    {
        ui->serverSelect->insertItem(0, ui->serverSelect->currentText());
        serversItems.prepend(ui->serverSelect->currentText()); // insert policy is InsertAtTop
        ui->serverSelect->setCurrentIndex(0);
    }

    settings.setValue("dxc/servers", serversItems);
    settings.setValue("dxc/last_server", ui->serverSelect->currentText());
}

QString DxWidget::modeFilterRegExp()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QString regexp("NOTHING");

    if (settings.value("dxc/filter_mode_phone",true).toBool())   regexp = regexp + "|" + BandPlan::MODE_GROUP_STRING_PHONE;
    if (settings.value("dxc/filter_mode_cw",true).toBool())      regexp = regexp + "|" + BandPlan::MODE_GROUP_STRING_CW;
    if (settings.value("dxc/filter_mode_ft8",true).toBool())     regexp = regexp + "|" + BandPlan::MODE_GROUP_STRING_FT8;
    if (settings.value("dxc/filter_mode_digital",true).toBool()) regexp = regexp + "|" + BandPlan::MODE_GROUP_STRING_DIGITAL;

    return regexp;
}

QString DxWidget::contFilterRegExp()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_cont_regexp","NOTHING|AF|AN|AS|EU|NA|OC|SA").toString();
}

QString DxWidget::spotterContFilterRegExp()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_spotter_cont_regexp","NOTHING|AF|AN|AS|EU|NA|OC|SA").toString();
}

QString DxWidget::bandFilterRegExp()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QString regexp("NOTHING");


    SqlListModel *bands= new SqlListModel("SELECT name FROM bands WHERE enabled = 1 ORDER BY start_freq", "Band");

    int band_index = 1; // the first record (0) is Header - skip it and start at position 1

    while (band_index < bands->rowCount())
    {
        QString band_name = bands->data(bands->index(band_index,0)).toString();
        if ( settings.value("dxc/filter_band_" + band_name,true).toBool() )
        {
            regexp.append("|" + band_name);
        }
        band_index++;
    }
    return regexp;
}

uint DxWidget::dxccStatusFilterValue()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_dxcc_status", DxccStatus::All).toUInt();
}

int DxWidget::getDedupTimeValue()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_duplicationtime", DEDUPLICATION_TIME).toInt();
}

int DxWidget::getDedupFreqValue()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_duplicationfreq", DEDUPLICATION_FREQ_TOLERANCE).toInt();
}

bool DxWidget::spotDedupValue()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_deduplication", false).toBool();
}

QStringList DxWidget::dxMemberList()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/filter_dx_member_list").toStringList();
}

bool DxWidget::getAutoconnectServer()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/autoconnect", false).toBool();
}

void DxWidget::saveAutoconnectServer(bool state)
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("dxc/autoconnect", state);
}

bool DxWidget::getKeepQSOs()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    return settings.value("dxc/keepqsos", false).toBool();
}

void DxWidget::saveKeepQSOs(bool state)
{
    FCT_IDENTIFICATION;

    QSettings settings;
    settings.setValue("dxc/keepqsos", state);
}

void DxWidget::sendCommand(const QString & command,
                           bool switchToConsole)
{
    FCT_IDENTIFICATION;

    QByteArray data;
    data.append(command.toLatin1());
    data.append("\r\n");

    if ( socket && socket->isOpen() )
    {
        socket->write(data);
    }

    // switch to raw mode to see a response
    if ( switchToConsole )
    {
        ui->viewModeCombo->setCurrentIndex(CONSOLE_VIEW);
    }
}

void DxWidget::saveWidgetSetting()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    settings.setValue("dxc/dxtablestate", ui->dxTable->horizontalHeader()->saveState());
    settings.setValue("dxc/wcytablestate", ui->wcyTable->horizontalHeader()->saveState());
    settings.setValue("dxc/wwvtablestate", ui->wwvTable->horizontalHeader()->saveState());
    settings.setValue("dxc/toalltablestate", ui->toAllTable->horizontalHeader()->saveState());
    settings.setValue("dxc/consolefontsize", ui->log->font().pointSize());
}

void DxWidget::restoreWidgetSetting()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QVariant state = settings.value("dxc/dxtablestate");

    if (!state.isNull())
    {
        ui->dxTable->horizontalHeader()->restoreState(state.toByteArray());
    }

    state = settings.value("dxc/wcytablestate");

    if (!state.isNull())
    {
        ui->wcyTable->horizontalHeader()->restoreState(state.toByteArray());
    }

    state = settings.value("dxc/wwvtablestate");

    if (!state.isNull())
    {
        ui->wwvTable->horizontalHeader()->restoreState(state.toByteArray());
    }
    state = settings.value("dxc/toalltablestate");

    if (!state.isNull())
    {
        ui->toAllTable->horizontalHeader()->restoreState(state.toByteArray());
    }

    int fontsize = settings.value("dxc/consolefontsize", -1).toInt();

    QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    if ( fontsize > 0 )
    {
        monospace.setPointSize(fontsize);
    }
    ui->log->setFont(monospace);
}

void DxWidget::send()
{
    FCT_IDENTIFICATION;

    sendCommand(ui->commandEdit->text());
    ui->commandEdit->clear();
}

void DxWidget::receive()
{
    FCT_IDENTIFICATION;

    static QRegularExpression dxSpotRE(QStringLiteral("^DX de ([a-zA-Z0-9\\/]+).*:\\s+([0-9|.]+)\\s+([a-zA-Z0-9\\/]+)[^\\s]*\\s+(.*)\\s+(\\d{4}Z)"),
                                       QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch dxSpotMatch;

    static QRegularExpression wcySpotRE(QStringLiteral("^(WCY de) +([A-Z0-9\\-#]*) +<(\\d{2})> *: +K=(\\d{1,3}) expK=(\\d{1,3}) A=(\\d{1,3}) R=(\\d{1,3}) SFI=(\\d{1,3}) SA=([a-zA-Z]{1,3}) GMF=([a-zA-Z]{1,3}) Au=([a-zA-Z]{2}) *$"),
                                        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch wcySpotMatch;

    static QRegularExpression wwvSpotRE(QStringLiteral("^(WWV de) +([A-Z0-9\\-#]*) +<(\\d{2})Z?> *: *SFI=(\\d{1,3}), A=(\\d{1,3}), K=(\\d{1,3}), (.*\\b) *-> *(.*\\b) *$"),
                                        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch wwvSpotMatch;

    static QRegularExpression toAllSpotRE(QStringLiteral("^(To ALL de) +([A-Z0-9\\-#]*)\\s?(<(\\d{4})Z>)?[ :]+(.*)?$"),
                                        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch toAllSpotMatch;

    static QRegularExpression SHDXFormatRE(QStringLiteral("^\\s{0,8}([0-9|.]+)\\s+([a-zA-Z0-9\\/]+)[^\\s]*\\s+(.*)\\s+(\\d{4}Z) (.*)<([a-zA-Z0-9\\/]+)>$"),
                                        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch SHDXFormatMatch;

    static QRegularExpression splitLineRE(QStringLiteral("(\a|\n|\r)+"));
    static QRegularExpression loginRE(QStringLiteral("enter your call(sign)?:"));

    reconnectAttempts = 0;
    const QStringList &lines = QString::fromUtf8(socket->readAll()).split(splitLineRE);

    for ( const QString &line : lines )
    {
        if ( !socket || !connectedServerString )
        {
            qCDebug(runtime) << "socket or connection string is null";
            return;
        }

        qCDebug(runtime) << connectionState << line;

        // Skip empty lines
        if ( line.length() == 0 )
        {
            continue;
        }

        if ( line.startsWith(QStringLiteral("login"), Qt::CaseInsensitive)
             || line.contains(loginRE) )
        {
            // username requested
            socket->write(connectedServerString->getUsername().append("\r\n").toLocal8Bit());
            connectionState = LOGIN_SENT;
            qCDebug(runtime) << "Login sent";
            continue;
        }

        if ( connectionState == LOGIN_SENT
             && line.contains(QStringLiteral("is an invalid callsign")) )
        {
            // invalid login
            QMessageBox::warning(nullptr,
                                 tr("DXC Server Error"),
                                 tr("An invalid callsign"));
            continue;
        }

        if ( connectionState == LOGIN_SENT
             && line.startsWith(QStringLiteral("password"), Qt::CaseInsensitive) )
        {
            // password requested
            QString password = CredentialStore::instance()->getPassword(connectedServerString->getPasswordStorageKey(),
                                                                        connectedServerString->getUsername());

            if ( password.isEmpty() )
            {
                InputPasswordDialog passwordDialog(tr("DX Cluster Password"),
                                                   "<b>" + tr("Security Notice") + ":</b> " + tr("The password can be sent via an unsecured channel") +
                                                   "<br/><br/>" +
                                                   "<b>" + tr("Server") + ":</b> " + socket->peerName() + ":" + QString::number(socket->peerPort()) +
                                                   "<br/>" +
                                                   "<b>" + tr("Username") + "</b>: " + connectedServerString->getUsername(), this);
                if ( passwordDialog.exec() == QDialog::Accepted )
                {
                    password = passwordDialog.getPassword();
                    if ( passwordDialog.getRememberPassword() && !password.isEmpty() )
                    {
                        CredentialStore::instance()->savePassword(connectedServerString->getPasswordStorageKey(),
                                                                  connectedServerString->getUsername(),
                                                                  password);
                    }
                }
                else
                {
                    disconnectCluster(false);
                    return;
                }
            }
            activateCurrPasswordIcon();
            socket->write(password.append("\r\n").toLocal8Bit());
            connectionState = PASSWORD_SENT;
            qCDebug(runtime) << "Password sent";
            continue;
        }

        if ( connectionState == PASSWORD_SENT
             && line.startsWith(QStringLiteral("sorry"), Qt::CaseInsensitive ) )
        {
            // invalid password
            CredentialStore::instance()->deletePassword(connectedServerString->getPasswordStorageKey(),
                                                        connectedServerString->getUsername());
            QMessageBox::warning(nullptr,
                                 QMessageBox::tr("DX Cluster Password"),
                                 QMessageBox::tr("Invalid Password"));
            continue;
        }

        if ( line.contains(QStringLiteral("dxspider"), Qt::CaseInsensitive) )
        {
            if ( connectionState == LOGIN_SENT
                 || connectionState == PASSWORD_SENT )
                connectionState = OPERATION;

            ui->commandButton->setEnabled(true);
        }

        /********************/
        /* Received DX SPOT */
        /********************/
        if ( line.startsWith(QStringLiteral("DX")) )
        {
            if ( connectionState == LOGIN_SENT
                 || connectionState == PASSWORD_SENT )
                connectionState = OPERATION;

            dxSpotMatch = dxSpotRE.match(line);

            if ( dxSpotMatch.hasMatch() )
            {
                //DX de N9EN/4:    18077.0  OS5Z         op. Marc; tnx QSO!             1359Z
                processDxSpot(dxSpotMatch.captured(1),   //spotter
                              dxSpotMatch.captured(2),   //freq
                              dxSpotMatch.captured(3),   //call
                              dxSpotMatch.captured(4));  //comment
            }
        }
        /************************/
        /* Received WCY Info */
        /************************/
        else if ( line.startsWith(QStringLiteral("WCY de")) )
        {
            wcySpotMatch = wcySpotRE.match(line);

            if ( wcySpotMatch.hasMatch() )
            {
                WCYSpot spot;

                spot.time = QDateTime::currentDateTime().toTimeSpec(Qt::UTC);
                spot.KIndex = wcySpotMatch.captured(4).toUInt();
                spot.expK = wcySpotMatch.captured(5).toUInt();
                spot.AIndex = wcySpotMatch.captured(6).toUInt();
                spot.RIndex = wcySpotMatch.captured(7).toUInt();
                spot.SFI = wcySpotMatch.captured(8).toUInt();
                spot.SA = wcySpotMatch.captured(9);
                spot.GMF = wcySpotMatch.captured(10);
                spot.Au = wcySpotMatch.captured(11);

                emit newWCYSpot(spot);
                wcyTableModel->addEntry(spot);
            }
        }
        /*********************/
        /* Received WWV Info */
        /*********************/
        else if ( line.startsWith(QStringLiteral("WWV de")) )
        {
            wwvSpotMatch = wwvSpotRE.match(line);

            if ( wwvSpotMatch.hasMatch() )
            {
                WWVSpot spot;

                spot.time = QDateTime::currentDateTime().toTimeSpec(Qt::UTC);
                spot.SFI = wwvSpotMatch.captured(4).toUInt();
                spot.AIndex = wwvSpotMatch.captured(5).toUInt();
                spot.KIndex = wwvSpotMatch.captured(6).toUInt();
                spot.info1 = wwvSpotMatch.captured(7);
                spot.info2 = wwvSpotMatch.captured(8);

                emit newWWVSpot(spot);
                wwvTableModel->addEntry(spot);
            }
        }
        /*************************/
        /* Received Generic Info */
        /*************************/
        else if ( line.startsWith(QStringLiteral("To ALL de")) )
        {
            toAllSpotMatch = toAllSpotRE.match(line);

            if ( toAllSpotMatch.hasMatch() )
            {
                ToAllSpot spot;

                spot.time = QDateTime::currentDateTime().toTimeSpec(Qt::UTC);
                spot.spotter = toAllSpotMatch.captured(2);
                DxccEntity spotter_info = Data::instance()->lookupDxcc(spot.spotter);
                spot.dxcc_spotter = spotter_info;
                spot.message = toAllSpotMatch.captured(5);

                emit newToAllSpot(spot);
                toAllTableModel->addEntry(spot);
            }
        }
        /****************/
        /* SH/DX format  */
        /****************/
        else if ( line.contains(SHDXFormatRE) )
        {
            SHDXFormatMatch = SHDXFormatRE.match(line);

            if ( SHDXFormatMatch.hasMatch() )
            {
                //14045.6 K5UV         6-Dec-2023 1359Z CWops CWT Contest             <VE4DL>
                const QDateTime &dateTime = QDateTime::fromString(SHDXFormatMatch.captured(3) +
                                                                  " " +
                                                                  SHDXFormatMatch.captured(4), "d-MMM-yyyy hhmmZ");
                processDxSpot(SHDXFormatMatch.captured(6),   //spotter
                              SHDXFormatMatch.captured(1),   //freq
                              SHDXFormatMatch.captured(2),   //call
                              SHDXFormatMatch.captured(5),
                              dateTime);  //comment
            }
        }
        ui->log->appendPlainText(line);
    }
}

void DxWidget::socketError(QAbstractSocket::SocketError socker_error)
{
    FCT_IDENTIFICATION;

    bool reconectRequested = (reconnectAttempts > 0 ) ? true : false;

    QString error_msg = QObject::tr("Cannot connect to DXC Server <p>Reason <b>: ");

    qCDebug(runtime) << socker_error;

    switch (socker_error)
    {
    case QAbstractSocket::ConnectionRefusedError:
        error_msg.append(QObject::tr("Connection Refused"));
        break;
    case QAbstractSocket::RemoteHostClosedError:
        error_msg.append(QObject::tr("Host closed the connection"));
        reconectRequested = (connectionState != LOGIN_SENT
                             && connectionState != PASSWORD_SENT);
        break;
    case QAbstractSocket::HostNotFoundError:
        error_msg.append(QObject::tr("Host not found"));
        break;
    case QAbstractSocket::SocketTimeoutError:
        error_msg.append(QObject::tr("Timeout"));
        reconectRequested = true;
        break;
    case QAbstractSocket::NetworkError:
        error_msg.append(QObject::tr("Network Error"));
        break;
    default:
        error_msg.append(QObject::tr("Internal Error"));

    }
    error_msg.append("</b></p>");

    qInfo() << "Detailed Error: " << socker_error;

    if ( connectionState != LOGIN_SENT
         && connectionState != PASSWORD_SENT
         && (! reconectRequested || reconnectAttempts == NUM_OF_RECONNECT_ATTEMPTS))
    {
        QMessageBox::warning(nullptr,
                             QMessageBox::tr("DXC Server Connection Error"),
                             error_msg);
    }
    disconnectCluster(reconectRequested);
}

void DxWidget::connected()
{
    FCT_IDENTIFICATION;

    if ( !socket )
    {
        qWarning() << "Socket is not opened";
        return;
    }

    int fd = socket->socketDescriptor();

#ifdef Q_OS_WIN
    DWORD  dwBytesRet = 0;

    struct tcp_keepalive   alive;    // your options for "keepalive" mode
    alive.onoff = TRUE;              // turn it on
    alive.keepalivetime = 10000;     // delay (ms) between requests, here is 10s, default is 2h (7200000)
    alive.keepaliveinterval = 5000;  // delay between "emergency" ping requests, their number (6) is not configurable
      /* So with this config  socket will send keepalive requests every 30 seconds after last data transaction when everything is ok.
          If there is no reply (wire plugged out) it'll send 6 requests with 5s delay  between them and then close.
          As a result we will get disconnect after approximately 1 min timeout.
       */
    if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR) {
           qWarning() << "WSAIotcl(SIO_KEEPALIVE_VALS) failed with err#" <<  WSAGetLastError();
    }
#else
    int enableKeepAlive = 1;
    int maxIdle = 10;
    int count = 3;
    int interval = 10;

    if ( setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive)) !=0 )
    {
         qWarning() << "Cannot set keepalive for DXC";
    }
    else
    {
#ifndef Q_OS_MACOS
        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &maxIdle, sizeof(maxIdle)) != 0 )
#else
        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &maxIdle, sizeof(maxIdle)) != 0 )
#endif /* Q_OS_MACOS */
        {
            qWarning() << "Cannot set keepalive idle for DXC";
        }

        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0 )
        {
            qWarning() << "Cannot set keepalive counter for DXC";
        }

        if ( setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0 )
        {
            qWarning() << "Cannot set keepalive interval for DXC";
        }

        // TODO: setup TCP_USER_TIMEOUT????
    }
#endif
    ui->commandEdit->setEnabled(true);
    ui->connectButton->setEnabled(true);
    ui->connectButton->setText(tr("Disconnect"));
    ui->commandEdit->setPlaceholderText("");
    ui->serverSelect->setStyleSheet("QComboBox {color: green}");
    connectionState = CONNECTED;
    saveDXCServers();
}

void DxWidget::viewModeChanged(int index)
{
    FCT_IDENTIFICATION;

    ui->stack->setCurrentIndex(index);
}

void DxWidget::entryDoubleClicked(QModelIndex index)
{
    FCT_IDENTIFICATION;

    emit tuneDx(dxTableModel->getCallsign(index),
                dxTableModel->getFrequency(index),
                dxTableModel->getBandPlanode(index));
}

void DxWidget::actionFilter()
{
    FCT_IDENTIFICATION;

    DxFilterDialog dialog;

    if (dialog.exec() == QDialog::Accepted)
        reloadSetting();
}

void DxWidget::adjusteServerSelectSize(QString input)
{
    FCT_IDENTIFICATION;

    qDebug(function_parameters)<< input << input.length();

    QFont f;
    QFontMetrics met(f);

    ui->serverSelect->setMinimumWidth(met.boundingRect(input).width() + 35);
    ui->serverSelect->update();
    ui->serverSelect->repaint();
}

void DxWidget::serverSelectChanged(int index)
{
    FCT_IDENTIFICATION;

    qDebug(function_parameters) << index;

    if ( (socket && socket->isOpen())
         || reconnectAttempts )
    {
        /* reconnect DXC Server */
        if ( index >= 0 )
        {
            disconnectCluster();
            connectCluster();
        }
    }
}

void DxWidget::setLastQSO(QSqlRecord qsoRecord)
{
    FCT_IDENTIFICATION;

    lastQSO = qsoRecord;
}

void DxWidget::reloadSetting()
{
    FCT_IDENTIFICATION;

    moderegexp.setPattern(modeFilterRegExp());
    contregexp.setPattern(contFilterRegExp());
    spottercontregexp.setPattern(spotterContFilterRegExp());
    bandregexp.setPattern(bandFilterRegExp());
    dxccStatusFilter = dxccStatusFilterValue();
    deduplicateSpots = spotDedupValue();
    deduplicatetime = getDedupTimeValue();
    deduplicatefreq = getDedupFreqValue();
    QStringList tmp = dxMemberList();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    dxMemberFilter = QSet<QString>(tmp.begin(), tmp.end());
#else /* Due to ubuntu 20.04 where qt5.12 is present */
    dxMemberFilter = QSet<QString>(QSet<QString>::fromList(tmp));
#endif
}

void DxWidget::prepareQSOSpot(QSqlRecord qso)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "QSO" << qso;

    if ( !ui->commandEdit->isEnabled() )
        return;

    if ( qso.contains(QStringLiteral("start_time")) )
    {
        //qso is valid record
        if ( qso.contains(QStringLiteral("freq"))
             && qso.contains(QStringLiteral("callsign")) )
        {
            double spotFreq = ( qso.contains("freq_rx")
                                && qso.value("freq_rx").toDouble() != 0.0 ) ? qso.value("freq_rx").toDouble()
                                                                            : qso.value("freq").toDouble();

            // DX Spider allow to enter QSO freq in MHz but it is not reliable for SHF bands.
            // a more reliable way is to send a spot with kHz value
            ui->commandEdit->setText(QString("dx %1 %2 ").arg(QString::number(Hz2kHz(MHz(spotFreq)), 'f', 0),
                                                              qso.value("callsign").toString()));
            ui->commandEdit->setFocus();
        }
    }
}

void DxWidget::actionCommandSpotQSO()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Last QSO" << lastQSO;

    prepareQSOSpot(lastQSO);
    ui->commandButton->setDefaultAction(ui->actionSpotQSO);
}

void DxWidget::actionCommandShowHFStats()
{
    FCT_IDENTIFICATION;

    sendCommand(QStringLiteral("sh/hfstats"), true);
    ui->commandButton->setDefaultAction(ui->actionShowHFStats);
}

void DxWidget::actionCommandShowVHFStats()
{
    FCT_IDENTIFICATION;

    sendCommand(QStringLiteral("sh/vhfstats"), true);
    ui->commandButton->setDefaultAction(ui->actionShowVHFStats);
}

void DxWidget::actionCommandShowWCY()
{
    FCT_IDENTIFICATION;

    sendCommand(QStringLiteral("sh/wcy"), true);
    ui->commandButton->setDefaultAction(ui->actionShowWCY);
}

void DxWidget::actionCommandShowWWV()
{
    FCT_IDENTIFICATION;

    sendCommand(QStringLiteral("sh/wwv"), true);
    ui->commandButton->setDefaultAction(ui->actionShowWWV);
}

void DxWidget::actionConnectOnStartup()
{
    FCT_IDENTIFICATION;

    saveAutoconnectServer(ui->actionConnectOnStartup->isChecked());

    if ( ui->actionConnectOnStartup->isChecked() && socket == nullptr)
    {
        // dxc is not connected, connnet it
        toggleConnect();
    }
}

void DxWidget::actionDeleteServer()
{
    FCT_IDENTIFICATION;

    actionForgetPassword();
    ui->serverSelect->removeItem(ui->serverSelect->currentIndex());
    ui->serverSelect->setMinimumWidth(0);
    saveDXCServers();
}

void DxWidget::actionForgetPassword()
{
    FCT_IDENTIFICATION;

    DxServerString serverName(ui->serverSelect->currentText(),
                              StationProfilesManager::instance()->getCurProfile1().callsign.toLower());

    if ( serverName.isValid() )
    {
        CredentialStore::instance()->deletePassword(serverName.getPasswordStorageKey(),
                                                    serverName.getUsername());
    }
    else
    {
        qCDebug(runtime) << "Cannot remove record from Secure Store, server name is not valid"
                         << ui->serverSelect->currentText();
    }
    ui->serverSelect->setItemIcon(ui->serverSelect->currentIndex(), QIcon());
}

void DxWidget::actionKeepSpots()
{
    FCT_IDENTIFICATION;

    saveKeepQSOs(ui->actionKeepSpots->isChecked());
}

void DxWidget::actionClear()
{
    FCT_IDENTIFICATION;

    QTableView *view = nullptr;

    switch ( ui->stack->currentIndex() )
    {
    case 0: dxTableModel->clear(); view = ui->dxTable; break;
    case 1: wcyTableModel->clear(); view = ui->wcyTable; break;
    case 2: wwvTableModel->clear(); view = ui->wwvTable; break;
    case 3: toAllTableModel->clear(); view = ui->toAllTable; break;
    default: view = nullptr;
    }

    if ( view )
    {
        view->repaint();
    }
}

void DxWidget::displayedColumns()
{
    FCT_IDENTIFICATION;

    QTableView *view = nullptr;

    switch ( ui->stack->currentIndex() )
    {
    case 0: view = ui->dxTable; break;
    case 1: view = ui->wcyTable; break;
    case 2: view = ui->wwvTable; break;
    case 3: view = ui->toAllTable; break;
    default: view = nullptr;
    }

    if ( view )
    {
        ColumnSettingSimpleDialog dialog(view);
        dialog.exec();
        saveWidgetSetting();
    }
}

QStringList DxWidget::getDXCServerList()
{
    FCT_IDENTIFICATION;

    QStringList ret;

    for ( int index = 0; index < ui->serverSelect->count(); index++ )
    {
        ret << ui->serverSelect->itemText(index);
    }
    return ret;
}

void DxWidget::serverComboSetup()
{
    FCT_IDENTIFICATION;

    QSettings settings;

    QStringList DXCservers = settings.value("dxc/servers", QStringList("hamqth.com:7300")).toStringList();
    DeleteHighlightedDXServerWhenDelPressedEventFilter *deleteHandled = new DeleteHighlightedDXServerWhenDelPressedEventFilter;

    ui->serverSelect->addItems(DXCservers);
    ui->serverSelect->installEventFilter(deleteHandled);
    connect(deleteHandled, &DeleteHighlightedDXServerWhenDelPressedEventFilter::deleteServerItem,
            this, &DxWidget::actionDeleteServer);

    QString lastUsedServer = settings.value("dxc/last_server").toString();
    int index = ui->serverSelect->findText(lastUsedServer);

    // if last server still exists then set it otherwise use the first one
    if ( index >= 0 )
    {
        ui->serverSelect->setCurrentIndex(index);
    }
}

void DxWidget::clearAllPasswordIcons()
{
    FCT_IDENTIFICATION;

    for (int i = 0; i < ui->serverSelect->count(); i++)
    {
        ui->serverSelect->setItemIcon(i, QIcon());
    }
}

void DxWidget::activateCurrPasswordIcon()
{
    FCT_IDENTIFICATION;

    ui->serverSelect->setItemIcon(ui->serverSelect->currentIndex(), QIcon(":/icons/password.png"));
}

void DxWidget::processDxSpot(const QString &spotter,
                             const QString &freq,
                             const QString &call,
                             const QString &comment,
                             const QDateTime &dateTime)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << spotter << freq << call << comment << dateTime << dateTime.isNull();

    DxSpot spot;

    spot.time = (dateTime.isNull()) ? QDateTime::currentDateTime().toTimeSpec(Qt::UTC)
                                    : dateTime;
    spot.callsign = call;
    spot.freq = freq.toDouble() / 1000;
    spot.band = BandPlan::freq2Band(spot.freq).name;
    spot.spotter = spotter;
    spot.comment = comment.trimmed();
    spot.bandPlanMode = modeGroupFromComment(spot.comment);
    if ( spot.bandPlanMode == BandPlan::BAND_MODE_UNKNOWN )
    {
        spot.bandPlanMode = BandPlan::freq2BandMode(spot.freq);
    }
    if ( spot.bandPlanMode == BandPlan::BAND_MODE_PHONE )
    {
        spot.bandPlanMode = (spot.freq < 10.0 ) ? BandPlan::BAND_MODE_LSB
                                                : BandPlan::BAND_MODE_USB;
    }
    spot.modeGroupString = BandPlan::bandMode2BandModeGroupString(spot.bandPlanMode);
    spot.dxcc = Data::instance()->lookupDxcc(call);
    spot.dxcc_spotter = Data::instance()->lookupDxcc(spotter);
    spot.status = Data::dxccStatus(spot.dxcc.dxcc, spot.band, spot.modeGroupString);
    spot.callsign_member = MembershipQE::instance()->query(spot.callsign);

    emit newSpot(spot);

    if ( spot.modeGroupString.contains(moderegexp)
         && spot.dxcc.cont.contains(contregexp)
         && spot.dxcc_spotter.cont.contains(spottercontregexp)
         && spot.band.contains(bandregexp)
         && ( spot.status & dxccStatusFilter)
         && ( dxMemberFilter.size() == 0
              || (dxMemberFilter.size() && spot.memberList2Set().intersects(dxMemberFilter)))
        )
    {
        if ( dxTableModel->addEntry(spot, deduplicateSpots, deduplicatetime, deduplicatefreq) )
            emit newFilteredSpot(spot);
    }
}

BandPlan::BandPlanMode DxWidget::modeGroupFromComment(const QString &comment) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << comment;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    const QStringList &tokenizedComment = comment.split(" ", Qt::SkipEmptyParts);
#else /* Due to ubuntu 20.04 where qt5.12 is present */
    const QStringList &tokenizedComment = comment.split(" ", QString::SkipEmptyParts);
#endif

    if ( tokenizedComment.contains("CW", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_CW;

    if ( tokenizedComment.contains("FT8", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_FT8;

    if ( tokenizedComment.contains("FT4", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_DIGITAL;

    if ( tokenizedComment.contains("MSK144", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_DIGITAL;

    if ( tokenizedComment.contains("RTTY", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_DIGITAL;

    if ( tokenizedComment.contains("SSTV", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_DIGITAL;

    if ( tokenizedComment.contains("PACKET", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_DIGITAL;

    if ( tokenizedComment.contains("SSB", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_PHONE;

    if ( tokenizedComment.contains("USB", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_USB;

    if ( tokenizedComment.contains("LSB", Qt::CaseInsensitive) )
        return BandPlan::BAND_MODE_LSB;

    return BandPlan::BAND_MODE_UNKNOWN;
}

DxWidget::~DxWidget()
{
    FCT_IDENTIFICATION;

    saveWidgetSetting();
    delete ui;
}


