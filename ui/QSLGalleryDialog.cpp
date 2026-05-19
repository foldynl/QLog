#include <algorithm>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QApplication>
#include <QStyle>
#include <QTimeZone>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QMenu>
#include <QFileDialog>

#include "QSLGalleryDialog.h"
#include "ui_QSLGalleryDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.qslgallerydialog");

class QSLCardDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.icon = QIcon();
        opt.decorationSize = QSize();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QRect rect = option.rect;
        const int iconW = option.decorationSize.width();
        const int iconH = option.decorationSize.height();
        const int textMargin = 4;

        // Draw icon centered horizontally
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        const int iconX = rect.x() + (rect.width() - iconW) / 2;
        const int iconY = rect.y() + textMargin;
        icon.paint(painter, iconX, iconY, iconW, iconH);

        // Text area below icon
        const int textY = iconY + iconH + textMargin;
        const QRect textRect(rect.x() + textMargin, textY,
                             rect.width() - 2 * textMargin, rect.bottom() - textY);

        // Bold callsign
        const QString callsign = index.data(QSLGalleryDialog::CallsignRole).toString();
        QFont boldFont = option.font;
        boldFont.setBold(true);
        painter->setFont(boldFont);
        painter->setPen(option.palette.color(QPalette::Text));

        const QFontMetrics boldFm(boldFont);
        const QString elidedCallsign = boldFm.elidedText(callsign, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, elidedCallsign);

        // Normal date below callsign
        const QString dateStr = index.data(QSLGalleryDialog::DateStringRole).toString();
        QFont normalFont = option.font;
        painter->setFont(normalFont);

        const QRect dateRect(textRect.x(), textY + boldFm.height() + 1,
                             textRect.width(), textRect.height() - boldFm.height());
        const QFontMetrics normalFm(normalFont);
        const QString elidedDate = normalFm.elidedText(dateStr, Qt::ElideRight, dateRect.width());
        painter->drawText(dateRect, Qt::AlignHCenter | Qt::AlignTop, elidedDate);

        // Draw favorite star in the top-right corner of the icon
        if ( index.data(QSLGalleryDialog::FavoriteRole).toBool() )
        {
            QFont starFont = option.font;
            starFont.setPixelSize(14);
            painter->setFont(starFont);

            const QString star = QStringLiteral("\u2605");
            const int starX = iconX + iconW - 14;
            const int starY = iconY + 14;

            // Black outline
            painter->setPen(Qt::black);
            painter->drawText(starX - 1, starY, star);
            painter->drawText(starX + 1, starY, star);
            painter->drawText(starX, starY - 1, star);
            painter->drawText(starX, starY + 1, star);

            // Yellow star
            painter->setPen(QColor(0xFF, 0xD7, 0x00));
            painter->drawText(starX, starY, star);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &) const override
    {
        const int w = option.decorationSize.width() + 20;
        const int h = option.decorationSize.height() + QFontMetrics(option.font).height() * 2 + 16;
        return QSize(w, h);
    }
};

