#ifndef QLOG_UI_QSLGALLERYDIALOG_H
#define QLOG_UI_QSLGALLERYDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QTemporaryDir>
#include <QListWidget>
#include <QMimeDatabase>
#include "core/QSLStorage.h"
#include "core/LogLocale.h"

namespace Ui {
class QSLGalleryDialog;
}

class QSLGalleryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QSLGalleryDialog(QWidget *parent = nullptr);
    ~QSLGalleryDialog();

    // Exposed for delegate access
    enum ItemDataRole
    {
        ContactIdRole = Qt::UserRole,
        SourceRole = Qt::UserRole + 1,
        NameRole = Qt::UserRole + 2,
        ThumbnailLoadedRole = Qt::UserRole + 3,
        CallsignRole = Qt::UserRole + 4,
        DateStringRole = Qt::UserRole + 5,
        FavoriteRole = Qt::UserRole + 6
    };

private slots:
    void filterTreeSelectionChanged();
    void cardDoubleClicked(QListWidgetItem *item);
    void showContextMenu(const QPoint &pos);
    void loadVisibleThumbnails();

private:
    enum FilterType
    {
        FILTER_ALL = 0,
        FILTER_FAVORITE,
        FILTER_COUNTRY,
        FILTER_YEAR,
        FILTER_MONTH,
        FILTER_BAND,
        FILTER_MODE,
        FILTER_CONTINENT
    };

    enum SortOrder
    {
        SORT_DATE_DESC = 0,
        SORT_DATE_ASC,
        SORT_CALLSIGN_ASC,
        SORT_CALLSIGN_DESC
    };

    void buildFilterTree();
    void loadGallery();
    void populateItems(const QList<QSLGalleryItem> &items);
    QPixmap createThumbnail(const QByteArray &data, const QString &name) const;
    void openItem(const QListWidgetItem *item);
    void saveItem(const QListWidgetItem *item);
    void toggleFavorite(QListWidgetItem *item);
    void exportFiltered();

    Ui::QSLGalleryDialog *ui;
    QSLStorage qslStorage;
    QTimer *scrollTimer;
    QTimer *searchTimer;
    QTemporaryDir *tempDir;
    LogLocale locale;
    QMimeDatabase mimeDb;
};

#endif // QLOG_UI_QSLGALLERYDIALOG_H
