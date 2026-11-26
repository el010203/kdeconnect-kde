kdeconnect_add_plugin(kdeconnect_filebrowser SOURCES filebrowserplugin.cpp)
target_link_libraries(kdeconnect_filebrowser
    kdeconnectcore
    KF6::I18n
)
