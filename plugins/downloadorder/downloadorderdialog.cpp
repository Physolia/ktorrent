/*
    SPDX-FileCopyrightText: 2008 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2008 Ivan Vasic <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "downloadorderdialog.h"

#include <KConfig>
#include <KConfigGroup>

#include <QMenu>

#include "downloadordermanager.h"
#include "downloadordermodel.h"
#include "downloadorderplugin.h"
#include <interfaces/torrentinterface.h>

namespace kt
{
DownloadOrderDialog::DownloadOrderDialog(DownloadOrderPlugin *plugin, bt::TorrentInterface *tor, QWidget *parent)
    : QDialog(parent)
    , tor(tor)
    , plugin(plugin)
{
    setupUi(this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &DownloadOrderDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &DownloadOrderDialog::reject);
    connect(this, &DownloadOrderDialog::accepted, this, &DownloadOrderDialog::commitDownloadOrder);
    setWindowTitle(i18n("File Download Order"));
    m_top_label->setText(i18n("File download order for <b>%1</b>:", tor->getDisplayName()));

    DownloadOrderManager *dom = plugin->manager(tor);
    m_custom_order_enabled->setChecked(dom != nullptr);
    m_order->setEnabled(dom != nullptr);
    m_move_up->setEnabled(false);
    m_move_down->setEnabled(false);
    m_move_top->setEnabled(false);
    m_move_bottom->setEnabled(false);
    m_search_files->setEnabled(false);

    m_move_up->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    connect(m_move_up, &QPushButton::clicked, this, &DownloadOrderDialog::moveUp);
    m_move_down->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    connect(m_move_down, &QPushButton::clicked, this, &DownloadOrderDialog::moveDown);
    m_move_top->setIcon(QIcon::fromTheme(QStringLiteral("go-top")));
    connect(m_move_top, &QPushButton::clicked, this, &DownloadOrderDialog::moveTop);
    m_move_bottom->setIcon(QIcon::fromTheme(QStringLiteral("go-bottom")));
    connect(m_move_bottom, &QPushButton::clicked, this, &DownloadOrderDialog::moveBottom);

    m_order->setSelectionMode(QAbstractItemView::ContiguousSelection);
    m_order->setDragEnabled(true);
    m_order->setAcceptDrops(true);
    m_order->setDropIndicatorShown(true);
    m_order->setDragDropMode(QAbstractItemView::InternalMove);

    model = new DownloadOrderModel(tor, this);
    if (dom)
        model->initOrder(dom->downloadOrder());
    m_order->setModel(model);

    QSize s = KSharedConfig::openConfig()->group("DownloadOrderDialog").readEntry("size", size());
    resize(s);

    connect(m_order->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DownloadOrderDialog::itemSelectionChanged);
    connect(m_custom_order_enabled, &QCheckBox::toggled, this, &DownloadOrderDialog::customOrderEnableToggled);
    connect(m_search_files, &QLineEdit::textChanged, this, &DownloadOrderDialog::search);

    QMenu *sort_by_menu = new QMenu(m_sort_by);
    sort_by_menu->addAction(i18n("Name"), model, &DownloadOrderModel::sortByName);
    sort_by_menu->addAction(i18n("Seasons and Episodes"), model, &DownloadOrderModel::sortBySeasonsAndEpisodes);
    sort_by_menu->addAction(i18n("Album Track Order"), model, &DownloadOrderModel::sortByAlbumTrackOrder);
    m_sort_by->setMenu(sort_by_menu);
    m_sort_by->setPopupMode(QToolButton::InstantPopup);
    m_sort_by->setEnabled(false);
}

DownloadOrderDialog::~DownloadOrderDialog()
{
    KSharedConfig::openConfig()->group("DownloadOrderDialog").writeEntry("size", size());
}

void DownloadOrderDialog::commitDownloadOrder()
{
    if (m_custom_order_enabled->isChecked()) {
        DownloadOrderManager *dom = plugin->manager(tor);
        if (!dom) {
            dom = plugin->createManager(tor);
            connect(tor, &bt::TorrentInterface::chunkDownloaded, dom, &DownloadOrderManager::chunkDownloaded);
        }

        dom->setDownloadOrder(model->downloadOrder());
        dom->save();
        dom->update();
    } else {
        DownloadOrderManager *dom = plugin->manager(tor);
        if (dom) {
            dom->disable();
            plugin->destroyManager(tor);
        }
    }
}

void DownloadOrderDialog::moveUp()
{
    QModelIndexList idx = m_order->selectionModel()->selectedRows();
    model->moveUp(idx.front().row(), idx.count());
    if (idx.front().row() > 0) {
        QItemSelection sel(model->index(idx.first().row() - 1), model->index(idx.last().row() - 1));
        m_order->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    }
}

void DownloadOrderDialog::moveTop()
{
    QModelIndexList idx = m_order->selectionModel()->selectedRows();
    model->moveTop(idx.front().row(), idx.count());
    if (idx.front().row() > 0) {
        QItemSelection sel(model->index(0), model->index(idx.count() - 1));
        m_order->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    }
}

void DownloadOrderDialog::moveDown()
{
    QModelIndexList idx = m_order->selectionModel()->selectedRows();
    model->moveDown(idx.front().row(), idx.count());
    if (idx.back().row() < (int)tor->getNumFiles() - 1) {
        QItemSelection sel(model->index(idx.first().row() + 1), model->index(idx.last().row() + 1));
        m_order->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    }
}

void DownloadOrderDialog::moveBottom()
{
    QModelIndexList idx = m_order->selectionModel()->selectedRows();
    model->moveBottom(idx.front().row(), idx.count());
    if (idx.back().row() < (int)tor->getNumFiles() - 1) {
        QItemSelection sel(model->index(tor->getNumFiles() - idx.size()), model->index(tor->getNumFiles() - 1));
        m_order->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    }
}

void DownloadOrderDialog::itemSelectionChanged(const QItemSelection &new_sel, const QItemSelection &old_sel)
{
    Q_UNUSED(old_sel);
    if (new_sel.empty()) {
        m_move_down->setEnabled(false);
        m_move_up->setEnabled(false);
        m_move_top->setEnabled(false);
        m_move_down->setEnabled(false);
    } else {
        bool up_ok = new_sel.front().topLeft().row() > 0;
        bool down_ok = new_sel.back().bottomRight().row() != (int)tor->getNumFiles() - 1;
        m_move_up->setEnabled(up_ok);
        m_move_top->setEnabled(up_ok);
        m_move_down->setEnabled(down_ok);
        m_move_bottom->setEnabled(down_ok);
    }
}

void DownloadOrderDialog::customOrderEnableToggled(bool on)
{
    m_search_files->setEnabled(on);
    m_sort_by->setEnabled(on);
    if (!on) {
        m_move_down->setEnabled(false);
        m_move_up->setEnabled(false);
        m_move_top->setEnabled(false);
        m_move_down->setEnabled(false);
    } else {
        itemSelectionChanged(m_order->selectionModel()->selection(), QItemSelection());
    }
}

void DownloadOrderDialog::search(const QString &text)
{
    if (text.isEmpty()) {
        model->clearHighLights();
    } else {
        QModelIndex idx = model->find(text);
        if (idx.isValid())
            m_order->scrollTo(idx);
    }
}
}

#include "moc_downloadorderdialog.cpp"
