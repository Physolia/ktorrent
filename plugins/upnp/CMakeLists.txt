ktorrent_add_plugin(ktorrent_upnp)

target_sources(ktorrent_upnp PRIVATE
    upnpplugin.cpp
    upnpwidget.cpp
    routermodel.cpp
)

ki18n_wrap_ui(ktorrent_upnp upnpwidget.ui)
kconfig_add_kcfg_files(ktorrent_upnp upnppluginsettings.kcfgc)

target_link_libraries(
    ktorrent_upnp
    ktcore
    KF5::Torrent
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::I18n
    KF5::WidgetsAddons
)
