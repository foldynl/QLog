#include "SuggestionEdit.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QStyle>
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.suggestionedit");

SuggestionEdit::SuggestionEdit(QWidget *parent) :
    QLineEdit(parent),
    suggestionAction(addAction(style()->standardIcon(QStyle::SP_ArrowDown), QLineEdit::TrailingPosition)),
    suggestionCompleter(new QCompleter(this)),
    suggestionModel(new QStandardItemModel(this))
{
    FCT_IDENTIFICATION;

    setPopupToolTip(tr("Show Suggestions"));
    connect(suggestionAction, &QAction::triggered, this, &SuggestionEdit::showSuggestions);

    // Associate the popup without QLineEdit's automatic insertion on highlighting.
    suggestionCompleter->setWidget(this);
    suggestionCompleter->setModel(suggestionModel);
    suggestionCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    suggestionCompleter->setMaxVisibleItems(10);
    suggestionCompleter->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    suggestionCompleter->popup()->installEventFilter(this);

    connect(suggestionCompleter, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, &SuggestionEdit::insertSuggestion);
    connect(this, &QLineEdit::textEdited, this, [this]()
    {
        updatePopup();
    });
    connect(this, &QLineEdit::textChanged, this, [this](const QString &text)
    {
        if ( text.isEmpty() )
            hideSuggestions();
    });
}

QStringList SuggestionEdit::predefinedValues() const
{
    FCT_IDENTIFICATION;

    return predefinedSuggestions;
}

void SuggestionEdit::setPredefinedValues(const QStringList &values)
{
    FCT_IDENTIFICATION;

    predefinedSuggestions = values;
    refreshSuggestions();
}

void SuggestionEdit::setUserValues(const QStringList &values)
{
    FCT_IDENTIFICATION;

    userSuggestions = values;
    refreshSuggestions();
}

void SuggestionEdit::setGroupLabels(const QString &predefined, const QString &user)
{
    FCT_IDENTIFICATION;

    predefinedLabel = predefined;
    userLabel = user;
    refreshSuggestions();
}

QString SuggestionEdit::popupToolTip() const
{
    FCT_IDENTIFICATION;

    return suggestionAction->toolTip();
}

void SuggestionEdit::setPopupToolTip(const QString &text)
{
    FCT_IDENTIFICATION;

    suggestionAction->setText(text);
    suggestionAction->setToolTip(text);
}

void SuggestionEdit::setSuggestWhileTyping(bool enabled)
{
    FCT_IDENTIFICATION;

    suggestOnTyping = enabled;
    refreshSuggestions();
}

void SuggestionEdit::populateSuggestions(const QString &query)
{
    FCT_IDENTIFICATION;

    QList<Item> predefined;
    QList<Item> user;
    for ( const QString &value : predefinedSuggestions )
        if ( value.contains(query, Qt::CaseInsensitive) )
            predefined.append({value, value});
    for ( const QString &value : userSuggestions )
        if ( !predefinedSuggestions.contains(value) && value.contains(query, Qt::CaseInsensitive) )
            user.append({value, value});
    setPopupItems(predefined, user);
}

void SuggestionEdit::addGroup(const QList<Item> &items, const QString &label)
{
    FCT_IDENTIFICATION;

    if ( items.isEmpty() )
        return;
    if ( !label.isEmpty() )
    {
        QStandardItem *heading = new QStandardItem(label);
        heading->setFlags(Qt::NoItemFlags);
        QFont headingFont = font();
        headingFont.setBold(true);
        heading->setFont(headingFont);
        suggestionModel->appendRow(heading);
    }
    for ( const Item &entry : items )
    {
        QStandardItem *item = new QStandardItem(entry.text);
        item->setData(entry.value, Qt::UserRole);
        item->setEditable(false);
        suggestionModel->appendRow(item);
    }
}

void SuggestionEdit::setPopupItems(const QList<Item> &predefined, const QList<Item> &user,
                                  const QString &help)
{
    FCT_IDENTIFICATION;

    suggestionModel->clear();
    if ( !help.isEmpty() )
    {
        // QCompleter sizes using the first row; keep help single-line and above the groups.
        const QStringList lines = help.split(QLatin1Char('\n'));
        for ( const QString &line : lines )
        {
            QStandardItem *item = new QStandardItem(line);
            item->setFlags(Qt::NoItemFlags);
            suggestionModel->appendRow(item);
        }
    }
    addGroup(user, userLabel);
    addGroup(predefined, predefinedLabel);
}