QSLGalleryDialog::QSLGalleryDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSLGalleryDialog),
    scrollTimer(new QTimer(this)),
    tempDir(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    // tree 20%, gallery 80%
    ui->splitter->setStretchFactor(0, 2);
    ui->splitter->setStretchFactor(1, 8);

    ui->cardListWidget->setItemDelegate(new QSLCardDelegate(ui->cardListWidget));

    connect(ui->filterTree, &QTreeWidget::currentItemChanged, this, [this]() { filterTreeSelectionChanged(); });
    connect(ui->cardListWidget, &QListWidget::itemDoubleClicked, this, &QSLGalleryDialog::cardDoubleClicked);

    ui->cardListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->cardListWidget, &QWidget::customContextMenuRequested, this, &QSLGalleryDialog::showContextMenu);

    // Lazy loading
    scrollTimer->setSingleShot(true);
    scrollTimer->setInterval(150);
    connect(scrollTimer, &QTimer::timeout, this, &QSLGalleryDialog::loadVisibleThumbnails);
    connect(ui->cardListWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { scrollTimer->start(); });
    connect(ui->exportFilteredButton, &QPushButton::clicked, this, &QSLGalleryDialog::exportFiltered);

    ui->sortCombo->addItem(tr("Date (Newest)"), SORT_DATE_DESC);
    ui->sortCombo->addItem(tr("Date (Oldest)"), SORT_DATE_ASC);
    ui->sortCombo->addItem(tr("Callsign (A-Z)"), SORT_CALLSIGN_ASC);
    ui->sortCombo->addItem(tr("Callsign (Z-A)"), SORT_CALLSIGN_DESC);

    connect(ui->sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { loadGallery(); });

    searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300);

    connect(searchTimer, &QTimer::timeout, this, [this]()
    {
        const QString text = ui->searchEdit->text().trimmed().toUpper();
        int visibleCount = 0;

        for ( int i = 0; i < ui->cardListWidget->count(); ++i )
        {
            QListWidgetItem *item = ui->cardListWidget->item(i);
            const bool match = text.isEmpty() || item->data(CallsignRole).toString().toUpper().contains(text);
            item->setHidden(!match);

            if ( match )
                ++visibleCount;
        }

        ui->statusLabel->setText(tr("%n QSL card(s)", "", visibleCount));
        ui->exportFilteredButton->setEnabled(visibleCount > 0);
    });

    connect(ui->searchEdit, &QLineEdit::textChanged, this, [this]() { searchTimer->start(); });

    buildFilterTree();
}

QSLGalleryDialog::~QSLGalleryDialog()
{
    FCT_IDENTIFICATION;

    delete tempDir;
    delete ui;
}

