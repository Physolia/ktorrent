/*
    SPDX-FileCopyrightText: 2006 Ivan Vasić <ivasic@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "bwschedulerplugin.h"

#include <QDateTime>
#include <QNetworkConfigurationManager>
#include <QTimer>

#include <KLocalizedString>
#include <KPluginFactory>

#include <interfaces/coreinterface.h>
#include <interfaces/functions.h>
#include <interfaces/guiinterface.h>
#include <net/socketmonitor.h>
#include <settings.h>
#include <util/constants.h>
#include <util/error.h>
#include <util/log.h>
#include <util/logsystemmanager.h>

#include "bwprefpage.h"
#include "schedule.h"
#include "scheduleeditor.h"

#include <bwschedulerpluginsettings.h>
#include <peer/peermanager.h>
#include <torrent/globals.h>

using namespace bt;

K_PLUGIN_CLASS_WITH_JSON(kt::BWSchedulerPlugin, "ktorrent_bwscheduler.json")

namespace kt
{
BWSchedulerPlugin::BWSchedulerPlugin(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plugin(parent, data, args)
    , m_editor(nullptr)
    , m_pref(nullptr)
{
    connect(&m_timer, &QTimer::timeout, this, &BWSchedulerPlugin::timerTriggered);
    screensaver =
        new org::freedesktop::ScreenSaver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"), QDBusConnection::sessionBus(), this);
    connect(screensaver, &org::freedesktop::ScreenSaver::ActiveChanged, this, &BWSchedulerPlugin::screensaverActivated);
    screensaver_on = screensaver->GetActive();

    QNetworkConfigurationManager *networkConfigurationManager = new QNetworkConfigurationManager(this);
    connect(networkConfigurationManager, &QNetworkConfigurationManager::onlineStateChanged, this, &BWSchedulerPlugin::networkStatusChanged);
}

BWSchedulerPlugin::~BWSchedulerPlugin()
{
}

void BWSchedulerPlugin::load()
{
    LogSystemManager::instance().registerSystem(i18n("Scheduler"), SYS_SCD);
    m_schedule = new Schedule();
    m_pref = new BWPrefPage(nullptr);
    connect(m_pref, &BWPrefPage::colorsChanged, this, &BWSchedulerPlugin::colorsChanged);
    getGUI()->addPrefPage(m_pref);

    connect(getCore(), &CoreInterface::settingsChanged, this, &BWSchedulerPlugin::colorsChanged);

    try {
        m_schedule->load(kt::DataDir() + QLatin1String("current.sched"));
    } catch (bt::Error &err) {
        Out(SYS_SCD | LOG_NOTICE) << "Failed to load current.sched : " << err.toString() << endl;
        m_schedule->clear();
    }

    m_editor = new ScheduleEditor(nullptr);
    connect(m_editor, &ScheduleEditor::loaded, this, &BWSchedulerPlugin::onLoaded);
    connect(m_editor, &ScheduleEditor::scheduleChanged, this, &BWSchedulerPlugin::timerTriggered);
    getGUI()->addActivity(m_editor);
    m_editor->setSchedule(m_schedule);

    // make sure that schedule gets applied again if the settings change
    connect(getCore(), &CoreInterface::settingsChanged, this, &BWSchedulerPlugin::timerTriggered);
    timerTriggered();
}

void BWSchedulerPlugin::unload()
{
    setNormalLimits();
    LogSystemManager::instance().unregisterSystem(i18n("Bandwidth Scheduler"));
    disconnect(getCore(), &CoreInterface::settingsChanged, this, &BWSchedulerPlugin::colorsChanged);
    disconnect(getCore(), &CoreInterface::settingsChanged, this, &BWSchedulerPlugin::timerTriggered);
    m_timer.stop();

    getGUI()->removeActivity(m_editor);
    delete m_editor;
    m_editor = nullptr;

    getGUI()->removePrefPage(m_pref);
    delete m_pref;
    m_pref = nullptr;

    try {
        m_schedule->save(kt::DataDir() + QLatin1String("current.sched"));
    } catch (bt::Error &err) {
        Out(SYS_SCD | LOG_NOTICE) << "Failed to save current.sched : " << err.toString() << endl;
    }

    delete m_schedule;
    m_schedule = nullptr;
}

void BWSchedulerPlugin::setNormalLimits()
{
    int ulim = Settings::maxUploadRate();
    int dlim = Settings::maxDownloadRate();
    if (screensaver_on && SchedulerPluginSettings::screensaverLimits()) {
        ulim = SchedulerPluginSettings::screensaverUploadLimit();
        dlim = SchedulerPluginSettings::screensaverDownloadLimit();
    }

    Out(SYS_SCD | LOG_NOTICE) << QStringLiteral("Changing schedule to normal values : %1 down, %2 up").arg(dlim).arg(ulim) << endl;
    // set normal limits
    getCore()->setSuspendedState(false);
    net::SocketMonitor::setDownloadCap(1024 * dlim);
    net::SocketMonitor::setUploadCap(1024 * ulim);
    if (m_editor)
        m_editor->updateStatusText(ulim, dlim, false, m_schedule->isEnabled());

    PeerManager::connectionLimits().setLimits(Settings::maxTotalConnections(), Settings::maxConnections());
}

void BWSchedulerPlugin::timerTriggered()
{
    QDateTime now = QDateTime::currentDateTime();
    ScheduleItem *item = m_schedule->getCurrentItem(now);
    if (!item || !m_schedule->isEnabled()) {
        setNormalLimits();
        restartTimer();
        return;
    }

    if (item->suspended) {
        Out(SYS_SCD | LOG_NOTICE) << QStringLiteral("Changing schedule to : PAUSED") << endl;
        if (!getCore()->getSuspendedState()) {
            getCore()->setSuspendedState(true);
            net::SocketMonitor::setDownloadCap(1024 * Settings::maxDownloadRate());
            net::SocketMonitor::setUploadCap(1024 * Settings::maxUploadRate());
            if (m_editor)
                m_editor->updateStatusText(Settings::maxUploadRate(), Settings::maxDownloadRate(), true, m_schedule->isEnabled());
        }
    } else {
        int ulim = item->upload_limit;
        int dlim = item->download_limit;
        if (screensaver_on && SchedulerPluginSettings::screensaverLimits()) {
            ulim = item->ss_upload_limit;
            dlim = item->ss_download_limit;
        }

        Out(SYS_SCD | LOG_NOTICE) << QStringLiteral("Changing schedule to : %1 down, %2 up").arg(dlim).arg(ulim) << endl;
        getCore()->setSuspendedState(false);

        net::SocketMonitor::setDownloadCap(1024 * dlim);
        net::SocketMonitor::setUploadCap(1024 * ulim);
        if (m_editor)
            m_editor->updateStatusText(ulim, dlim, false, m_schedule->isEnabled());
    }

    if (item->set_conn_limits) {
        Out(SYS_SCD | LOG_NOTICE)
            << QStringLiteral("Setting connection limits to : %1 per torrent, %2 global").arg(item->torrent_conn_limit).arg(item->global_conn_limit) << endl;

        PeerManager::connectionLimits().setLimits(item->global_conn_limit, item->torrent_conn_limit);
    } else {
        PeerManager::connectionLimits().setLimits(Settings::maxTotalConnections(), Settings::maxConnections());
    }

    restartTimer();
}

void BWSchedulerPlugin::restartTimer()
{
    QDateTime now = QDateTime::currentDateTime();
    // now calculate the new interval
    int wait_time = m_schedule->getTimeToNextScheduleEvent(now) * 1000;
    Out(SYS_SCD | LOG_NOTICE) << "Timer will fire in " << wait_time << " ms" << endl;
    if (wait_time < 1000)
        wait_time = 1000;
    m_timer.stop();
    m_timer.start(wait_time);
}

void BWSchedulerPlugin::onLoaded(Schedule *ns)
{
    delete m_schedule;
    m_schedule = ns;
    m_editor->setSchedule(ns);
    timerTriggered();
}

void BWSchedulerPlugin::colorsChanged()
{
    if (m_editor) {
        m_editor->setSchedule(m_schedule);
        m_editor->colorsChanged();
    }
}

void BWSchedulerPlugin::screensaverActivated(bool on)
{
    screensaver_on = on;
    timerTriggered();
}

void BWSchedulerPlugin::networkStatusChanged(bool online)
{
    if (online) {
        Out(SYS_SCD | LOG_NOTICE) << "Network is up, setting schedule" << endl;
        timerTriggered();
    }
}

}

#include <bwschedulerplugin.moc>

#include "moc_bwschedulerplugin.cpp"
