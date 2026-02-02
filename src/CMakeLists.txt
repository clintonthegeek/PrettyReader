# Generate KConfig settings class from kcfg schema
kconfig_add_kcfg_files(PRETTYREADER_KCFG_SRCS app/prettyreadersettings.kcfgc GENERATE_MOC)

add_library(PrettyReaderCore STATIC
    ${PRETTYREADER_KCFG_SRCS}
    app/mainwindow.cpp
    app/mainwindow.h
    app/metadatastore.cpp
    app/metadatastore.h
    markdown/documentbuilder.cpp
    markdown/documentbuilder.h
    markdown/codeblockhighlighter.cpp
    markdown/codeblockhighlighter.h
    markdown/markdownhighlighter.cpp
    markdown/markdownhighlighter.h
    canvas/pageitem.cpp
    canvas/pageitem.h
    canvas/documentview.cpp
    canvas/documentview.h
    print/headerfooterrenderer.cpp
    print/headerfooterrenderer.h
    print/printcontroller.cpp
    print/printcontroller.h
    style/pagelayout.h
    style/paragraphstyle.cpp
    style/paragraphstyle.h
    style/characterstyle.cpp
    style/characterstyle.h
    style/stylemanager.cpp
    style/stylemanager.h
    style/styletreemodel.cpp
    style/styletreemodel.h
    style/thememanager.cpp
    style/thememanager.h
    widgets/pagelayoutwidget.cpp
    widgets/pagelayoutwidget.h
    widgets/sidebar.cpp
    widgets/sidebar.h
    widgets/styledockwidget.cpp
    widgets/styledockwidget.h
    widgets/stylepropertieseditor.cpp
    widgets/stylepropertieseditor.h
    widgets/toolview.cpp
    widgets/toolview.h
    widgets/filebrowserdock.cpp
    widgets/filebrowserdock.h
    widgets/tocwidget.cpp
    widgets/tocwidget.h
    widgets/documenttab.cpp
    widgets/documenttab.h
)

target_include_directories(PrettyReaderCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/app
        ${CMAKE_CURRENT_SOURCE_DIR}/markdown
        ${CMAKE_CURRENT_SOURCE_DIR}/canvas
        ${CMAKE_CURRENT_SOURCE_DIR}/print
        ${CMAKE_CURRENT_SOURCE_DIR}/style
        ${CMAKE_CURRENT_SOURCE_DIR}/widgets
        ${CMAKE_CURRENT_BINARY_DIR}
)

# MD4C system library
find_library(MD4C_LIBRARY NAMES md4c REQUIRED)
find_path(MD4C_INCLUDE_DIR NAMES md4c.h REQUIRED)

# Bundle theme JSON files as Qt resources
qt_add_resources(PrettyReaderCore "themes"
    PREFIX "/themes"
    BASE themes
    FILES
        themes/default.json
        themes/academic.json
)

target_include_directories(PrettyReaderCore PRIVATE ${MD4C_INCLUDE_DIR})

target_link_libraries(PrettyReaderCore
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::WidgetsAddons
        KF6::IconThemes
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::ConfigWidgets
        KF6::KIOFileWidgets
        KF6::SyntaxHighlighting
        ${MD4C_LIBRARY}
)

qt_add_executable(PrettyReader
    WIN32 MACOSX_BUNDLE
    app/main.cpp
)

# Embed XMLGUI resource file
qt_add_resources(PrettyReader "xmlgui"
    PREFIX "/kxmlgui5/prettyreader"
    BASE app
    FILES app/prettyreaderui.rc
)

target_link_libraries(PrettyReader
    PRIVATE
        PrettyReaderCore
)

include(GNUInstallDirs)

install(TARGETS PrettyReader
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES app/prettyreaderui.rc
    DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/prettyreader
)

install(FILES ../org.prettyreader.PrettyReader.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)