void QSLGalleryDialog::buildFilterTree()
{
    FCT_IDENTIFICATION;

    ui->filterTree->clear();

    // "All" root item
    QTreeWidgetItem *allItem = new QTreeWidgetItem(ui->filterTree);
    allItem->setText(0, tr("All QSL Cards"));
    allItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    allItem->setData(0, Qt::UserRole, FILTER_ALL);

    // "Favorites" item
    QTreeWidgetItem *favItem = new QTreeWidgetItem(ui->filterTree);
    favItem->setText(0, tr("Favorites"));
    favItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    favItem->setData(0, Qt::UserRole, FILTER_FAVORITE);

    const QSLStorage::FilterValues filters = qslStorage.getDistinctFilterValues();

    // "By Country" branch
    QTreeWidgetItem *countryRoot = new QTreeWidgetItem(ui->filterTree);
    countryRoot->setText(0, tr("By Country"));
    countryRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    countryRoot->setFlags(countryRoot->flags() & ~Qt::ItemIsSelectable);

    QList<QString> sortedCountries = filters.countries.keys();
    std::sort(sortedCountries.begin(), sortedCountries.end(), [](const QString &a, const QString &b)
    {
        return a.localeAwareCompare(b) < 0;
    });

    for ( const QString &country : static_cast<const QList<QString>&>(sortedCountries) )
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(countryRoot);
        item->setText(0, country);
        item->setData(0, Qt::UserRole, FILTER_COUNTRY);
        item->setData(0, Qt::UserRole + 1, filters.countries.value(country));
    }

    // "By Date" branch (year -> months)
    QTreeWidgetItem *dateRoot = new QTreeWidgetItem(ui->filterTree);
    dateRoot->setText(0, tr("By Date"));
    dateRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    dateRoot->setFlags(dateRoot->flags() & ~Qt::ItemIsSelectable);

    // Iterate years in descending order
    QStringList years = filters.yearMonths.keys();
    std::sort(years.begin(), years.end(), std::greater<QString>());

    for ( const QString &year : static_cast<const QStringList&>(years) )
    {
        QTreeWidgetItem *yearItem = new QTreeWidgetItem(dateRoot);
        yearItem->setText(0, year);
        yearItem->setData(0, Qt::UserRole, FILTER_YEAR);
        yearItem->setData(0, Qt::UserRole + 1, year);

        const QStringList &months = filters.yearMonths.value(year);

        for ( const QString &month : months )
        {
            QTreeWidgetItem *monthItem = new QTreeWidgetItem(yearItem);
            monthItem->setText(0, month);
            monthItem->setData(0, Qt::UserRole, FILTER_MONTH);
            monthItem->setData(0, Qt::UserRole + 1, year);
            monthItem->setData(0, Qt::UserRole + 2, month);
        }
    }

    // "By Band" branch
    QTreeWidgetItem *bandRoot = new QTreeWidgetItem(ui->filterTree);
    bandRoot->setText(0, tr("By Band"));
    bandRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    bandRoot->setFlags(bandRoot->flags() & ~Qt::ItemIsSelectable);

    for ( const QString &band : filters.bands )
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(bandRoot);
        item->setText(0, band);
        item->setData(0, Qt::UserRole, FILTER_BAND);
        item->setData(0, Qt::UserRole + 1, band);
    }

    // "By Mode" branch
    QTreeWidgetItem *modeRoot = new QTreeWidgetItem(ui->filterTree);
    modeRoot->setText(0, tr("By Mode"));
    modeRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    modeRoot->setFlags(modeRoot->flags() & ~Qt::ItemIsSelectable);

    for ( const QString &mode : filters.modes )
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(modeRoot);
        item->setText(0, mode);
        item->setData(0, Qt::UserRole, FILTER_MODE);
        item->setData(0, Qt::UserRole + 1, mode);
    }

    // "By Continent" branch
    QTreeWidgetItem *contRoot = new QTreeWidgetItem(ui->filterTree);
    contRoot->setText(0, tr("By Continent"));
    contRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    contRoot->setFlags(contRoot->flags() & ~Qt::ItemIsSelectable);

    for ( const QString &cont : filters.continents )
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(contRoot);
        item->setText(0, cont);
        item->setData(0, Qt::UserRole, FILTER_CONTINENT);
        item->setData(0, Qt::UserRole + 1, cont);
    }

    // Select "All" by default
    ui->filterTree->setCurrentItem(allItem);
}

void QSLGalleryDialog::filterTreeSelectionChanged()
{
    FCT_IDENTIFICATION;

    ui->searchEdit->blockSignals(true);
    ui->searchEdit->clear();
    ui->searchEdit->blockSignals(false);

    loadGallery();
}

void QSLGalleryDialog::loadGallery()
{
    FCT_IDENTIFICATION;

    const QTreeWidgetItem *current = ui->filterTree->currentItem();

    if ( !current )
        return;

    const FilterType filter = static_cast<FilterType>(current->data(0, Qt::UserRole).toInt());
    QList<QSLGalleryItem> items;

    switch ( filter )
    {
    case FILTER_ALL:
        items = qslStorage.getGalleryItems();
        break;

    case FILTER_FAVORITE:
        items = qslStorage.getGalleryItemsFavorite();
        break;

    case FILTER_COUNTRY:
    {
        const int dxcc = current->data(0, Qt::UserRole + 1).toInt();
        items = qslStorage.getGalleryItemsByDxcc(dxcc);
        break;
    }

    case FILTER_YEAR:
    {
        const QString year = current->data(0, Qt::UserRole + 1).toString();
        items = qslStorage.getGalleryItemsByYear(year);
        break;
    }

    case FILTER_MONTH:
    {
        const QString year = current->data(0, Qt::UserRole + 1).toString();
        const QString month = current->data(0, Qt::UserRole + 2).toString();
        items = qslStorage.getGalleryItemsByYearMonth(year, month);
        break;
    }

    case FILTER_BAND:
    {
        const QString band = current->data(0, Qt::UserRole + 1).toString();
        items = qslStorage.getGalleryItemsByBand(band);
        break;
    }

    case FILTER_MODE:
    {
        const QString mode = current->data(0, Qt::UserRole + 1).toString();
        items = qslStorage.getGalleryItemsByMode(mode);
        break;
    }

    case FILTER_CONTINENT:
    {
        const QString continent = current->data(0, Qt::UserRole + 1).toString();
        items = qslStorage.getGalleryItemsByContinent(continent);
        break;
    }
    }

    populateItems(items);
}

