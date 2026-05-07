#include "SqlHighlighter.h"
#include "core/debug.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>

MODULE_IDENTIFICATION("qlog.ui.component.sqlhighlighter");

// Detect dark mode by checking whether the window background is darker than mid-gray.
static bool detectDarkMode()
{
    return QApplication::palette().window().color().lightness() < 128;
}

const QStringList &SqlHighlighter::sqlKeywords()
{
    static const QStringList keywords = {
        "SELECT", "FROM", "WHERE", "AND", "OR", "NOT", "IN", "LIKE", "BETWEEN",
        "IS", "NULL", "ORDER", "BY", "GROUP", "HAVING", "LIMIT", "OFFSET",
        "DISTINCT", "ALL", "AS", "JOIN", "LEFT", "RIGHT", "INNER", "OUTER",
        "FULL", "CROSS", "ON", "UNION", "INTERSECT", "EXCEPT", "WITH",
        "RECURSIVE", "CASE", "WHEN", "THEN", "ELSE", "END", "EXISTS",
        "ASC", "DESC", "COLLATE", "CAST", "TRUE", "FALSE", "ROWID",
        "NOCASE", "BINARY"
    };
    return keywords;
}

const QStringList &SqlHighlighter::sqlFunctions()
{
    static const QStringList functions = {
        "COUNT", "SUM", "AVG", "MIN", "MAX", "ABS", "LENGTH", "LOWER", "UPPER",
        "SUBSTR", "TRIM", "LTRIM", "RTRIM", "REPLACE", "INSTR", "PRINTF",
        "ROUND", "COALESCE", "NULLIF", "IFNULL", "IIF", "TYPEOF",
        "DATE", "TIME", "DATETIME", "JULIANDAY", "STRFTIME",
        "RANDOM", "HEX", "QUOTE", "GROUP_CONCAT", "JSON_EXTRACT"
    };
    return functions;
}

SqlHighlighter::SqlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent),
      isDark(detectDarkMode())
{
    // -----------------------------------------------------------------------
    // Color palette - two sets so both themes are readable.
    //
    // Dark  colours lifted from VS Code "Dark+" defaults.
    // Light colours lifted from VS Code "Light+" defaults.
    // -----------------------------------------------------------------------
    const QColor keywordColor  = isDark ? QColor(86,  156, 214) : QColor( 0,   0, 187);
    const QColor functionColor = isDark ? QColor(220, 220, 170) : QColor(121,  94,  38);
    const QColor stringColor   = isDark ? QColor(206, 145, 120) : QColor(163,  21,  21);
    const QColor numberColor   = isDark ? QColor(181, 206, 168) : QColor(  9, 134,  88);
    const QColor commentColor  = isDark ? QColor(106, 153,  85) : QColor( 10, 121,  10);

    // SQL keywords - bold
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(keywordColor);
    keywordFormat.setFontWeight(QFont::Bold);

    for ( const QString &kw : sqlKeywords() )
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(kw),
            QRegularExpression::CaseInsensitiveOption);
        rule.format = keywordFormat;
        rules.append(rule);
    }

    // Built-in functions
    QTextCharFormat functionFormat;
    functionFormat.setForeground(functionColor);

    for ( const QString &fn : sqlFunctions() )
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(fn),
            QRegularExpression::CaseInsensitiveOption);
        rule.format = functionFormat;
        rules.append(rule);
    }

    // Single-quoted strings
    QTextCharFormat stringFormat;
    stringFormat.setForeground(stringColor);
    Rule stringRule;
    stringRule.pattern = QRegularExpression("'[^']*'");
    stringRule.format  = stringFormat;
    rules.append(stringRule);

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(numberColor);
    Rule numberRule;
    numberRule.pattern = QRegularExpression("\\b[0-9]+(\\.[0-9]+)?\\b");
    numberRule.format  = numberFormat;
    rules.append(numberRule);

    // Single-line comments (--)
    QTextCharFormat singleLineCommentFormat;
    singleLineCommentFormat.setForeground(commentColor);
    Rule slComment;
    slComment.pattern = QRegularExpression("--[^\n]*");
    slComment.format  = singleLineCommentFormat;
    rules.append(slComment);

    // Multi-line block comment format and delimiters
    multiLineCommentFormat.setForeground(commentColor);
    blockCommentStart = QRegularExpression("/\\*");
    blockCommentEnd   = QRegularExpression("\\*/");

    // identifierColor is applied in setUserIdentifiers() using m_isDark
}

void SqlHighlighter::setUserIdentifiers(const QStringList &identifiers)
{
    identifierRules.clear();

    // Re-derive the identifier color from m_isDark so it stays in sync.
    const QColor identifierColor = isDark ? QColor(156, 220, 254) : QColor(0, 16, 128);

    QTextCharFormat identifierFormat;
    identifierFormat.setForeground(identifierColor);

    for (const QString &id : identifiers)
    {
        Rule rule;
        rule.pattern = QRegularExpression(
            QString("\\b%1\\b").arg(QRegularExpression::escape(id)),
            QRegularExpression::CaseInsensitiveOption);
        rule.format = identifierFormat;
        identifierRules.append(rule);
    }

    rehighlight();
}

void SqlHighlighter::highlightBlock(const QString &text)
{
    // Schema identifiers first (lowest priority - keywords override them below)
    for ( const Rule &rule : static_cast<const QVector<Rule>&>(identifierRules) )
    {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // Keywords, functions, strings, numbers, single-line comments
    for ( const Rule &rule : static_cast<const QVector<Rule>&>(rules) )
    {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // Multi-line block comments - tracked across blocks via block state
    setCurrentBlockState(0);

    int startIndex = (previousBlockState() == 1) ? 0 : text.indexOf(blockCommentStart);

    while (startIndex >= 0)
    {
        QRegularExpressionMatch endMatch = blockCommentEnd.match(text, startIndex);
        int endIndex = endMatch.capturedStart();
        int len;
        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            len = text.length() - startIndex;
        }
        else
        {
            len = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, len, multiLineCommentFormat);
        startIndex = text.indexOf(blockCommentStart, startIndex + len);
    }
}
