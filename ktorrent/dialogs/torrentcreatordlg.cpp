/*
    SPDX-FileCopyrightText: 2007 Joris Guisson <joris.guisson@gmail.com>
    SPDX-FileCopyrightText: 2007 Ivan Vasic <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <ctime>

#include <QCompleter>

#include <KFileWidget>
#include <KMessageBox>
#include <KMessageBox_KTCompat>
#include <KRecentDirs>

#include "core.h"
#include "gui.h"
#include "torrentcreatordlg.h"
#include <dht/dht.h>
#include <dht/dhtbase.h>
#include <groups/group.h>
#include <groups/groupmanager.h>
#include <interfaces/functions.h>
#include <torrent/globals.h>
#include <util/error.h>
#include <util/log.h>
#include <util/stringcompletionmodel.h>

using namespace bt;

namespace kt
{
TorrentCreatorDlg::TorrentCreatorDlg(Core *core, GUI *gui, QWidget *parent)
    : QDialog(parent)
    , core(core)
    , gui(gui)
    , mktor(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose);
    tracker_completion = webseeds_completion = nodes_completion = nullptr;
    setWindowTitle(i18n("Create A Torrent"));
    setupUi(this);
    adjustSize();
    loadGroups();

    m_url->setMode(KFile::ExistingOnly | KFile::LocalOnly | KFile::Directory);
    m_selectDirectory->setChecked(true);

    m_dht_tab->setEnabled(false);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_selectDirectory, &QRadioButton::clicked, this, &TorrentCreatorDlg::selectDirectory);
    connect(m_selectFile, &QRadioButton::clicked, this, &TorrentCreatorDlg::selectFile);

    connect(m_dht, &QCheckBox::toggled, this, &TorrentCreatorDlg::dhtToggled);

    // tracker box stuff
    connect(m_add_tracker, &QPushButton::clicked, this, &TorrentCreatorDlg::addTrackerPressed);
    connect(m_tracker, &QLineEdit::returnPressed, this, &TorrentCreatorDlg::addTrackerPressed);
    connect(m_remove_tracker, &QPushButton::clicked, this, &TorrentCreatorDlg::removeTrackerPressed);
    connect(m_move_up, &QPushButton::clicked, this, &TorrentCreatorDlg::moveUpPressed);
    connect(m_move_down, &QPushButton::clicked, this, &TorrentCreatorDlg::moveDownPressed);
    connect(m_tracker, &QLineEdit::textChanged, this, &TorrentCreatorDlg::trackerTextChanged);
    connect(m_tracker_list, &QListWidget::itemSelectionChanged, this, &TorrentCreatorDlg::trackerSelectionChanged);
    m_add_tracker->setEnabled(false); // disable until there is text in m_tracker
    m_remove_tracker->setEnabled(false);
    m_move_up->setEnabled(false);
    m_move_down->setEnabled(false);

    // dht box
    connect(m_add_node, &QPushButton::clicked, this, &TorrentCreatorDlg::addNodePressed);
    connect(m_node, &QLineEdit::returnPressed, this, &TorrentCreatorDlg::addNodePressed);
    connect(m_remove_node, &QPushButton::clicked, this, &TorrentCreatorDlg::removeNodePressed);
    connect(m_node, &QLineEdit::textChanged, this, &TorrentCreatorDlg::nodeTextChanged);
    connect(m_node_list, &QTreeWidget::itemSelectionChanged, this, &TorrentCreatorDlg::nodeSelectionChanged);
    m_add_node->setEnabled(false);
    m_remove_node->setEnabled(false);

    // populate dht box with some nodes from our own table
    const QMap<QString, int> n = bt::Globals::instance().getDHT().getClosestGoodNodes(10);

    for (QMap<QString, int>::const_iterator it = n.cbegin(); it != n.cend(); ++it) {
        QTreeWidgetItem *twi = new QTreeWidgetItem(m_node_list);
        twi->setText(0, it.key());
        twi->setText(1, QString::number(it.value()));
        m_node_list->addTopLevelItem(twi);
    }

    // webseed stuff
    connect(m_add_webseed, &QPushButton::clicked, this, &TorrentCreatorDlg::addWebSeedPressed);
    connect(m_remove_webseed, &QPushButton::clicked, this, &TorrentCreatorDlg::removeWebSeedPressed);
    connect(m_webseed, &QLineEdit::textChanged, this, &TorrentCreatorDlg::webSeedTextChanged);
    connect(m_webseed_list, &QListWidget::itemSelectionChanged, this, &TorrentCreatorDlg::webSeedSelectionChanged);
    m_add_webseed->setEnabled(false);
    m_remove_webseed->setEnabled(false);

    connect(&update_timer, &QTimer::timeout, this, &TorrentCreatorDlg::updateProgressBar);
    loadCompleterData();
    m_progress->setValue(0);
}

TorrentCreatorDlg::~TorrentCreatorDlg()
{
    tracker_completion->save();
    webseeds_completion->save();
    nodes_completion->save();

    delete mktor;
}

void TorrentCreatorDlg::loadGroups()
{
    GroupManager *gman = core->getGroupManager();
    GroupManager::Itr it = gman->begin();

    QStringList grps;

    // First default group
    grps << i18n("All Torrents");

    // now custom ones
    while (it != gman->end()) {
        if (!it->second->isStandardGroup())
            grps << it->first;
        ++it;
    }

    m_group->addItems(grps);
}

void TorrentCreatorDlg::loadCompleterData()
{
    QString file = kt::DataDir() + QStringLiteral("torrent_creator_known_trackers");
    tracker_completion = new StringCompletionModel(file, this);
    tracker_completion->load();
    m_tracker->setCompleter(new QCompleter(tracker_completion, this));

    file = kt::DataDir() + QStringLiteral("torrent_creator_known_webseeds");
    webseeds_completion = new StringCompletionModel(file, this);
    webseeds_completion->load();
    m_webseed->setCompleter(new QCompleter(webseeds_completion, this));

    file = kt::DataDir() + QStringLiteral("torrent_creator_known_nodes");
    nodes_completion = new StringCompletionModel(file, this);
    nodes_completion->load();
    m_node->setCompleter(new QCompleter(nodes_completion, this));
}

void TorrentCreatorDlg::addTrackerPressed()
{
    if (m_tracker->text().length() > 0) {
        tracker_completion->addString(m_tracker->text());
        m_tracker_list->addItem(m_tracker->text());
        m_tracker->clear();
    }
}

void TorrentCreatorDlg::removeTrackerPressed()
{
    qDeleteAll(m_tracker_list->selectedItems());
}

void TorrentCreatorDlg::moveUpPressed()
{
    const QList<QListWidgetItem *> sel = m_tracker_list->selectedItems();
    for (QListWidgetItem *s : sel) {
        int r = m_tracker_list->row(s);
        if (r > 0) {
            m_tracker_list->insertItem(r - 1, m_tracker_list->takeItem(r));
            m_tracker_list->setCurrentRow(r - 1);
        }
    }
}

void TorrentCreatorDlg::moveDownPressed()
{
    const QList<QListWidgetItem *> sel = m_tracker_list->selectedItems();
    for (QListWidgetItem *s : sel) {
        int r = m_tracker_list->row(s);
        if (r + 1 < m_tracker_list->count()) {
            m_tracker_list->insertItem(r + 1, m_tracker_list->takeItem(r));
            m_tracker_list->setCurrentRow(r + 1);
        }
    }
}

void TorrentCreatorDlg::addNodePressed()
{
    if (m_node->text().length() > 0) {
        QTreeWidgetItem *twi = new QTreeWidgetItem(m_node_list);
        nodes_completion->addString(m_node->text());
        twi->setText(0, m_node->text());
        twi->setText(1, QString::number(m_port->value()));
        m_node_list->addTopLevelItem(twi);
        m_node->clear();
    }
}

void TorrentCreatorDlg::removeNodePressed()
{
    qDeleteAll(m_node_list->selectedItems());
}

void TorrentCreatorDlg::dhtToggled(bool on)
{
    m_private->setEnabled(!on);
    m_dht_tab->setEnabled(on);
    m_tracker_tab->setEnabled(!on);
}

void TorrentCreatorDlg::nodeTextChanged(const QString &str)
{
    m_add_node->setEnabled(str.length() > 0);
}

void TorrentCreatorDlg::nodeSelectionChanged()
{
    m_remove_node->setEnabled(m_node_list->selectedItems().count() > 0);
}

void TorrentCreatorDlg::trackerTextChanged(const QString &str)
{
    m_add_tracker->setEnabled(str.length() > 0);
}

void TorrentCreatorDlg::trackerSelectionChanged()
{
    bool enable_buttons = m_tracker_list->selectedItems().count() > 0;
    m_remove_tracker->setEnabled(enable_buttons);
    m_move_up->setEnabled(enable_buttons);
    m_move_down->setEnabled(enable_buttons);
}

void TorrentCreatorDlg::addWebSeedPressed()
{
    QUrl url(m_webseed->text());
    if (!url.isValid()) {
        KMessageBox::error(this, i18n("Invalid URL: %1", url.toDisplayString()));
        return;
    }

    if (url.scheme() != QLatin1String("http")) {
        KMessageBox::error(this, i18n("Only HTTP is supported for webseeding."));
        return;
    }

    webseeds_completion->addString(m_webseed->text());
    m_webseed_list->addItem(m_webseed->text());
    m_webseed->clear();
}

void TorrentCreatorDlg::removeWebSeedPressed()
{
    qDeleteAll(m_webseed_list->selectedItems());
}

void TorrentCreatorDlg::webSeedTextChanged(const QString &str)
{
    m_add_webseed->setEnabled(str.length() > 0);
}

void TorrentCreatorDlg::webSeedSelectionChanged()
{
    m_remove_webseed->setEnabled(m_webseed_list->selectedItems().count() > 0);
}

void TorrentCreatorDlg::accept()
{
    if (!m_url->url().isValid()) {
        gui->errorMsg(i18n("You must select a file or a folder."));
        return;
    }

    if (m_tracker_list->count() == 0 && !m_dht->isChecked()) {
        QString msg = i18n(
            "You have not added a tracker, "
            "are you sure you want to create this torrent?");
        if (KMessageBox::warningTwoActions(gui, msg, QString(), KGuiItem(i18nc("@action:button", "Create")), KStandardGuiItem::cancel())
            == KMessageBox::SecondaryAction)
            return;
    }

    if (m_node_list->topLevelItemCount() == 0 && m_dht->isChecked()) {
        gui->errorMsg(i18n("You must add at least one node."));
        return;
    }

    QUrl url = m_url->url();
    Uint32 chunk_size_table[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};

    int chunk_size = chunk_size_table[m_chunk_size->currentIndex()];
    QString name = url.toLocalFile();
    name = QFileInfo(name).isDir() ? QDir(name).dirName() : url.fileName();

    QStringList trackers;

    if (m_dht->isChecked()) {
        for (int i = 0; i < m_node_list->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_node_list->topLevelItem(i);
            trackers.append(item->text(0) + QLatin1Char(',') + item->text(1));
        }
    } else {
        for (int i = 0; i < m_tracker_list->count(); ++i) {
            QListWidgetItem *item = m_tracker_list->item(i);
            trackers.append(item->text());
        }
    }

    QList<QUrl> webseeds;
    for (int i = 0; i < m_webseed_list->count(); ++i) {
        QListWidgetItem *item = m_webseed_list->item(i);
        webseeds.append(QUrl(item->text()));
    }

    try {
        mktor = new bt::TorrentCreator(url.toLocalFile(), trackers, webseeds, chunk_size, name, m_comments->text(), m_private->isChecked(), m_dht->isChecked());

        connect(mktor, &bt::TorrentCreator::finished, this, &TorrentCreatorDlg::hashCalculationDone, Qt::QueuedConnection);
        mktor->start();
        setProgressBarEnabled(true);
        update_timer.start(1000);
        m_progress->setMaximum(mktor->getNumChunks());
    } catch (bt::Error &err) {
        delete mktor;
        mktor = nullptr;
        Out(SYS_GEN | LOG_IMPORTANT) << "Error: " << err.toString() << endl;
        gui->errorMsg(err.toString());
    }
}

void TorrentCreatorDlg::hashCalculationDone()
{
    setProgressBarEnabled(false);
    update_timer.stop();

    QString recentDirClass;
    QString s = QFileDialog::getSaveFileName(this,
                                             i18n("Choose a file to save the torrent"),
                                             KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///openTorrent")), recentDirClass).toLocalFile(),
                                             kt::TorrentFileFilter(false));

    if (s.isEmpty()) {
        QDialog::reject();
        return;
    }

    if (!recentDirClass.isEmpty())
        KRecentDirs::add(recentDirClass, QFileInfo(s).absolutePath());

    if (!s.endsWith(QLatin1String(".torrent")))
        s += QLatin1String(".torrent");

    mktor->saveTorrent(s);
    bt::TorrentInterface *tc = core->createTorrent(mktor, m_start_seeding->isChecked());
    if (m_group->currentIndex() > 0 && tc) {
        QString groupName = m_group->currentText();

        GroupManager *gman = core->getGroupManager();
        Group *group = gman->find(groupName);
        if (group) {
            group->addTorrent(tc, true);
            gman->saveGroups();
        }
    }

    QDialog::accept();
}

void TorrentCreatorDlg::reject()
{
    if (mktor && mktor->isRunning()) {
        disconnect(mktor, &bt::TorrentCreator::finished, this, &TorrentCreatorDlg::hashCalculationDone);
        mktor->stop();
        mktor->wait();
    }
    QDialog::reject();
}

void TorrentCreatorDlg::setProgressBarEnabled(bool on)
{
    m_progress->setEnabled(on);
    m_options->setEnabled(!on);
    m_url->setEnabled(!on);
    m_tabs->setEnabled(!on);
    m_comments->setEnabled(!on);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!on);
}

void TorrentCreatorDlg::updateProgressBar()
{
    m_progress->setValue(mktor->getCurrentChunk());
}

void TorrentCreatorDlg::selectFile()
{
    m_url->setMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly);
}

void TorrentCreatorDlg::selectDirectory()
{
    m_url->setMode(KFile::ExistingOnly | KFile::LocalOnly | KFile::Directory);
}

}

#include "moc_torrentcreatordlg.cpp"
