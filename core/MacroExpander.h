#ifndef QLOG_CORE_MACROEXPANDER_H
#define QLOG_CORE_MACROEXPANDER_H

#include <QCoreApplication>
#include <QMap>
#include <QStringList>
#include <QSqlRecord>

class MacroExpander
{
    Q_DECLARE_TR_FUNCTIONS(MacroExpander)

public:
    using Context = QMap<QString, QString>;

    struct Result
    {
        QString text;
        QStringList missingMacros;
    };

    static Result expand(const QString &text, const Context &context);
    static Result expandSpotComment(const QString &text, const Context &context);
    static Context fromQSO(const QSqlRecord &qso);
    static QString helpText();

private:
    enum Format
    {
        UPPER_CASE, UNCHANGED, REPORT_N, SERIAL_NUMBER, SERIAL_NUMBER_N, PROPAGATION
    };

    struct Definition
    {
        const char *name;
        const char *field;
        Format format;
        const char *description;
    };

    static const QList<Definition> &definitions();
};

#endif // QLOG_CORE_MACROEXPANDER_H
