ktorrent_add_plugin(ktorrent_scanforlostfiles)

target_sources(ktorrent_scanforlostfiles PRIVATE
	scanforlostfilesplugin.cpp
	scanforlostfileswidget.cpp
	scanforlostfilesprefpage.cpp
	scanforlostfilesthread.cpp
	nodeoperations.cpp)

ki18n_wrap_ui(ktorrent_scanforlostfiles scanforlostfilesprefpage.ui scanforlostfileswidget.ui)
kconfig_add_kcfg_files(ktorrent_scanforlostfiles scanforlostfilespluginsettings.kcfgc)

target_link_libraries(
    ktorrent_scanforlostfiles
    ktcore
    KF5::Torrent
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::I18n
    KF5::KIOCore
)
