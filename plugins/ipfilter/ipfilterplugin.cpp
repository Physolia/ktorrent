/*
    SPDX-FileCopyrightText: 2005 Joris Guisson <joris.guisson@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ipfilterplugin.h"

#include <KMainWindow>
#include <KNotification>
#include <KPluginFactory>

#include <QTimer>

#include <interfaces/coreinterface.h>
#include <interfaces/functions.h>
#include <interfaces/guiinterface.h>
#include <peer/accessmanager.h>
#include <util/constants.h>
#include <util/log.h>
#include <util/logsystemmanager.h>

#include "ipblocklist.h"
#include "ipfilterpluginsettings.h"

using namespace bt;

K_PLUGIN_CLASS_WITH_JSON(kt::IPFilterPlugin, "ktorrent_ipfilter.json")

namespace kt
{
IPFilterPlugin::IPFilterPlugin(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plugin(parent, data, args)
{
    connect(&auto_update_timer, &QTimer::timeout, this, &IPFilterPlugin::checkAutoUpdate);
    auto_update_timer.setSingleShot(true);
}

IPFilterPlugin::~IPFilterPlugin()
{
}

void IPFilterPlugin::load()
{
    LogSystemManager::instance().registerSystem(i18n("IP Filter"), SYS_IPF);
    pref = new IPBlockingPrefPage(this);
    connect(pref, &IPBlockingPrefPage::updateFinished, this, &IPFilterPlugin::checkAutoUpdate);
    getGUI()->addPrefPage(pref);

    if (IPBlockingPluginSettings::useLevel1())
        loadAntiP2P();

    checkAutoUpdate();
}

void IPFilterPlugin::unload()
{
    LogSystemManager::instance().unregisterSystem(i18n("IP Filter"));
    getGUI()->removePrefPage(pref);
    delete pref;
    pref = nullptr;
    if (ip_filter) {
        AccessManager::instance().removeBlockList(ip_filter.data());
        ip_filter.reset();
    }
}

bool IPFilterPlugin::loadAntiP2P()
{
    if (ip_filter)
        return true;

    ip_filter.reset(new IPBlockList());
    if (!ip_filter->load(kt::DataDir() + QStringLiteral("level1.dat"))) {
        ip_filter.reset();
        return false;
    }
    AccessManager::instance().addBlockList(ip_filter.data());
    return true;
}

bool IPFilterPlugin::unloadAntiP2P()
{
    if (ip_filter) {
        AccessManager::instance().removeBlockList(ip_filter.data());
        ip_filter.reset();
        return true;
    } else
        return true;
}

bool IPFilterPlugin::loadedAndRunning()
{
    return !ip_filter.isNull();
}

void IPFilterPlugin::checkAutoUpdate()
{
    auto_update_timer.stop();
    if (!loadedAndRunning() || !IPBlockingPluginSettings::autoUpdate())
        return;

    KConfigGroup g = KSharedConfig::openConfig()->group("IPFilterAutoUpdate");
    bool ok = g.readEntry("last_update_ok", false);
    QDateTime now = QDateTime::currentDateTime();
    if (!ok) {
        QDateTime last_update_attempt = g.readEntry("last_update_attempt", now);
        // if we cannot do it now, or the last attempt was less then 15 minute ago, try again in 15 minutes
        if (last_update_attempt.secsTo(now) < AUTO_UPDATE_RETRY_INTERVAL || !pref->doAutoUpdate())
            auto_update_timer.start(AUTO_UPDATE_RETRY_INTERVAL * 1000);
    } else {
        QDateTime last_updated = g.readEntry("last_updated", QDateTime());
        QDateTime next_update;
        if (last_updated.isNull())
            next_update = now.addDays(IPBlockingPluginSettings::autoUpdateInterval());
        else
            next_update = QDateTime(last_updated).addDays(IPBlockingPluginSettings::autoUpdateInterval());

        if (now >= next_update) {
            if (!pref->doAutoUpdate()) // if we cannot do it now, try again in 15 minutes
                auto_update_timer.start(AUTO_UPDATE_RETRY_INTERVAL * 1000);
        } else {
            // schedule an auto update
            auto_update_timer.start(1000 * (now.secsTo(next_update) + 5));
            Out(SYS_IPF | LOG_NOTICE) << "Scheduling ipfilter auto update on " << next_update.toString() << endl;
        }
    }
}

void IPFilterPlugin::notification(const QString &msg)
{
    KNotification::event(QStringLiteral("PluginEvent"), msg, QPixmap(), getGUI()->getMainWindow());
}

}

#include <ipfilterplugin.moc>

#include "moc_ipfilterplugin.cpp"
