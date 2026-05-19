#ifndef QLOG_UI_COMPONENT_SQLHIGHLIGHTER_H
#define QLOG_UI_COMPONENT_SQLHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class SqlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit SqlHighlighter(QTextDocument *parent = nullptr);
    void setUserIdentifiers(const QStringList &identifiers);

    static const QStringList &sqlKeywords();
    static const QStringList &sqlFunctions();

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<Rule> rules;
    QVector<Rule> identifierRules;

    QTextCharFormat multiLineCommentFormat;
    QRegularExpression blockCommentStart;
    QRegularExpression blockCommentEnd;

    bool isDark;
};

#endif // QLOG_UI_COMPONENT_SQLHIGHLIGHTER_H
