/*
    SPDX-FileCopyrightText: 2007 Joris Guisson <joris.guisson@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QHeaderView>
#include <QPushButton>
#include <QSortFilterProxyModel>

#include <KLocalizedString>

#include "core.h"
#include "speedlimitsdlg.h"
#include "speedlimitsmodel.h"
#include "spinboxdelegate.h"
#include <interfaces/functions.h>
#include <interfaces/torrentinterface.h>
#include <settings.h>
#include <torrent/queuemanager.h>
#include <util/constants.h>
#include <util/log.h>

using namespace bt;

namespace kt
{
SpeedLimitsDlg::SpeedLimitsDlg(bt::TorrentInterface *current, Core *core, QWidget *parent)
    : QDialog(parent)
    , core(core)
    , current(current)
{
    setupUi(this);
    setWindowIcon(QIcon::fromTheme(QStringLiteral("kt-speed-limits")));
    setWindowTitle(i18n("Speed Limits"));

    model = new SpeedLimitsModel(core, this);
    QSortFilterProxyModel *pm = new QSortFilterProxyModel(this);
    pm->setSourceModel(model);
    pm->setSortRole(Qt::UserRole);

    m_speed_limits_view->setModel(pm);
    m_speed_limits_view->setItemDelegate(new SpinBoxDelegate(this));
    m_speed_limits_view->setUniformRowHeights(true);
    m_speed_limits_view->setSortingEnabled(true);
    m_speed_limits_view->sortByColumn(0, Qt::AscendingOrder);
    m_speed_limits_view->header()->setSortIndicatorShown(true);
    m_speed_limits_view->header()->setSectionsClickable(true);
    m_speed_limits_view->setAlternatingRowColors(true);

    QPushButton *apply_btn = m_buttonBox->button(QDialogButtonBox::Apply);
    apply_btn->setEnabled(false);
    connect(model, &SpeedLimitsModel::enableApply, apply_btn, &QPushButton::setEnabled);
    connect(apply_btn, &QPushButton::clicked, this, &SpeedLimitsDlg::apply);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_upload_rate->setValue(Settings::maxUploadRate());
    m_download_rate->setValue(Settings::maxDownloadRate());
    connect(m_upload_rate, &QSpinBox::valueChanged, this, &SpeedLimitsDlg::spinBoxValueChanged);
    connect(m_download_rate, &QSpinBox::valueChanged, this, &SpeedLimitsDlg::spinBoxValueChanged);
    connect(m_filter, &QLineEdit::textChanged, pm, &QSortFilterProxyModel::setFilterFixedString);
    loadState();

    // if current is specified, select it and scroll to it
    if (current) {
        kt::QueueManager *qman = core->getQueueManager();
        int idx = 0;
        QList<bt::TorrentInterface *>::iterator itr = qman->begin();
        while (itr != qman->end()) {
            if (*itr == current)
                break;

            idx++;
            itr++;
        }

        if (itr != qman->end()) {
            QItemSelectionModel *sel = m_speed_limits_view->selectionModel();
            QModelIndex midx = pm->mapFromSource(model->index(idx, 0));
            QModelIndex midx2 = pm->mapFromSource(model->index(idx, 4));
            sel->select(QItemSelection(midx, midx2), QItemSelectionModel::Select);
            m_speed_limits_view->scrollTo(midx);
        }
    }
}

SpeedLimitsDlg::~SpeedLimitsDlg()
{
}

void SpeedLimitsDlg::saveState()
{
    KConfigGroup g = KSharedConfig::openConfig()->group("SpeedLimitsDlg");
    QByteArray s = m_speed_limits_view->header()->saveState();
    g.writeEntry("view_state", s.toBase64());
    g.writeEntry("size", size());
}

void SpeedLimitsDlg::loadState()
{
    KConfigGroup g = KSharedConfig::openConfig()->group("SpeedLimitsDlg");
    QByteArray s = QByteArray::fromBase64(g.readEntry("view_state", QByteArray()));
    if (!s.isEmpty()) {
        m_speed_limits_view->header()->restoreState(s);
        m_speed_limits_view->header()->setSortIndicatorShown(true);
        m_speed_limits_view->header()->setSectionsClickable(true);
    }

    QSize ws = g.readEntry("size", size());
    resize(ws);
}

void SpeedLimitsDlg::accept()
{
    apply();
    saveState();
    QDialog::accept();
}

void SpeedLimitsDlg::reject()
{
    saveState();
    QDialog::reject();
}

void SpeedLimitsDlg::apply()
{
    model->apply();
    m_buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    bool apply = false;
    if (Settings::maxUploadRate() != m_upload_rate->value()) {
        Settings::setMaxUploadRate(m_upload_rate->value());
        apply = true;
    }

    if (Settings::maxDownloadRate() != m_download_rate->value()) {
        Settings::setMaxDownloadRate(m_download_rate->value());
        apply = true;
    }

    if (apply) {
        kt::ApplySettings();
        Settings::self()->save();
    }
}

void SpeedLimitsDlg::spinBoxValueChanged(int)
{
    m_buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

}

#include "moc_speedlimitsdlg.cpp"
