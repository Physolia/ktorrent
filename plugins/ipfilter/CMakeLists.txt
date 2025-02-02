ktorrent_add_plugin(ktorrent_ipfilter)

find_package(KF5Archive ${KF5_MIN_VERSION} REQUIRED)

target_sources(ktorrent_ipfilter PRIVATE
    ipblocklist.cpp
    ipblockingprefpage.cpp
    convertthread.cpp
    convertdialog.cpp
    ipfilterplugin.cpp
    downloadandconvertjob.cpp
)

ki18n_wrap_ui(ktorrent_ipfilter ipblockingprefpage.ui convertdialog.ui)
kconfig_add_kcfg_files(ktorrent_ipfilter ipfilterpluginsettings.kcfgc)

target_link_libraries(
    ktorrent_ipfilter
    ktcore
    KF5::Torrent
    KF5::Archive
    KF5::CoreAddons
    KF5::I18n
    KF5::KIOWidgets
    KF5::Notifications
    KF5::TextWidgets
    KF5::WidgetsAddons
)

find_package(Qt${QT_MAJOR_VERSION}Test ${QT5_REQUIRED_VERSION})
if (Qt${QT_MAJOR_VERSION}Test_DIR)
    add_subdirectory(tests)
endif()
