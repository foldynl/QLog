#include "SmartSearchBox.h"

#include <QVBoxLayout>
#include <QStandardItem>
#include <QAbstractTableModel>
#include <QPushButton>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QCoreApplication>
#include "models/SqlListModel.h"
#include "core/debug.h"

SmartSearchBox::SmartSearchBox(QWidget *parent)
    : QWidget(parent),
      searchField(new QLineEdit(this)),
      listView(new QListView(this)),
      popup(new QDialog(this, Qt::Popup)),
      filterModel(new QSortFilterProxyModel(this)),
      selectedColumn(0),
      maxRowsInList(10),
      highlightWhenEnable(false)
{
    filterModel->setSourceModel(nullptr);
    filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterModel->setFilterRole(Qt::DisplayRole);

    listView->setModel(filterModel);
    listView->setObjectName("SearchListView");

    searchField->setClearButtonEnabled(true);
    searchField->setObjectName("SearchBox");
    searchField->setPlaceholderText(tr("Search"));

    QVBoxLayout *popupLayout = new QVBoxLayout(popup);
    popupLayout->setContentsMargins(5, 5, 5, 5);
    popupLayout->addWidget(searchField);
    popupLayout->addWidget(listView);
    popup->setLayout(popupLayout);
    popup->setObjectName("SearchPopup");

    connect(searchField, &QLineEdit::textChanged, this, &SmartSearchBox::onTextChanged);
    connect(listView, &QListView::clicked, this, &SmartSearchBox::onItemClicked);

    openButton = new QPushButton(this);
    connect(openButton, &QPushButton::clicked, this, [this]()
    {
        popup->move(mapToGlobal(QPoint(0, height())));
        popup->show();
        adjustPopupSize();
        searchField->setFocus();
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(openButton);
    setLayout(mainLayout);

    searchField->installEventFilter(this);
    listView->installEventFilter(this);
    popup->installEventFilter(this);

    connect(popup, &QDialog::finished, this, [this]()
    {
        searchField->clear();
    });
}

void SmartSearchBox::setModel(QAbstractItemModel *model)
{
    if ( !model ) return ;

    SqlListModel *sqlModel = qobject_cast<SqlListModel*>(model);

    if ( sqlModel )
        while (sqlModel->canFetchMore()) sqlModel->fetchMore();

    filterModel->setSourceModel(model);
}

void SmartSearchBox::onTextChanged(const QString &text)
{
    filterModel->setFilterRegularExpression(QRegularExpression::escape(text));

    if ( filterModel->rowCount() == 0 )
    {
        listView->clearSelection();
        return;
    }

    QModelIndex indexToSelect;

    if (text.isEmpty())
        indexToSelect = currentValueSourceIndex.isValid()  ? filterModel->mapFromSource(currentValueSourceIndex)
                                                           : filterModel->index(0, selectedColumn);
    else
        indexToSelect = filterModel->index(0, selectedColumn);

    if ( indexToSelect.isValid() )
        listView->setCurrentIndex(indexToSelect);
}

void SmartSearchBox::onItemClicked(const QModelIndex &index)
{
    const QModelIndex &sourceIndex = filterModel->mapToSource(index);
    currentValueSourceIndex = sourceIndex;
    popup->hide();
    searchField->clear();
    changeButtonText(currentText());
}

bool SmartSearchBox::eventFilter(QObject *obj, QEvent *event)
{

    // This is a hack for pressing the ESC key for closing the Widget
    // It is defined as a Global Shortcut, but the problem is that the global shortcut consumes this event.
    // Therefore, the event never reaches this widget.
    // A suitable event was found for the ESC key press that is generated and does not negatively affect
    // the other widget functionalities.
    if ( event->type() == QEvent::KeyRelease
         && obj == popup
         && !this->hasFocus() && !popup->hasFocus() && !searchField->hasFocus() && ! listView->hasFocus())
    {
        searchField->clear();
        popup->hide();
        return QWidget::eventFilter(obj, event);
    }

    if ( event->type() != QEvent::KeyPress )
        return QWidget::eventFilter(obj, event);

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    int key = keyEvent->key();

    if ( obj == searchField )
    {
        if ( (key == Qt::Key_Down || key == Qt::Key_Up) && filterModel->rowCount() > 0 )
        {
            listView->setFocus();

            QModelIndex currIndex = listView->currentIndex();
            int currentRow = currIndex.isValid() ? currIndex.row()
                                                    : (key == Qt::Key_Up ? filterModel->rowCount() : -1);
            int nextRow = currentRow + (key == Qt::Key_Down ? 1 : -1);

            if ( nextRow < 0 ) nextRow = 0;
            else if ( nextRow >= filterModel->rowCount() ) nextRow = filterModel->rowCount() - 1;

            listView->setCurrentIndex(filterModel->index(nextRow, selectedColumn));
            return true;
        }

        if ( key == Qt::Key_Return || key == Qt::Key_Enter )
        {
            QModelIndex index = listView->currentIndex();
            if ( index.isValid() )
                onItemClicked(index);
            return true;
        }
    }
    else if ( obj == listView )
    {
        if ( key == Qt::Key_Return || key == Qt::Key_Enter )
        {
            QModelIndex index = listView->currentIndex();
            if (index.isValid())
                onItemClicked(index);
            return true;
        }

        if ( key == Qt::Key_Up && listView->currentIndex().row() == 0 )
        {
            searchField->setFocus();
            return true;
        }

        if ( !keyEvent->text().isEmpty() )
        {
            // send keytext to search widget
            searchField->setFocus();
            QKeyEvent forwardedEvent(QEvent::KeyPress, key, keyEvent->modifiers(), keyEvent->text());
            QCoreApplication::sendEvent(searchField, &forwardedEvent);
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SmartSearchBox::adjustPopupSize()
{
    if (!filterModel || filterModel->rowCount() == 0)
        return;

    int popupWidth = listView->sizeHintForColumn(selectedColumn) + 40; // padding
    listView->setMinimumWidth(popupWidth);

    int visibleRows = qMin(filterModel->rowCount(), maxRowsInList);
    int rowHeight = listView->sizeHintForRow(0);
    if (rowHeight <= 0)
        rowHeight = listView->fontMetrics().height();

    int searchFieldHeight = searchField->sizeHint().height();
    int popupHeight = searchFieldHeight + (visibleRows * rowHeight) + 20; // margin

    popup->resize(popupWidth + 10, popupHeight);
}

void SmartSearchBox::changeButtonText(const QString &text)
{
    if ( openButton->text() != text )
    {
        openButton->setText(text);
        adjustMaxSize();
        listView->setCurrentIndex(filterModel->mapFromSource(currentValueSourceIndex));
        emit currentTextChanged(text);
    }

    openButton->setStyleSheet( ( highlightWhenEnable
                                 && currentValueSourceIndex.row() > 0) ? "QPushButton "
                                                                  "{ border: 2px solid red; "
                                                                  "  border-radius: 4px; "
                                                                  "  padding: 2px;}"
                                                                : "");
}

void SmartSearchBox::setModelColumn(int column)
{
    filterModel->setFilterKeyColumn(column);
    listView->setModelColumn(column);
    selectedColumn = column;
}

QVariant SmartSearchBox::currentValue(int column)
{
    if ( !filterModel->sourceModel() )
        return {};

    return filterModel->sourceModel()->data(currentValueSourceIndex, Qt::UserRole + column);
}

QString SmartSearchBox::currentText() const
{
    if ( !filterModel->sourceModel() )
        return {};

    return filterModel->sourceModel()->data(currentValueSourceIndex).toString();
}

int SmartSearchBox::currentIndex()
{
    if (!filterModel
        || !filterModel->sourceModel()
        || !currentValueSourceIndex.isValid() )
        return 0;

    QModelIndex filteredIndex = filterModel->mapFromSource(currentValueSourceIndex);
    if (!filteredIndex.isValid())
        return 0;

    return filteredIndex.row();
}

void SmartSearchBox::setCurrentText(const QString &text)
{
    if (!filterModel || !filterModel->sourceModel()) return;

    QAbstractItemModel *model = filterModel->sourceModel();

    // default value is the first row value
    currentValueSourceIndex = model->index(0, selectedColumn);

    for ( int row = 0; row < model->rowCount(); ++row )
    {
        const QModelIndex &index = model->index(row, selectedColumn);
        QString itemText = index.data(Qt::DisplayRole).toString();

        if ( itemText.compare(text, Qt::CaseInsensitive) == 0 )
        {
            currentValueSourceIndex = index;
            break;
        }
    }
    changeButtonText(currentText());
}

void SmartSearchBox::setCurrentValue(const QVariant var, int column)
{
    if (!filterModel || !filterModel->sourceModel()) return;

    QAbstractItemModel *model = filterModel->sourceModel();

    // default value is the first row value
    currentValueSourceIndex = model->index(0, selectedColumn);
    for ( int row = 0; row < model->rowCount(); ++row )
    {
        const QModelIndex &index = model->index(row, selectedColumn);
        QVariant userData = index.data(Qt::UserRole + column);

        if ( userData.toString() == var.toString() )
        {
            currentValueSourceIndex = index;
            break;
        }
    }
    changeButtonText(currentText());
}

void SmartSearchBox::adjustMaxSize()
{
    if ( !filterModel
         || filterModel->sourceModel()
         || filterModel->sourceModel()->rowCount() == 0)
        return;

    const QModelIndex index = filterModel->sourceModel()->index(0, selectedColumn);
    if (!index.isValid())
        return;

    const QString text = filterModel->sourceModel()->data(index).toString();

    if (text.isEmpty())
        return;

    const QFontMetrics metrics(font());
    openButton->setMaximumWidth(metrics.horizontalAdvance(text) + 35); // + padding
}

void SmartSearchBox::refreshModel()
{
    if ( !filterModel->sourceModel() ) return ;

    SqlListModel *sqlModel = qobject_cast<SqlListModel*>(filterModel->sourceModel());

    if ( sqlModel )
    {
        QString text = currentText();
        sqlModel->refresh();
        setCurrentText(text);
    }
}

void SmartSearchBox::setHighlightWhenEnable(bool state)
{
    highlightWhenEnable = state;
}