void QSLGalleryDialog::populateItems(const QList<QSLGalleryItem> &items)
{
    FCT_IDENTIFICATION;

    ui->cardListWidget->clear();

    QList<QSLGalleryItem> sorted = items;
    const SortOrder order = static_cast<SortOrder>(ui->sortCombo->currentData().toInt());

    std::sort(sorted.begin(), sorted.end(), [order](const QSLGalleryItem &a, const QSLGalleryItem &b)
    {
        switch ( order )
        {
        case SORT_DATE_ASC:
            return a.startTime < b.startTime;
        case SORT_CALLSIGN_ASC:
            return a.callsign.compare(b.callsign, Qt::CaseInsensitive) < 0;
        case SORT_CALLSIGN_DESC:
            return a.callsign.compare(b.callsign, Qt::CaseInsensitive) > 0;
        case SORT_DATE_DESC:
        default:
            return a.startTime > b.startTime;
        }
    });

    // Create a placeholder pixmap
    QPixmap placeholder(150, 112);
    placeholder.fill(QColor(220, 220, 220));

    for ( const QSLGalleryItem &item : sorted )
    {
        const QString dateStr = item.startTime.toTimeZone(QTimeZone::utc())
                                              .toString(locale.formatDateTimeShortWithYYYY());

        QListWidgetItem *listItem = new QListWidgetItem(QIcon(placeholder), QString());
        listItem->setData(ContactIdRole, static_cast<quint64>(item.contactId));
        listItem->setData(SourceRole, static_cast<int>(item.source));
        listItem->setData(NameRole, item.name);
        listItem->setData(ThumbnailLoadedRole, false);
        listItem->setData(CallsignRole, item.callsign);
        listItem->setData(DateStringRole, dateStr);
        listItem->setData(FavoriteRole, item.favorite);

        ui->cardListWidget->addItem(listItem);
    }

    ui->statusLabel->setText(tr("%n QSL card(s)", "", items.count()));
    ui->exportFilteredButton->setEnabled(!items.isEmpty());

    // initial thumbnail load
    QTimer::singleShot(0, this, &QSLGalleryDialog::loadVisibleThumbnails);
}

