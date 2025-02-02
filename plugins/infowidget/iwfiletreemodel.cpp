/*
    SPDX-FileCopyrightText: 2007 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2007 Ivan Vasic <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "iwfiletreemodel.h"

#include <cmath>

#include <KLocalizedString>
#include <QApplication>

#include "infowidgetpluginsettings.h"
#include <interfaces/functions.h>
#include <interfaces/torrentfileinterface.h>
#include <interfaces/torrentinterface.h>
#include <util/functions.h>

using namespace bt;

namespace kt
{
IWFileTreeModel::IWFileTreeModel(bt::TorrentInterface *tc, QObject *parent)
    : TorrentFileTreeModel(tc, KEEP_FILES, parent)
{
    mmfile = tc ? IsMultimediaFile(tc->getStats().output_path) : 0;
    preview = false;
    percentage = 0;

    if (root && tc) {
        BitSet d = tc->downloadedChunksBitSet();
        d -= tc->onlySeedChunksBitSet();
        root->initPercentage(tc, d);
    }
}

IWFileTreeModel::~IWFileTreeModel()
{
}

void IWFileTreeModel::changeTorrent(bt::TorrentInterface *tc)
{
    kt::TorrentFileTreeModel::changeTorrent(tc);
    mmfile = tc ? IsMultimediaFile(tc->getStats().output_path) : 0;
    preview = false;
    percentage = 0;

    if (root && tc) {
        BitSet d = tc->downloadedChunksBitSet();
        d -= tc->onlySeedChunksBitSet();
        root->initPercentage(tc, d);
    }
}

int IWFileTreeModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 5;
}

QVariant IWFileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    if (section < 2)
        return TorrentFileTreeModel::headerData(section, orientation, role);

    switch (section) {
    case 2:
        return i18n("Priority");
    case 3:
        return i18nc("@title:column", "Preview");
    // xgettext: no-c-format
    case 4:
        return i18nc("Percent of File Downloaded", "% Complete");
    default:
        return QVariant();
    }
}

static QString PriorityString(const bt::TorrentFileInterface *file)
{
    switch (file->getPriority()) {
    case FIRST_PREVIEW_PRIORITY:
    case FIRST_PRIORITY:
        return i18nc("Download first", "First");
    case LAST_PREVIEW_PRIORITY:
    case LAST_PRIORITY:
        return i18nc("Download last", "Last");
    case ONLY_SEED_PRIORITY:
    case EXCLUDED:
        return QString();
    case NORMAL_PREVIEW_PRIORITY:
    default:
        return i18nc("Download normally(not as first or last)", "Normal");
    }
}

QVariant IWFileTreeModel::data(const QModelIndex &index, int role) const
{
    Node *n = nullptr;
    if (index.column() < 2 && role != Qt::ForegroundRole)
        return TorrentFileTreeModel::data(index, role);

    if (!tc || !index.isValid() || !(n = (Node *)index.internalPointer()))
        return QVariant();

    if (role == Qt::ForegroundRole && index.column() == 2 && tc->getStats().multi_file_torrent && n->file) {
        const bt::TorrentFileInterface *file = n->file;
        switch (file->getPriority()) {
        case FIRST_PREVIEW_PRIORITY:
        case FIRST_PRIORITY:
            return InfoWidgetPluginSettings::firstColor();
        case LAST_PREVIEW_PRIORITY:
        case LAST_PRIORITY:
            return InfoWidgetPluginSettings::lastColor();
        case NORMAL_PREVIEW_PRIORITY:
        case NORMAL_PRIORITY:
            return QVariant();
        case ONLY_SEED_PRIORITY:
        case EXCLUDED:
        default:
            return QVariant();
        }
    }

    if (role == Qt::DisplayRole)
        return displayData(n, index);
    else if (role == Qt::UserRole)
        return sortData(n, index);

    return QVariant();
}

QVariant IWFileTreeModel::displayData(Node *n, const QModelIndex &index) const
{
    if (tc->getStats().multi_file_torrent && n->file) {
        const bt::TorrentFileInterface *file = n->file;
        switch (index.column()) {
        case 2:
            return PriorityString(file);
        case 3:
            if (file->isMultimedia()) {
                if (file->isPreviewAvailable())
                    return i18nc("preview available", "Available");
                else
                    return i18nc("Preview pending", "Pending");
            } else
                return i18nc("No preview available", "No");
        case 4:
            if (file->getPriority() == ONLY_SEED_PRIORITY || file->getPriority() == EXCLUDED)
                return QVariant();
            else
                return ki18n("%1 %").subs(n->percentage, 0, 'f', 2).toString();
        default:
            return QVariant();
        }
    } else if (!tc->getStats().multi_file_torrent) {
        switch (index.column()) {
        case 2:
            return QVariant();
        case 3:
            if (mmfile) {
                if (tc->readyForPreview())
                    return i18nc("Preview available", "Available");
                else
                    return i18nc("Preview pending", "Pending");
            } else
                return i18nc("No preview available", "No");
        case 4:
            return ki18n("%1 %").subs(bt::Percentage(tc->getStats()), 0, 'f', 2).toString();
        default:
            return QVariant();
        }
    } else if (tc->getStats().multi_file_torrent && index.column() == 4) {
        return ki18n("%1 %").subs(n->percentage, 0, 'f', 2).toString();
    }

    return QVariant();
}

QVariant IWFileTreeModel::sortData(Node *n, const QModelIndex &index) const
{
    if (tc->getStats().multi_file_torrent && n->file) {
        const bt::TorrentFileInterface *file = n->file;
        switch (index.column()) {
        case 2:
            return (int)file->getPriority();
        case 3:
            if (file->isMultimedia()) {
                if (file->isPreviewAvailable())
                    return 3;
                else
                    return 2;
            } else
                return 1;
        case 4:
            return n->percentage;
        }
    } else if (!tc->getStats().multi_file_torrent) {
        switch (index.column()) {
        case 2:
            return QVariant();
        case 3:
            if (mmfile) {
                if (tc->readyForPreview())
                    return 3;
                else
                    return 2;
            } else
                return 1;
        case 4:
            return bt::Percentage(tc->getStats());
        }
    } else if (tc->getStats().multi_file_torrent && index.column() == 4) {
        return n->percentage;
    }

    return QVariant();
}

void IWFileTreeModel::changePriority(const QModelIndexList &indexes, Priority newpriority)
{
    if (!tc)
        return;

    for (const QModelIndex &idx : indexes) {
        Node *n = (Node *)idx.internalPointer();
        if (n)
            setPriority(n, newpriority, true);
    }
}

void IWFileTreeModel::setPriority(TorrentFileTreeModel::Node *n, Priority newpriority, bool selected_node)
{
    if (!n->file) {
        for (int i = 0; i < n->children.count(); i++) {
            // recurse down the tree
            setPriority(n->children.at(i), newpriority, false);
        }

        Q_EMIT dataChanged(createIndex(n->row(), 0, n), createIndex(n->row(), 4, n));
    } else {
        bt::TorrentFileInterface *file = n->file;
        Priority old = file->getPriority();

        // When recursing down the tree don't reinclude files
        if ((old == EXCLUDED || old == ONLY_SEED_PRIORITY) && !selected_node)
            return;

        if (newpriority != old) {
            file->setPriority(newpriority);
            Q_EMIT dataChanged(createIndex(n->row(), 0, n), createIndex(n->row(), 4, n));
        }
    }
}

void IWFileTreeModel::filePercentageChanged(bt::TorrentFileInterface *file, float percentage)
{
    Q_UNUSED(percentage);
    if (tc)
        update(index(0, 0, QModelIndex()), file, 4);
}

void IWFileTreeModel::filePreviewChanged(bt::TorrentFileInterface *file, bool preview)
{
    Q_UNUSED(preview);
    if (tc)
        update(index(0, 0, QModelIndex()), file, 3);
}

void IWFileTreeModel::update(const QModelIndex &idx, bt::TorrentFileInterface *file, int col)
{
    if (!tc)
        return;

    Node *n = (Node *)idx.internalPointer();
    if (n->file && n->file == file) {
        QModelIndex i = createIndex(idx.row(), col, n);
        Q_EMIT dataChanged(i, i);
        if (col == 4) {
            // update percentages along the tree
            // this will go back up the tree and update the percentage of
            // all directories involved
            BitSet d = tc->downloadedChunksBitSet();
            d -= tc->onlySeedChunksBitSet();
            n->updatePercentage(d);

            // emit necessary signals
            QModelIndex parent = idx.parent();
            while (parent.isValid()) {
                Node *nd = (Node *)parent.internalPointer();
                i = createIndex(parent.row(), 4, nd);
                Q_EMIT dataChanged(i, i);
                parent = parent.parent();
            }
        }
    } else {
        for (int i = 0; i < n->children.count(); i++) {
            // recurse down the tree
            update(this->index(i, 0, idx), file, col);
        }
    }
}

void IWFileTreeModel::update()
{
    if (!tc)
        return;

    if (!tc->getStats().multi_file_torrent) {
        bool changed = false;
        bool np = mmfile && tc->readyForPreview();
        if (preview != np) {
            preview = np;
            changed = true;
        }

        double perc = bt::Percentage(tc->getStats());
        if (fabs(perc - percentage) > 0.001) {
            percentage = perc;
            changed = true;
        }

        if (changed)
            Q_EMIT dataChanged(createIndex(0, 2), createIndex(0, 4));
    }
}
}

#include "moc_iwfiletreemodel.cpp"
