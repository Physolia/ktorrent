ktorrent_add_plugin(ktorrent_zeroconf)

target_sources(ktorrent_zeroconf PRIVATE
    zeroconfplugin.cpp
    torrentservice.cpp
)

# ki18n_wrap_ui(ktorrent_zeroconf zeroconfpref.ui searchbar.ui)
#kconfig_add_kcfg_files(ktorrent_zeroconf zeroconfpluginsettings.kcfgc)

target_link_libraries(ktorrent_zeroconf
    ktcore
    KF5::Torrent
    KF5::CoreAddons
    KF5::DNSSD
    KF5::I18n
)