void QSLGalleryDialog::loadVisibleThumbnails()
{
    FCT_IDENTIFICATION;

    const QListWidget *lw = ui->cardListWidget;
    const int totalCount = lw->count();

    if ( totalCount == 0 )
        return;

    // Find visible range using viewport geometry
    const QRect viewportRect = lw->viewport()->rect();
    const QModelIndex topLeft = lw->indexAt(viewportRect.topLeft());
    const QModelIndex bottomRight = lw->indexAt(viewportRect.bottomRight());

    int firstVisible = topLeft.isValid() ? topLeft.row() : 0;
    int lastVisible = bottomRight.isValid() ? bottomRight.row() : totalCount - 1;

    // If bottomRight is invalid, scan forward to find last visible item
    if ( !bottomRight.isValid() && topLeft.isValid() )
    {
        for ( int i = firstVisible; i < totalCount; ++i )
        {
            const QRect itemRect = lw->visualItemRect(lw->item(i));
            if ( !itemRect.intersects(viewportRect) && i > firstVisible )
            {
                lastVisible = i - 1;
                break;
            }
            lastVisible = i;
        }
    }

    // Add margin for smooth scrolling
    const int margin = 10;
    firstVisible = qMax(0, firstVisible - margin);
    lastVisible = qMin(totalCount - 1, lastVisible + margin);

    qCDebug(runtime) << "Loading thumbnails" << firstVisible << "-" << lastVisible
                     << "of" << totalCount;

    for ( int i = firstVisible; i <= lastVisible; ++i )
    {
        QListWidgetItem *item = lw->item(i);

        if ( item->data(ThumbnailLoadedRole).toBool() )
            continue;

        const qulonglong contactId = item->data(ContactIdRole).toULongLong();
        const int source = item->data(SourceRole).toInt();
        const QString name = item->data(NameRole).toString();
        const QByteArray data = qslStorage.getQSLData(contactId, source, name);

        if ( !data.isEmpty() )
        {
            item->setIcon(QIcon(createThumbnail(data, name)));

            const QMimeType mimeType = mimeDb.mimeTypeForData(data);

            if ( mimeType.name().startsWith("image/") )
                item->setToolTip(QString("<img src='data:%1;base64, %2' width='600'>")
                                     .arg(mimeType.name(), QString(data.toBase64())));
            else
                item->setToolTip(QString("%1 (%2)").arg(name, mimeType.comment()));
        }

        item->setData(ThumbnailLoadedRole, true);
    }
}

