#ifndef QLOG_UI_COMPONENT_DXCOMMANDEDIT_H
#define QLOG_UI_COMPONENT_DXCOMMANDEDIT_H

#include "SuggestionEdit.h"

class DxCommandEdit : public SuggestionEdit
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

protected:
    void populateSuggestions(const QString &query) override;

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
    QList<Command> commands;
    QList<HistoryEntry> history;
    QString server;
    bool historyEnabled = true;
};

#endif
