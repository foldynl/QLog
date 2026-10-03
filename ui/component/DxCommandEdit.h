#ifndef QLOG_UI_COMPONENT_DXCOMMANDEDIT_H
#define QLOG_UI_COMPONENT_DXCOMMANDEDIT_H

#include <QLineEdit>
#include <QList>

class QCompleter;
class QStandardItemModel;

class DxCommandEdit : public QLineEdit
{
    Q_OBJECT

public:
    enum ClusterType
    {
        UNKNOWN = 0,
        DXSPIDER = 1,
        CC_CLUSTER = 2
    };

    explicit DxCommandEdit(QWidget *parent = nullptr);
    void setServer(const QString &server);
    void setClusterType(ClusterType type);
    void rememberCommand(const QString &command);

public slots:
    void showCommands();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    static const int MAX_HISTORY = 10;
    static const int MAX_MY_COMMANDS = 5;

    struct Command
    {
        QString name;
        QString description;
        QString parameters;
        QString example;
        bool parameterSuffix;
    };

    struct HistoryEntry
    {
        QString command;
        int count;
    };

    static QString commandName(const QString &text);
    void updatePopup(bool showAll = false);
    void insertCommand(const QModelIndex &index);
    void addHeading(const QString &text);

    QCompleter *commandCompleter;
    QStandardItemModel *commandModel;
    QList<Command> commands;
    QList<HistoryEntry> history;
    QString server;
    bool historyEnabled = true;
};

#endif
