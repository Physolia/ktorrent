/*
    SPDX-FileCopyrightText: 2008 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2008 Ivan Vasic <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFileInfo>
#include <QIcon>
#include <QMimeData>

#include <KLocalizedString>

#include "mediamodel.h"
#include <interfaces/coreinterface.h>
#include <interfaces/torrentfileinterface.h>
#include <interfaces/torrentinterface.h>
#include <torrent/queuemanager.h>
#include <util/constants.h>
#include <util/functions.h>
#include <util/log.h>

using namespace bt;

namespace kt
{
MediaModel::MediaModel(CoreInterface *core, QObject *parent)
    : QAbstractListModel(parent)
    , core(core)
{
    const QueueManager *const qman = core->getQueueManager();
    for (bt::TorrentInterface *tc : *qman) {
        onTorrentAdded(tc);
    }
}

MediaModel::~MediaModel()
{
}

int MediaModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return items.count();
    else
        return 0;
}

int MediaModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant MediaModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section);
    Q_UNUSED(orientation);
    Q_UNUSED(role);
    return QVariant();
}

QVariant MediaModel::data(const QModelIndex &index, int role) const
{
    if (index.column() != 0 || index.row() < 0 || index.row() >= items.count())
        return QVariant();

    MediaFile::Ptr mf = items.at(index.row());
    switch (role) {
    case Qt::ToolTipRole: {
        QString preview = mf->previewAvailable() ? i18n("Available") : i18n("Pending");
        return i18n("<b>%1</b><br/>Preview: %2<br/>Downloaded: %3 %", mf->name(), preview, mf->downloadPercentage());
    } break;
    case Qt::DisplayRole:
        return mf->name();
    case Qt::DecorationRole:
        return QIcon::fromTheme(m_mimeDatabase.mimeTypeForFile(mf->path()).iconName());
    case Qt::UserRole: // user role is for finding out if a torrent is complete
        return mf->fullyAvailable();
    case Qt::UserRole + 1:
        return QFileInfo(mf->path()).lastModified().toSecsSinceEpoch();
    default:
        return QVariant();
    }

    return QVariant();
}

bool MediaModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for (int i = 0; i < count; i++) {
        if (row >= 0 && row < items.count()) {
            items.removeAt(row);
        }
    }
    endRemoveRows();
    return true;
}

bool MediaModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid())
        return false;

    beginInsertRows(QModelIndex(), row, row + count - 1);
    endInsertRows();
    return true;
}

void MediaModel::onTorrentAdded(bt::TorrentInterface *tc)
{
    if (tc->getStats().multi_file_torrent) {
        int cnt = 0;
        for (Uint32 i = 0; i < tc->getNumFiles(); i++) {
            if (tc->getTorrentFile(i).isMultimedia()) {
                MediaFile::Ptr p(new MediaFile(tc, i));
                items.append(p);
                cnt++;
            }
        }

        if (cnt)
            insertRows(items.count() - 1, cnt, QModelIndex());
    } else if (tc->isMultimedia()) {
        MediaFile::Ptr p(new MediaFile(tc));
        items.append(p);
        insertRow(items.count() - 1);
    }
}

void MediaModel::onTorrentRemoved(bt::TorrentInterface *tc)
{
    int row = 0;
    int start = -1;
    int cnt = 0;
    for (MediaFile::Ptr mf : qAsConst(items)) {
        if (mf->torrent() == tc) {
            if (start == -1) {
                // start of the range
                start = row;
                cnt = 1;
            } else
                cnt++; // Still in the middle of the media files of this torrent
        } else if (start != -1) {
            // We have found the end
            break;
        }

        row++;
    }

    if (cnt > 0)
        removeRows(start, cnt, QModelIndex());
}

MediaFileRef MediaModel::fileForIndex(const QModelIndex &idx) const
{
    if (idx.row() < 0 || idx.row() >= items.count())
        return MediaFileRef(QString());
    else
        return MediaFileRef(items.at(idx.row()));
}

QModelIndex MediaModel::indexForPath(const QString &path) const
{
    Uint32 idx = 0;
    for (MediaFile::Ptr mf : qAsConst(items)) {
        if (mf->path() == path)
            return index(idx, 0, QModelIndex());
        idx++;
    }

    return QModelIndex();
}

MediaFileRef MediaModel::find(const QString &path)
{
    for (MediaFile::Ptr mf : qAsConst(items)) {
        if (mf->path() == path)
            return MediaFileRef(mf);
    }

    return MediaFileRef(path);
}

Qt::ItemFlags MediaModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    if (index.isValid())
        return Qt::ItemIsDragEnabled | defaultFlags;
    else
        return defaultFlags;
}

QStringList MediaModel::mimeTypes() const
{
    QStringList types;
    types << QStringLiteral("text/uri-list");
    return types;
}

QMimeData *MediaModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *data = new QMimeData();
    QList<QUrl> urls;
    for (const QModelIndex &idx : indexes) {
        if (!idx.isValid() || idx.row() < 0 || idx.row() >= items.count())
            continue;

        MediaFile::Ptr p = items.at(idx.row());
        urls << QUrl::fromLocalFile(p->path());
    }
    data->setUrls(urls);
    return data;
}

QModelIndex MediaModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || row >= items.count() || column != 0 || parent.isValid())
        return QModelIndex();

    return createIndex(row, column);
}

}

#include "moc_mediamodel.cpp"