QPixmap QSLGalleryDialog::createThumbnail(const QByteArray &data, const QString &name) const
{
    FCT_IDENTIFICATION;

    const QMimeType mimeType = mimeDb.mimeTypeForData(data);

    if ( mimeType.name().startsWith("image/") )
    {
        QPixmap pixmap;
        if ( pixmap.loadFromData(data) )
            return pixmap.scaled(150, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Non-image file: draw file type label over generic icon
    QPixmap pixmap(150, 112);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    const QIcon fileIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    fileIcon.paint(&p, 0, 0, 150, 112);

    const QString suffix = QFileInfo(name).suffix().toUpper();

    if ( !suffix.isEmpty() )
    {
        QFont font = p.font();
        font.setPixelSize(18);
        font.setBold(true);
        p.setFont(font);

        const QRect textRect(0, 70, 150, 30);
        p.setPen(Qt::white);
        p.setBrush(QColor(0, 0, 0, 160));
        const QFontMetrics fm(font);
        const int textWidth = fm.horizontalAdvance(suffix) + 10;
        const int bgX = (150 - textWidth) / 2;
        p.drawRoundedRect(bgX, 72, textWidth, fm.height() + 4, 3, 3);

        p.setPen(Qt::white);
        p.drawText(textRect, Qt::AlignCenter, suffix);
    }

    p.end();
    return pixmap;
}

void QSLGalleryDialog::cardDoubleClicked(QListWidgetItem *item)
{
    FCT_IDENTIFICATION;

    openItem(item);
}

void QSLGalleryDialog::showContextMenu(const QPoint &pos)
{
    FCT_IDENTIFICATION;

    QListWidgetItem *item = ui->cardListWidget->itemAt(pos);

    if ( !item )
        return;

    QMenu menu(this);

    const bool isFav = item->data(FavoriteRole).toBool();

    menu.addAction(isFav ? tr("Remove from Favorites") : tr("Add to Favorites"),
                   this, [this, item] { toggleFavorite(item); });
    menu.addSeparator();
    menu.addAction(tr("Open"),    this, [this, item] { openItem(item); });
    menu.addAction(tr("Save..."), this, [this, item] { saveItem(item); });

    menu.exec(ui->cardListWidget->viewport()->mapToGlobal(pos));
}

void QSLGalleryDialog::openItem(const QListWidgetItem *item)
{
    FCT_IDENTIFICATION;

    if ( !item )
        return;

    const qulonglong contactId = item->data(ContactIdRole).toULongLong();
    const int source = item->data(SourceRole).toInt();
    const QString name = item->data(NameRole).toString();
    const QByteArray data = qslStorage.getQSLData(contactId, source, name);

    if ( data.isEmpty() )
    {
        qCWarning(runtime) << "No QSL data for" << contactId << source << name;
        return;
    }

    // Create temp dir if needed
    if ( !tempDir )
    {
        tempDir = new QTemporaryDir();
        if ( !tempDir->isValid() )
        {
            qCWarning(runtime) << "Cannot create temporary directory";
            return;
        }
    }

    const QString filePath = tempDir->path() + "/" + name;
    QFile file(filePath);

    if ( !file.open(QIODevice::WriteOnly) )
    {
        qCWarning(runtime) << "Cannot write temporary file" << filePath;
        return;
    }

    file.write(data);
    file.close();

    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void QSLGalleryDialog::saveItem(const QListWidgetItem *item)
{
    FCT_IDENTIFICATION;

    if ( !item )
        return;

    const qulonglong contactId = item->data(ContactIdRole).toULongLong();
    const int source = item->data(SourceRole).toInt();
    const QString name = item->data(NameRole).toString();
    const QByteArray data = qslStorage.getQSLData(contactId, source, name);

    if ( data.isEmpty() )
    {
        qCDebug(runtime) << "No QSL data for" << contactId << source << name;
        return;
    }

    const QString savePath = QFileDialog::getSaveFileName(this,
                                                          tr("Save QSL Card"),
                                                          QDir::homePath() + "/" + name);
    if ( savePath.isEmpty() )
        return;

    QFile file(savePath);

    if ( !file.open(QIODevice::WriteOnly) )
    {
        qCDebug(runtime) << "Cannot write file" << savePath;
        return;
    }

    file.write(data);
    file.close();

    qCDebug(runtime) << "QSL card saved to" << savePath;
}

void QSLGalleryDialog::toggleFavorite(QListWidgetItem *item)
{
    FCT_IDENTIFICATION;

    if ( !item )
        return;

    const qulonglong contactId = item->data(ContactIdRole).toULongLong();
    const QSLObject::SourceType source = static_cast<QSLObject::SourceType>(item->data(SourceRole).toInt());
    const QString name = item->data(NameRole).toString();
    const bool currentFav = item->data(FavoriteRole).toBool();

    if ( qslStorage.setFavorite(contactId, source, name, !currentFav) )
    {
        item->setData(FavoriteRole, !currentFav);
        ui->cardListWidget->update();
    }
}

void QSLGalleryDialog::exportFiltered()
{
    FCT_IDENTIFICATION;

    const int count = ui->cardListWidget->count();

    if ( count == 0 )
        return;

    const QString dir = QFileDialog::getExistingDirectory(this,
                                                           tr("Export QSL Cards"),
                                                           QDir::homePath());
    if ( dir.isEmpty() )
        return;

    int saved = 0;
    int visible = 0;

    for ( int i = 0; i < count; ++i )
    {
        const QListWidgetItem *item = ui->cardListWidget->item(i);

        if ( item->isHidden() )
            continue;

        ++visible;

        const qulonglong contactId = item->data(ContactIdRole).toULongLong();
        const int source = item->data(SourceRole).toInt();
        const QString name = item->data(NameRole).toString();
        const QByteArray data = qslStorage.getQSLData(contactId, source, name);

        if ( data.isEmpty() )
        {
            qCDebug(runtime) << "No QSL data for" << contactId << source << name;
            continue;
        }

        const QString filePath = QDir(dir).filePath(name);
        QFile file(filePath);

        if ( !file.open(QIODevice::WriteOnly) )
        {
            qCDebug(runtime) << "Cannot write file" << filePath;
            continue;
        }

        file.write(data);
        ++saved;
    }

    ui->statusLabel->setText(tr("Exported %1 of %2 cards").arg(saved).arg(visible));
    qCDebug(runtime) << "Exported" << saved << "of" << visible << "QSL cards to" << dir;
}
