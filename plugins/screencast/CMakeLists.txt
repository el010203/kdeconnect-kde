kdeconnect_add_plugin(kdeconnect_screencast SOURCES screencastplugin.cpp screencastwindow.cpp kwinscreenshot.cpp)
target_link_libraries(kdeconnect_screencast
    kdeconnectcore
    Qt::DBus
    Qt::Gui
    Qt::Widgets
    KF6::I18n
)
