/*
    SPDX-FileCopyrightText: 2005-2007 Joris Guisson <joris.guisson@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "infowidgetplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <interfaces/coreinterface.h>
#include <interfaces/guiinterface.h>
#include <interfaces/torrentinterface.h>
#include <settings.h>
#include <util/log.h>
#include <util/logsystemmanager.h>

#include "chunkdownloadview.h"
#include "fileview.h"
#include "infowidgetpluginsettings.h"
#include "iwprefpage.h"
#include "monitor.h"
#include "peerview.h"
#include "statustab.h"
#include "trackerview.h"
#include "webseedstab.h"

K_PLUGIN_CLASS_WITH_JSON(kt::InfoWidgetPlugin, "ktorrent_infowidget.json")

using namespace bt;

namespace kt
{
InfoWidgetPlugin::InfoWidgetPlugin(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plugin(parent, data, args)
{
}

InfoWidgetPlugin::~InfoWidgetPlugin()
{
}

void InfoWidgetPlugin::load()
{
    LogSystemManager::instance().registerSystem(i18n("Info Widget"), SYS_INW);
    connect(getCore(), &CoreInterface::settingsChanged, this, &InfoWidgetPlugin::applySettings);

    status_tab = new StatusTab(nullptr);
    file_view = new FileView(nullptr);
    file_view->loadState(KSharedConfig::openConfig());
    connect(getCore(), &CoreInterface::torrentRemoved, this, &InfoWidgetPlugin::torrentRemoved);

    pref = new IWPrefPage(nullptr);
    TorrentActivityInterface *ta = getGUI()->getTorrentActivity();
    ta->addViewListener(this);
    ta->addToolWidget(status_tab, i18nc("@title:tab", "Status"), QStringLiteral("dialog-information"), i18n("Displays status information about a torrent"));
    ta->addToolWidget(file_view, i18nc("@title:tab", "Files"), QStringLiteral("folder"), i18n("Shows all the files in a torrent"));

    applySettings();

    getGUI()->addPrefPage(pref);
    currentTorrentChanged(const_cast<bt::TorrentInterface *>(ta->getCurrentTorrent()));
}

void InfoWidgetPlugin::unload()
{
    LogSystemManager::instance().unregisterSystem(i18n("Bandwidth Scheduler"));
    disconnect(getCore(), &CoreInterface::settingsChanged, this, &InfoWidgetPlugin::applySettings);
    disconnect(getCore(), &CoreInterface::torrentRemoved, this, &InfoWidgetPlugin::torrentRemoved);
    if (cd_view)
        cd_view->saveState(KSharedConfig::openConfig());
    if (peer_view)
        peer_view->saveState(KSharedConfig::openConfig());
    if (file_view)
        file_view->saveState(KSharedConfig::openConfig());
    if (webseeds_tab)
        webseeds_tab->saveState(KSharedConfig::openConfig());
    if (tracker_view)
        tracker_view->saveState(KSharedConfig::openConfig());
    KSharedConfig::openConfig()->sync();

    TorrentActivityInterface *ta = getGUI()->getTorrentActivity();
    ta->removeViewListener(this);
    getGUI()->removePrefPage(pref);
    ta->removeToolWidget(status_tab);
    ta->removeToolWidget(file_view);
    if (cd_view)
        ta->removeToolWidget(cd_view);
    if (tracker_view)
        ta->removeToolWidget(tracker_view);
    if (peer_view)
        ta->removeToolWidget(peer_view);
    if (webseeds_tab)
        ta->removeToolWidget(webseeds_tab);

    delete monitor;
    monitor = nullptr;
    delete status_tab;
    status_tab = nullptr;
    delete file_view;
    file_view = nullptr;
    delete cd_view;
    cd_view = nullptr;
    delete peer_view;
    peer_view = nullptr;
    delete tracker_view;
    tracker_view = nullptr;
    delete webseeds_tab;
    webseeds_tab = nullptr;
    delete pref;
    pref = nullptr;
}

void InfoWidgetPlugin::guiUpdate()
{
    if (status_tab && status_tab->isVisible())
        status_tab->update();

    if (file_view && file_view->isVisible())
        file_view->update();

    if (peer_view && peer_view->isVisible())
        peer_view->update();

    if (cd_view && cd_view->isVisible())
        cd_view->update();

    if (tracker_view && tracker_view->isVisible())
        tracker_view->update();

    if (webseeds_tab && webseeds_tab->isVisible())
        webseeds_tab->update();
}

void InfoWidgetPlugin::currentTorrentChanged(bt::TorrentInterface *tc)
{
    if (status_tab)
        status_tab->changeTC(tc);
    if (file_view)
        file_view->changeTC(tc);
    if (cd_view)
        cd_view->changeTC(tc);
    if (tracker_view)
        tracker_view->changeTC(tc);
    if (webseeds_tab)
        webseeds_tab->changeTC(tc);

    if (peer_view)
        peer_view->setEnabled(tc != nullptr);

    createMonitor(tc);
}

void InfoWidgetPlugin::applySettings()
{
    // if the colors are invalid, set the default colors
    bool save = false;
    if (!InfoWidgetPluginSettings::firstColor().isValid()) {
        save = true;
        InfoWidgetPluginSettings::setFirstColor(Qt::green);
    }

    if (!InfoWidgetPluginSettings::lastColor().isValid()) {
        save = true;
        InfoWidgetPluginSettings::setLastColor(Qt::red);
    }

    if (save)
        InfoWidgetPluginSettings::self()->save();

    showWebSeedsTab(InfoWidgetPluginSettings::showWebSeedsTab());
    showPeerView(InfoWidgetPluginSettings::showPeerView());
    showChunkView(InfoWidgetPluginSettings::showChunkView());
    showTrackerView(InfoWidgetPluginSettings::showTrackersView());
}

void InfoWidgetPlugin::showPeerView(bool show)
{
    TorrentActivityInterface *ta = getGUI()->getTorrentActivity();
    bt::TorrentInterface *tc = ta->getCurrentTorrent();

    if (show && !peer_view) {
        peer_view = new PeerView(nullptr);
        ta->addToolWidget(peer_view, i18n("Peers"), QStringLiteral("system-users"), i18n("Displays all the peers you are connected to for a torrent"));
        peer_view->loadState(KSharedConfig::openConfig());
        createMonitor(tc);
    } else if (!show && peer_view) {
        peer_view->saveState(KSharedConfig::openConfig());
        ta->removeToolWidget(peer_view);
        delete peer_view;
        peer_view = nullptr;
        createMonitor(tc);
    }
}

void InfoWidgetPlugin::showChunkView(bool show)
{
    TorrentActivityInterface *ta = getGUI()->getTorrentActivity();
    bt::TorrentInterface *tc = ta->getCurrentTorrent();

    if (show && !cd_view) {
        cd_view = new ChunkDownloadView(nullptr);
        ta->addToolWidget(cd_view, i18n("Chunks"), QStringLiteral("kt-chunks"), i18n("Displays all the chunks you are downloading, of a torrent"));

        cd_view->loadState(KSharedConfig::openConfig());
        cd_view->changeTC(tc);
        createMonitor(tc);
    } else if (!show && cd_view) {
        cd_view->saveState(KSharedConfig::openConfig());
        ta->removeToolWidget(cd_view);
        delete cd_view;
        cd_view = nullptr;
        createMonitor(tc);
    }
}

void InfoWidgetPlugin::showTrackerView(bool show)
{
    TorrentActivityInterface *ta = getGUI()->getTorrentActivity();
    if (show && !tracker_view) {
        tracker_view = new TrackerView(nullptr);
        ta->addToolWidget(tracker_view, i18n("Trackers"), QStringLiteral("network-server"), i18n("Displays information about all the trackers of a torrent"));
        tracker_view->loadState(KSharedConfig::openConfig());
        tracker_view->changeTC(ta->getCurrentTorrent());
    } else if (!show && tracker_view) {
        tracker_view->saveState(KSharedConfig::openConfig());
        ta->removeToolWidget(tracker_view);
        delete tracker_view;
        tracker_view = nullptr;
    }
}

void InfoWidgetPlugin::showWebSeedsTab(bool show)
{
    TorrentActivityInterface *ta = getGUI()->getTorrentActivity();
    if (show && !webseeds_tab) {
        webseeds_tab = new WebSeedsTab(nullptr);
        ta->addToolWidget(webseeds_tab, i18n("Webseeds"), QStringLiteral("network-server"), i18n("Displays all the webseeds of a torrent"));
        webseeds_tab->loadState(KSharedConfig::openConfig());
        webseeds_tab->changeTC(ta->getCurrentTorrent());
    } else if (!show && webseeds_tab) {
        webseeds_tab->saveState(KSharedConfig::openConfig());
        ta->removeToolWidget(webseeds_tab);
        delete webseeds_tab;
        webseeds_tab = nullptr;
    }
}

void InfoWidgetPlugin::createMonitor(bt::TorrentInterface *tc)
{
    delete monitor;
    monitor = nullptr;

    if (peer_view)
        peer_view->removeAll();
    if (cd_view)
        cd_view->removeAll();

    if (tc && (peer_view || cd_view))
        monitor = new Monitor(tc, peer_view, cd_view, file_view);
}

void InfoWidgetPlugin::torrentRemoved(bt::TorrentInterface *tc)
{
    file_view->onTorrentRemoved(tc);
    // for some reason currentTorrentChanged doesn't always get called
    // when the current torrent is removed, this leads to crashes
    // so manually call it here, to prevent crashes
    currentTorrentChanged(getGUI()->getTorrentActivity()->getCurrentTorrent());
}

}

#include "infowidgetplugin.moc"

#include "moc_infowidgetplugin.cpp"
