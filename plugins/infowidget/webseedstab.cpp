/*
    SPDX-FileCopyrightText: 2008 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2008 Ivan Vasic <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "webseedstab.h"

#include <KMessageBox>
#include <QHeaderView>
#include <QStyle>

#include "webseedsmodel.h"
#include <interfaces/webseedinterface.h>

using namespace bt;
using namespace Qt::Literals::StringLiterals;

namespace kt
{
WebSeedsTab::WebSeedsTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    m_gridLayout->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                     style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                     style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                     0);
    connect(m_add, &QPushButton::clicked, this, &WebSeedsTab::addWebSeed);
    connect(m_remove, &QPushButton::clicked, this, &WebSeedsTab::removeWebSeed);
    connect(m_disable_all, &QPushButton::clicked, this, &WebSeedsTab::disableAll);
    connect(m_enable_all, &QPushButton::clicked, this, &WebSeedsTab::enableAll);
    m_add->setIcon(QIcon::fromTheme(QLatin1String("list-add")));
    m_remove->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
    m_add->setEnabled(false);
    m_remove->setEnabled(false);
    m_webseed_list->setEnabled(false);
    model = new WebSeedsModel(this);
    proxy_model = new QSortFilterProxyModel(this);
    proxy_model->setSourceModel(model);
    proxy_model->setSortRole(Qt::UserRole);
    m_webseed_list->setModel(proxy_model);
    m_webseed_list->setSortingEnabled(true);
    m_webseed_list->setUniformRowHeights(true);

    connect(m_webseed_list->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            qOverload<const QItemSelection &, const QItemSelection &>(&WebSeedsTab::selectionChanged));

    connect(m_webseed, &QLineEdit::textChanged, this, &WebSeedsTab::onWebSeedTextChanged);
}

WebSeedsTab::~WebSeedsTab()
{
}

void WebSeedsTab::changeTC(bt::TorrentInterface *tc)
{
    curr_tc = tc;
    model->changeTC(tc);
    m_add->setEnabled(tc != nullptr);
    m_remove->setEnabled(tc != nullptr);
    m_webseed_list->setEnabled(tc != nullptr);
    m_webseed->setEnabled(tc != nullptr);
    m_enable_all->setEnabled(tc != nullptr);
    m_disable_all->setEnabled(tc != nullptr);
    onWebSeedTextChanged(m_webseed->text());

    // see if we need to enable or disable the remove button
    if (curr_tc)
        selectionChanged(m_webseed_list->selectionModel()->selectedRows());
}

void WebSeedsTab::addWebSeed()
{
    if (!curr_tc)
        return;

    bt::TorrentInterface *tc = curr_tc.data();
    QUrl url(m_webseed->text());
    if (tc && url.isValid() && (url.scheme() == "http"_L1 || url.scheme() == "https"_L1)) {
        if (tc->addWebSeed(url)) {
            model->changeTC(tc);
            m_webseed->clear();
        } else {
            KMessageBox::error(this, i18n("Cannot add the webseed %1, it is already part of the list of webseeds.", url.toDisplayString()));
        }
    }
}

void WebSeedsTab::removeWebSeed()
{
    if (!curr_tc)
        return;

    bt::TorrentInterface *tc = curr_tc.data();
    const QModelIndexList idx_list = m_webseed_list->selectionModel()->selectedRows();
    for (const QModelIndex &idx : idx_list) {
        const WebSeedInterface *ws = tc->getWebSeed(proxy_model->mapToSource(idx).row());
        if (ws && ws->isUserCreated()) {
            if (!tc->removeWebSeed(ws->getUrl()))
                KMessageBox::error(this, i18n("Cannot remove webseed %1, it is part of the torrent.", ws->getUrl().toDisplayString()));
        }
    }

    model->changeTC(tc);
}

void WebSeedsTab::selectionChanged(const QModelIndexList &indexes)
{
    if (curr_tc) {
        for (const QModelIndex &idx : indexes) {
            const WebSeedInterface *ws = curr_tc.data()->getWebSeed(proxy_model->mapToSource(idx).row());
            if (ws && ws->isUserCreated()) {
                m_remove->setEnabled(true);
                return;
            }
        }
    }

    m_remove->setEnabled(false);
}

void WebSeedsTab::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    if (!curr_tc)
        return;

    selectionChanged(selected.indexes());
}

void WebSeedsTab::onWebSeedTextChanged(const QString &ws)
{
    QUrl url(ws);
    m_add->setEnabled(!curr_tc.isNull() && url.isValid() && (url.scheme() == "http"_L1 || url.scheme() == "https"_L1));
}

void WebSeedsTab::update()
{
    if (model->update())
        proxy_model->invalidate();
}

void WebSeedsTab::saveState(KSharedConfigPtr cfg)
{
    KConfigGroup g = cfg->group(QStringLiteral("WebSeedsTab"));
    QByteArray s = m_webseed_list->header()->saveState();
    g.writeEntry("state", s.toBase64());
}

void WebSeedsTab::loadState(KSharedConfigPtr cfg)
{
    KConfigGroup g = cfg->group(QStringLiteral("WebSeedsTab"));
    QByteArray s = QByteArray::fromBase64(g.readEntry("state", QByteArray()));
    if (!s.isEmpty())
        m_webseed_list->header()->restoreState(s);
}

void WebSeedsTab::disableAll()
{
    for (int i = 0; i < model->rowCount(); i++) {
        model->setData(model->index(i, 0), Qt::Unchecked, Qt::CheckStateRole);
    }
}

void WebSeedsTab::enableAll()
{
    for (int i = 0; i < model->rowCount(); i++) {
        model->setData(model->index(i, 0), Qt::Checked, Qt::CheckStateRole);
    }
}

}

#include "moc_webseedstab.cpp"
