/*
    SPDX-FileCopyrightText: 2005 Joris Guisson <joris.guisson@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fileview.h"

#include <QActionGroup>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QToolBar>

#include <KConfigGroup>
#include <KFileWidget>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageBox_KTCompat>
#include <KRecentDirs>
#include <KSharedConfig>

#include "iwfilelistmodel.h"
#include "iwfiletreemodel.h"
#include <interfaces/functions.h>
#include <interfaces/torrentfileinterface.h>
#include <interfaces/torrentinterface.h>
#include <util/bitset.h>
#include <util/error.h>
#include <util/functions.h>
#include <util/log.h>
#include <util/timer.h>
#include <util/treefiltermodel.h>

using namespace bt;

namespace kt
{
FileView::FileView(QWidget *parent)
    : QWidget(parent)
    , model(nullptr)
    , show_list_of_files(false)
    , header_state_loaded(false)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    view = new QTreeView(this);
    toolbar = new QToolBar(this);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    layout->addWidget(toolbar);

    filter = new QLineEdit(this);
    filter->setPlaceholderText(i18n("Filter"));
    filter->setClearButtonEnabled(true);
    filter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(filter, &QLineEdit::textChanged, this, &FileView::setFilter);
    filter->hide();
    vbox->addWidget(filter);
    vbox->addWidget(view);
    layout->addItem(vbox);

    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setRootIsDecorated(false);
    view->setSortingEnabled(true);
    view->setAlternatingRowColors(true);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setUniformRowHeights(true);

    proxy_model = new TreeFilterModel(this);
    proxy_model->setSortRole(Qt::UserRole);
    if (show_list_of_files)
        model = new IWFileListModel(nullptr, this);
    else
        model = new IWFileTreeModel(nullptr, this);
    proxy_model->setSourceModel(model);
    view->setModel(proxy_model);

    setupActions();

    connect(view, &QTreeView::customContextMenuRequested, this, &FileView::showContextMenu);
    connect(view, &QTreeView::doubleClicked, this, &FileView::onDoubleClicked);

    setEnabled(false);
}

FileView::~FileView()
{
}

void FileView::setupActions()
{
    context_menu = new QMenu(this);
    open_action = context_menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("Open file", "Open"), this, &FileView::open);
    open_with_action =
        context_menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("Open file with", "Open With"), this, &FileView::openWith);
    check_data = context_menu->addAction(QIcon::fromTheme(QStringLiteral("kt-check-data")), i18n("Check File"), this, &FileView::checkFile);
    context_menu->addSeparator();
    download_first_action = context_menu->addAction(i18n("Download first"), this, &FileView::downloadFirst);
    download_normal_action = context_menu->addAction(i18n("Download normally"), this, &FileView::downloadNormal);
    download_last_action = context_menu->addAction(i18n("Download last"), this, &FileView::downloadLast);
    context_menu->addSeparator();
    dnd_action = context_menu->addAction(i18n("Do not download"), this, &FileView::doNotDownload);
    delete_action = context_menu->addAction(i18n("Delete File(s)"), this, &FileView::deleteFiles);
    context_menu->addSeparator();
    move_files_action = context_menu->addAction(i18n("Move File"), this, &FileView::moveFiles);
    context_menu->addSeparator();
    collapse_action = context_menu->addAction(i18n("Collapse Folder Tree"), this, &FileView::collapseTree);
    expand_action = context_menu->addAction(i18n("Expand Folder Tree"), this, &FileView::expandTree);

    QActionGroup *ag = new QActionGroup(this);
    show_tree_action = new QAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), i18n("File Tree"), this);
    connect(show_tree_action, &QAction::triggered, this, &FileView::showTree);
    show_list_action = new QAction(QIcon::fromTheme(QStringLiteral("view-list-text")), i18n("File List"), this);
    connect(show_list_action, &QAction::triggered, this, &FileView::showList);
    ag->addAction(show_list_action);
    ag->addAction(show_tree_action);
    ag->setExclusive(true);
    show_list_action->setCheckable(true);
    show_tree_action->setCheckable(true);
    toolbar->addAction(show_tree_action);
    toolbar->addAction(show_list_action);

    show_filter_action = new QAction(QIcon::fromTheme(QStringLiteral("view-filter")), i18n("Show Filter"), this);
    show_filter_action->setCheckable(true);
    connect(show_filter_action, &QAction::toggled, filter, &QLineEdit::setVisible);
    toolbar->addAction(show_filter_action);
}

void FileView::changeTC(bt::TorrentInterface *tc)
{
    if (tc == curr_tc.data())
        return;

    if (curr_tc)
        expanded_state_map[curr_tc.data()] = model->saveExpandedState(proxy_model, view);

    curr_tc = tc;
    setEnabled(tc != nullptr);
    model->changeTorrent(tc);
    if (tc) {
        connect(tc, &bt::TorrentInterface::missingFilesMarkedDND, this, &FileView::onMissingFileMarkedDND);

        view->setRootIsDecorated(!show_list_of_files && tc->getStats().multi_file_torrent);
        if (!show_list_of_files) {
            auto i = expanded_state_map.constFind(tc);
            if (i != expanded_state_map.constEnd())
                model->loadExpandedState(proxy_model, view, i.value());
            else
                view->expandAll();
        }
    }

#if 0
    if (!header_state_loaded) {
        view->resizeColumnToContents(0);
        header_state_loaded = true;
    }
#else
    view->resizeColumnToContents(0);
#endif
}

void FileView::onMissingFileMarkedDND(bt::TorrentInterface *tc)
{
    if (curr_tc.data() == tc)
        model->missingFilesMarkedDND();
}

void FileView::showContextMenu(const QPoint &p)
{
    if (!curr_tc)
        return;

    bt::TorrentInterface *tc = curr_tc.data();
    const TorrentStats &s = tc->getStats();

    QModelIndexList sel = view->selectionModel()->selectedRows();
    if (sel.count() == 0)
        return;

    if (sel.count() > 1) {
        download_first_action->setEnabled(true);
        download_normal_action->setEnabled(true);
        download_last_action->setEnabled(true);
        open_action->setEnabled(false);
        open_with_action->setEnabled(false);
        dnd_action->setEnabled(true);
        delete_action->setEnabled(true);
        context_menu->popup(view->viewport()->mapToGlobal(p));
        move_files_action->setEnabled(true);
        collapse_action->setEnabled(!show_list_of_files);
        expand_action->setEnabled(!show_list_of_files);
        check_data->setEnabled(true);
        return;
    }

    QModelIndex item = proxy_model->mapToSource(sel.front());
    bt::TorrentFileInterface *file = model->indexToFile(item);

    download_first_action->setEnabled(false);
    download_last_action->setEnabled(false);
    download_normal_action->setEnabled(false);
    dnd_action->setEnabled(false);
    delete_action->setEnabled(false);

    if (!s.multi_file_torrent) {
        open_action->setEnabled(true);
        open_with_action->setEnabled(true);
        move_files_action->setEnabled(true);
        preview_path = tc->getStats().output_path;
        collapse_action->setEnabled(false);
        expand_action->setEnabled(false);
        check_data->setEnabled(true);
    } else if (file) {
        check_data->setEnabled(true);
        move_files_action->setEnabled(true);
        collapse_action->setEnabled(false);
        expand_action->setEnabled(false);
        if (!file->isNull()) {
            open_action->setEnabled(true);
            open_with_action->setEnabled(true);
            preview_path = file->getPathOnDisk();

            download_first_action->setEnabled(file->getPriority() != FIRST_PRIORITY);
            download_normal_action->setEnabled(file->getPriority() != NORMAL_PRIORITY);
            download_last_action->setEnabled(file->getPriority() != LAST_PRIORITY);
            dnd_action->setEnabled(file->getPriority() != ONLY_SEED_PRIORITY);
            delete_action->setEnabled(file->getPriority() != EXCLUDED);
        } else {
            open_action->setEnabled(false);
            open_with_action->setEnabled(false);
        }
    } else {
        check_data->setEnabled(false);
        move_files_action->setEnabled(false);
        download_first_action->setEnabled(true);
        download_normal_action->setEnabled(true);
        download_last_action->setEnabled(true);
        dnd_action->setEnabled(true);
        delete_action->setEnabled(true);
        open_action->setEnabled(true);
        open_with_action->setEnabled(true);
        preview_path = tc->getStats().output_path + model->dirPath(item);
        collapse_action->setEnabled(!show_list_of_files);
        expand_action->setEnabled(!show_list_of_files);
    }

    context_menu->popup(view->viewport()->mapToGlobal(p));
}

void FileView::open()
{
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(preview_path));
    job->start();
}

void FileView::openWith()
{
    auto *job = new KIO::ApplicationLauncherJob();
    job->setUrls({QUrl::fromLocalFile(preview_path)});
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
}

void FileView::changePriority(bt::Priority newpriority)
{
    QModelIndexList sel = view->selectionModel()->selectedRows(2);
    for (QModelIndex &i : sel)
        i = proxy_model->mapToSource(i);

    model->changePriority(sel, newpriority);
    proxy_model->invalidate();
}

void FileView::downloadFirst()
{
    changePriority(FIRST_PRIORITY);
}

void FileView::downloadLast()
{
    changePriority(LAST_PRIORITY);
}

void FileView::downloadNormal()
{
    changePriority(NORMAL_PRIORITY);
}

void FileView::doNotDownload()
{
    changePriority(ONLY_SEED_PRIORITY);
}

void FileView::deleteFiles()
{
    QModelIndexList sel = view->selectionModel()->selectedRows();
    Uint32 n = sel.count();
    if (n == 1) { // single item can be a directory
        if (!model->indexToFile(proxy_model->mapToSource(sel.front())))
            n++;
    }

    QString msg = i18np("You will lose all data in this file, are you sure you want to do this?",
                        "You will lose all data in these files, are you sure you want to do this?",
                        n);

    if (KMessageBox::warningTwoActions(nullptr, msg, QString(), KStandardGuiItem::del(), KStandardGuiItem::cancel()) == KMessageBox::PrimaryAction)
        changePriority(EXCLUDED);
}

void FileView::moveFiles()
{
    if (!curr_tc)
        return;

    bt::TorrentInterface *tc = curr_tc.data();
    if (tc->getStats().multi_file_torrent) {
        const QModelIndexList sel = view->selectionModel()->selectedRows();
        QMap<bt::TorrentFileInterface *, QString> moves;

        QString recentDirClass;
        QString dir =
            QFileDialog::getExistingDirectory(this,
                                              i18n("Select a directory to move the data to."),
                                              KFileWidget::getStartUrl(QUrl(QLatin1String("kfiledialog:///saveTorrentData")), recentDirClass).toLocalFile());

        if (dir.isEmpty())
            return;

        if (!recentDirClass.isEmpty())
            KRecentDirs::add(recentDirClass, dir);

        for (const QModelIndex &idx : sel) {
            bt::TorrentFileInterface *tfi = model->indexToFile(proxy_model->mapToSource(idx));
            if (!tfi)
                continue;

            moves.insert(tfi, dir);
        }

        if (moves.count() > 0) {
            tc->moveTorrentFiles(moves);
        }
    } else {
        QString recentDirClass;
        QString dir =
            QFileDialog::getExistingDirectory(this,
                                              i18n("Select a directory to move the data to."),
                                              KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///saveTorrentData")), recentDirClass).toLocalFile());

        if (dir.isEmpty())
            return;

        if (!recentDirClass.isEmpty())
            KRecentDirs::add(recentDirClass, dir);

        tc->changeOutputDir(dir, bt::TorrentInterface::MOVE_FILES);
    }
}

void FileView::expandCollapseTree(const QModelIndex &idx, bool expand)
{
    int rowCount = proxy_model->rowCount(idx);
    for (int i = 0; i < rowCount; i++) {
        const QModelIndex &ridx = proxy_model->index(i, 0, idx);
        if (proxy_model->hasChildren(ridx))
            expandCollapseTree(ridx, expand);
    }
    view->setExpanded(idx, expand);
}

void FileView::expandCollapseSelected(bool expand)
{
    const QModelIndexList sel = view->selectionModel()->selectedRows();
    for (const QModelIndex &i : sel) {
        if (proxy_model->hasChildren(i))
            expandCollapseTree(i, expand);
    }
}

void FileView::collapseTree()
{
    expandCollapseSelected(false);
}

void FileView::expandTree()
{
    expandCollapseSelected(true);
}

void FileView::onDoubleClicked(const QModelIndex &index)
{
    if (!curr_tc)
        return;

    bt::TorrentInterface *tc = curr_tc.data();
    const TorrentStats &s = tc->getStats();

    QString pathToOpen;
    bool isMultimedia = false;
    bool isPreviewAvailable = false;
    int downloadPercentage = 0;
    int fileIndex = 0;

    if (s.multi_file_torrent) {
        bt::TorrentFileInterface *file = model->indexToFile(proxy_model->mapToSource(index));
        if (!file) {
            // directory
            pathToOpen = s.output_path + model->dirPath(proxy_model->mapToSource(index));
        } else {
            isMultimedia = (file->isVideo() || file->isAudio() || file->isMultimedia()) && !file->doNotDownload();
            if (isMultimedia) {
                isPreviewAvailable = file->isPreviewAvailable();
                downloadPercentage = file->getDownloadPercentage();
                fileIndex = file->getIndex();
            }
            pathToOpen = file->getPathOnDisk();
        }
    } else {
        isMultimedia = tc->isMultimedia();
        isPreviewAvailable = tc->readyForPreview();
        if (s.total_bytes)
            downloadPercentage = 100 - 100 * s.total_bytes_to_download / s.total_bytes;
        pathToOpen = s.output_path;
    }

    if (isMultimedia) {
        static QList<TorrentFileStream::Ptr> streams;
        bool doStream = false;
        if (!isPreviewAvailable) {
            doStream = KMessageBox::PrimaryAction
                == KMessageBox::questionTwoActions(this,
                                                   i18n("Not enough data downloaded for opening the file.\n\n"
                                                        "Enable sequential download mode for it to obtain necessary data with a higher priority?"),
                                                   QString(),
                                                   KGuiItem(i18nc("@action:button", "Enable Sequential Download Mode"), QStringLiteral("dialog-ok")),
                                                   KStandardGuiItem::cancel());
        } else if (downloadPercentage < 90) {
            doStream = true;
            // doStream = KMessageBox::Yes==KMessageBox::questionYesNo(this, i18n("Enable sequential download mode for this file?"),
            //    QString(), KStandardGuiItem::yes(), KStandardGuiItem::no(), QStringLiteral("SequentialModeOnFileOpen"));
        }
        if (doStream) {
            streams << tc->createTorrentFileStream(fileIndex, true, nullptr);
            if (streams.last().isNull())
                streams << tc->createTorrentFileStream(fileIndex, false, nullptr);
        }
        if (!isPreviewAvailable)
            return;
    }
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(pathToOpen));
    job->start();
}

void FileView::saveState(KSharedConfigPtr cfg)
{
    if (!model)
        return;

    KConfigGroup g = cfg->group("FileView");
    QByteArray s = view->header()->saveState();
    g.writeEntry("state", s.toBase64());
    g.writeEntry("show_list_of_files", show_list_of_files);
}

void FileView::loadState(KSharedConfigPtr cfg)
{
    KConfigGroup g = cfg->group("FileView");
    QByteArray s = g.readEntry("state", QByteArray());
    if (!s.isEmpty()) {
        QHeaderView *v = view->header();
        v->restoreState(QByteArray::fromBase64(s));
        view->sortByColumn(v->sortIndicatorSection(), v->sortIndicatorOrder());
        header_state_loaded = true;
    }

    bool show_list = g.readEntry("show_list_of_files", false);
    if (show_list_of_files != show_list)
        setShowListOfFiles(show_list);

    show_list_action->setChecked(show_list);
    show_tree_action->setChecked(!show_list);
}

void FileView::update()
{
    if (model)
        model->update();
}

void FileView::onTorrentRemoved(bt::TorrentInterface *tc)
{
    expanded_state_map.remove(tc);
}

void FileView::setShowListOfFiles(bool on)
{
    if (show_list_of_files == on)
        return;

    QByteArray header_state = view->header()->saveState();
    show_list_of_files = on;
    if (!curr_tc) {
        // no torrent, but still need to change the model
        proxy_model->setSourceModel(nullptr);
        delete model;
        if (show_list_of_files)
            model = new IWFileListModel(nullptr, this);
        else
            model = new IWFileTreeModel(nullptr, this);
        proxy_model->setSourceModel(model);
        view->header()->restoreState(header_state);
        return;
    }

    bt::TorrentInterface *tc = curr_tc.data();
    if (on)
        expanded_state_map[tc] = model->saveExpandedState(proxy_model, view);

    proxy_model->setSourceModel(nullptr);
    delete model;
    model = nullptr;

    if (show_list_of_files)
        model = new IWFileListModel(tc, this);
    else
        model = new IWFileTreeModel(tc, this);

    proxy_model->setSourceModel(model);
    view->setRootIsDecorated(!show_list_of_files && tc->getStats().multi_file_torrent);
    view->header()->restoreState(header_state);

    if (!on) {
        auto i = expanded_state_map.constFind(tc);
        if (i != expanded_state_map.constEnd())
            model->loadExpandedState(proxy_model, view, i.value());
        else
            view->expandAll();
    }

    collapse_action->setEnabled(!show_list_of_files);
    expand_action->setEnabled(!show_list_of_files);
}

void FileView::showTree()
{
    if (show_list_of_files)
        setShowListOfFiles(false);
}

void FileView::showList()
{
    if (!show_list_of_files)
        setShowListOfFiles(true);
}

void FileView::filePercentageChanged(bt::TorrentFileInterface *file, float percentage)
{
    if (model)
        model->filePercentageChanged(file, percentage);
}

void FileView::filePreviewChanged(bt::TorrentFileInterface *file, bool preview)
{
    if (model)
        model->filePreviewChanged(file, preview);
}

void FileView::setFilter(const QString &f)
{
    Q_UNUSED(f);
    proxy_model->setFilterFixedString(filter->text());
}

void FileView::checkFile()
{
    const QModelIndexList sel = view->selectionModel()->selectedRows();
    if (!curr_tc || sel.isEmpty())
        return;

    if (curr_tc.data()->getStats().multi_file_torrent) {
        bt::Uint32 from = curr_tc.data()->getStats().total_chunks;
        bt::Uint32 to = 0;
        for (const QModelIndex &idx : sel) {
            bt::TorrentFileInterface *tfi = model->indexToFile(proxy_model->mapToSource(idx));
            if (!tfi)
                continue;

            if (tfi->getFirstChunk() < from)
                from = tfi->getFirstChunk();
            if (tfi->getLastChunk() > to)
                to = tfi->getLastChunk();
        }
        curr_tc.data()->startDataCheck(false, from, to);
    } else
        curr_tc.data()->startDataCheck(false, 0, curr_tc.data()->getStats().total_chunks);
}

}

#include "moc_fileview.cpp"
