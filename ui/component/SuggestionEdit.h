#ifndef QLOG_UI_COMPONENT_SUGGESTIONEDIT_H
#define QLOG_UI_COMPONENT_SUGGESTIONEDIT_H

#include <QLineEdit>
#include <QList>
#include <QStringList>

class QAbstractItemView;
class QCompleter;
class QStandardItemModel;
class QWidgetAction;

class SuggestionEdit : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY(QStringList predefinedValues READ predefinedValues WRITE setPredefinedValues)
    Q_PROPERTY(QString popupToolTip READ popupToolTip WRITE setPopupToolTip)

public:
    struct Item
    {
        QString text;
        QString value;
    };

    explicit SuggestionEdit(QWidget *parent = nullptr);
    QStringList predefinedValues() const;
    void setPredefinedValues(const QStringList &values);
    QStringList userValues() const;
    void setUserValues(const QStringList &values);
    void setGroupLabels(const QString &predefined, const QString &user);
    QString popupToolTip() const;
    void setPopupToolTip(const QString &text);
    void setSuggestWhileTyping(bool enabled);
    QAbstractItemView *popup() const;

public slots:
    void showSuggestions();

signals:
    void suggestionSelected(const QString &value);

protected:
    virtual void populateSuggestions(const QString &query);
    void setPopupItems(const QList<Item> &predefined, const QList<Item> &user,
                       const QString &help = QString());
    void hideSuggestions();
    void refreshSuggestions();
    bool event(QEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void updatePopup(bool showAll = false);
    void insertSuggestion(const QModelIndex &index);
    void addGroup(const QList<Item> &items, const QString &label);

    QWidgetAction *suggestionAction;
    QCompleter *suggestionCompleter;
    QStandardItemModel *suggestionModel;
    QStringList predefinedSuggestions;
    QStringList userSuggestions;
    QString predefinedLabel;
    QString userLabel;
    bool suggestOnTyping = false;
};

#endif