void SuggestionEdit::hideSuggestions()
{
    FCT_IDENTIFICATION;

    suggestionCompleter->popup()->hide();
}

void SuggestionEdit::refreshSuggestions()
{
    FCT_IDENTIFICATION;

    if ( suggestionCompleter->popup()->isVisible() )
        updatePopup(!suggestOnTyping);
}

void SuggestionEdit::updatePopup(bool showAll)
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() || !hasFocus() || (!showAll && (!suggestOnTyping || text().trimmed().size() < 2)) )
    {
        hideSuggestions();
        return;
    }

    populateSuggestions(showAll ? QString() : text().trimmed());
    if ( suggestionModel->rowCount() == 0 )
    {
        hideSuggestions();
        return;
    }

    QRect popupRect = rect();
    popupRect.setWidth(qMax(width(), suggestionCompleter->popup()->sizeHintForColumn(0)
                                  + style()->pixelMetric(QStyle::PM_ScrollBarExtent)));
    suggestionCompleter->complete(popupRect);
    // Enter keeps the field's normal behavior until the operator explicitly selects a row.
    suggestionCompleter->popup()->setCurrentIndex(QModelIndex());
}

void SuggestionEdit::insertSuggestion(const QModelIndex &index)
{
    FCT_IDENTIFICATION;

    if ( !(index.flags() & Qt::ItemIsSelectable) )
        return;
    const QString value = index.data(Qt::UserRole).toString();
    hideSuggestions();
    setText(value);
    setFocus();
    if ( suggestOnTyping )
        updatePopup();
}

void SuggestionEdit::showSuggestions()
{
    FCT_IDENTIFICATION;

    setFocus();
    updatePopup(true);
}

bool SuggestionEdit::event(QEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->type() == QEvent::Hide || (event->type() == QEvent::EnabledChange && !isEnabled()) )
        suggestionCompleter->popup()->hide();
    if ( event->type() == QEvent::ShortcutOverride && suggestionCompleter->popup()->isVisible() )
    {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if ( key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter
             || key == Qt::Key_Up || key == Qt::Key_Down )
        {
            event->accept();
            return true;
        }
    }
    return QLineEdit::event(event);
}

bool SuggestionEdit::eventFilter(QObject *object, QEvent *event)
{
    FCT_IDENTIFICATION;

    if ( object == suggestionCompleter->popup() && event->type() == QEvent::KeyPress )
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();
        if ( key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter
             || key == Qt::Key_Up || key == Qt::Key_Down )
        {
            keyPressEvent(keyEvent);
            return true;
        }
    }
    return QLineEdit::eventFilter(object, event);
}

void SuggestionEdit::keyPressEvent(QKeyEvent *event)
{
    FCT_IDENTIFICATION;

    QAbstractItemView *popup = suggestionCompleter->popup();
    const int key = event->key();
    if ( key == Qt::Key_Up || key == Qt::Key_Down )
    {
        if ( !popup->isVisible() )
            showSuggestions();
        const int step = key == Qt::Key_Down ? 1 : -1;
        int row = popup->currentIndex().isValid() ? popup->currentIndex().row()
                                                : (step > 0 ? -1 : suggestionModel->rowCount());
        while ( (row += step) >= 0 && row < suggestionModel->rowCount() )
        {
            const QModelIndex index = suggestionCompleter->completionModel()->index(row, 0);
            if ( index.flags() & Qt::ItemIsSelectable )
            {
                popup->setCurrentIndex(index);
                popup->scrollTo(index);
                break;
            }
        }
        event->accept();
        return;
    }
    if ( popup->isVisible() )
    {
        if ( key == Qt::Key_Escape )
        {
            popup->hide();
            event->accept();
            return;
        }
        if ( key == Qt::Key_Return || key == Qt::Key_Enter )
        {
            const QModelIndex index = popup->currentIndex();
            popup->hide();
            if ( index.flags() & Qt::ItemIsSelectable )
            {
                insertSuggestion(index);
                event->accept();
                return;
            }
        }
    }
    QLineEdit::keyPressEvent(event);
}

void SuggestionEdit::focusOutEvent(QFocusEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->reason() != Qt::PopupFocusReason )
        suggestionCompleter->popup()->hide();
    QLineEdit::focusOutEvent(event);
}
