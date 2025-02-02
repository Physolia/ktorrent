/*
    SPDX-FileCopyrightText: 2009 Joris Guisson <joris.guisson@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KLocalizedString>
#include <QComboBox>

#include "shutdowntorrentmodel.h"
#include <interfaces/coreinterface.h>
#include <torrent/queuemanager.h>

namespace kt
{
ShutdownTorrentModel::ShutdownTorrentModel(CoreInterface *core, QObject *parent)
    : QAbstractTableModel(parent)
    , qman(core->getQueueManager())
{
    for (kt::QueueManager::iterator i = qman->begin(); i != qman->end(); i++) {
        TriggerItem cond;
        cond.checked = false;
        cond.tc = *i;
        cond.trigger = DOWNLOADING_COMPLETED;
        conds.append(cond);
    }

    connect(core, &CoreInterface::torrentAdded, this, &ShutdownTorrentModel::torrentAdded);
    connect(core, &CoreInterface::torrentRemoved, this, &ShutdownTorrentModel::torrentRemoved);
}

ShutdownTorrentModel::~ShutdownTorrentModel()
{
}

void ShutdownTorrentModel::torrentAdded(bt::TorrentInterface *tc)
{
    TriggerItem cond;
    cond.checked = false;
    cond.tc = tc;
    cond.trigger = DOWNLOADING_COMPLETED;
    conds.append(cond);
    insertRow(conds.count() - 1);
}

void ShutdownTorrentModel::torrentRemoved(bt::TorrentInterface *tc)
{
    int idx = 0;
    for (const TriggerItem &c : qAsConst(conds)) {
        if (c.tc == tc) {
            removeRow(idx);
            break;
        }
        idx++;
    }
}

QVariant ShutdownTorrentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= conds.count())
        return QVariant();

    if (role == Qt::CheckStateRole) {
        if (index.column() != 0)
            return QVariant();

        return conds.at(index.row()).checked ? Qt::Checked : Qt::Unchecked;
    } else if (role == Qt::DisplayRole) {
        const TriggerItem &cond = conds.at(index.row());
        switch (index.column()) {
        case 0:
            return cond.tc->getDisplayName();
        case 1:
            if (cond.trigger == DOWNLOADING_COMPLETED)
                return i18n("Downloading finishes");
            else
                return i18n("Seeding finishes");
        default:
            return QVariant();
        }
    } else if (role == Qt::EditRole) {
        if (index.column() == 1)
            return conds.at(index.row()).trigger;
    }

    return QVariant();
}

int ShutdownTorrentModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

int ShutdownTorrentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : qman->count();
}

bool ShutdownTorrentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= conds.count())
        return false;

    if (role == Qt::CheckStateRole) {
        TriggerItem &cond = conds[index.row()];
        Qt::CheckState checked = static_cast<Qt::CheckState>(value.toInt());
        cond.checked = checked == Qt::Checked;
        Q_EMIT dataChanged(index, index);
        return true;
    } else if (role == Qt::EditRole) {
        int v = value.toInt();
        if (v < 0 || v > 1)
            return false;

        Trigger trigger = (Trigger)v;
        TriggerItem &cond = conds[index.row()];
        cond.trigger = trigger;
        Q_EMIT dataChanged(index, index);
        return true;
    }
    return false;
}

QVariant ShutdownTorrentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
    case 0:
        return i18n("Torrent");
    case 1:
        return i18n("Event");
    default:
        return QVariant();
    }
}

bool ShutdownTorrentModel::insertRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    beginInsertRows(QModelIndex(), row, row + count - 1);
    endInsertRows();
    return true;
}

bool ShutdownTorrentModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for (int i = 0; i < count; i++) {
        conds.takeAt(row);
    }
    endRemoveRows();
    return true;
}

Qt::ItemFlags ShutdownTorrentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= conds.count())
        return {};

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == 0)
        flags |= Qt::ItemIsUserCheckable;

    if (index.column() == 1)
        flags |= Qt::ItemIsEditable;

    return flags;
}

void ShutdownTorrentModel::applyRules(Action action, kt::ShutdownRuleSet *rules)
{
    rules->clear();
    for (const TriggerItem &c : qAsConst(conds)) {
        if (c.checked)
            rules->addRule(action, SPECIFIC_TORRENT, c.trigger, c.tc);
    }
}

void ShutdownTorrentModel::addRule(const kt::ShutdownRule &rule)
{
    QList<TriggerItem>::iterator i = conds.begin();
    while (i != conds.end()) {
        TriggerItem &c = *i;
        if (c.tc == rule.tc) {
            c.checked = true;
            c.trigger = rule.trigger;
            break;
        }
        i++;
    }
}

//////////////////////////////////////////////////////

ShutdownTorrentDelegate::ShutdownTorrentDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

ShutdownTorrentDelegate::~ShutdownTorrentDelegate()
{
}

QWidget *ShutdownTorrentDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    QComboBox *cb = new QComboBox(parent);
    cb->addItem(i18n("Downloading finishes"));
    cb->addItem(i18n("Seeding finishes"));
    return cb;
}

void ShutdownTorrentDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    QComboBox *cb = static_cast<QComboBox *>(editor);
    cb->setCurrentIndex(value);
}

void ShutdownTorrentDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *cb = static_cast<QComboBox *>(editor);
    int value = cb->currentIndex();
    model->setData(index, value, Qt::EditRole);
}

QSize ShutdownTorrentDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    QComboBox tmp;
    return tmp.sizeHint();
}

void ShutdownTorrentDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    QRect r = option.rect;
    if (option.rect.height() < editor->sizeHint().height())
        r.setHeight(editor->sizeHint().height());
    editor->setGeometry(r);
}

}

#include "moc_shutdowntorrentmodel.cpp"
