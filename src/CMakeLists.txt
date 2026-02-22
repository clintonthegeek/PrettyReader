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
    markdown/footnoteparser.cpp
    markdown/footnoteparser.h
    canvas/pageitem.cpp
    canvas/pageitem.h
    canvas/documentview.cpp
    canvas/documentview.h
    canvas/pdfpageitem.cpp
    canvas/pdfpageitem.h
    canvas/rendercache.cpp
    canvas/rendercache.h
    canvas/qtboxrenderer.cpp
    canvas/qtboxrenderer.h
    canvas/webviewitem.cpp
    canvas/webviewitem.h
    print/headerfooterrenderer.cpp
    print/headerfooterrenderer.h
    print/printcontroller.cpp
    print/printcontroller.h
    style/pagelayout.h
    style/masterpage.h
    style/fontfeatures.h
    style/paragraphstyle.cpp
    style/paragraphstyle.h
    style/characterstyle.cpp
    style/characterstyle.h
    style/tablestyle.cpp
    style/tablestyle.h
    style/footnotestyle.cpp
    style/footnotestyle.h
    style/stylemanager.cpp
    style/stylemanager.h
    style/styletreemodel.cpp
    style/styletreemodel.h
    style/colorpalette.cpp
    style/colorpalette.h
    style/fontpairing.cpp
    style/fontpairing.h
    style/fontdegradationmap.cpp
    style/fontdegradationmap.h
    style/thememanager.cpp
    style/thememanager.h
    style/themecomposer.cpp
    style/themecomposer.h
    style/palettemanager.cpp
    style/palettemanager.h
    style/fontpairingmanager.cpp
    style/fontpairingmanager.h
    typography/hyphenator.cpp
    typography/hyphenator.h
    typography/shortwords.cpp
    typography/shortwords.h
    export/rtfexporter.cpp
    export/rtfexporter.h
    export/contentrtfexporter.cpp
    export/contentrtfexporter.h
    export/rtffilteroptions.h
    # PDF rendering pipeline (Phase 4)
    model/contentbuilder.cpp
    model/contentbuilder.h
    model/contentmodel.h
    model/pdfexportoptions.h
    model/pagerangeparser.cpp
    model/pagerangeparser.h
    model/contentfilter.cpp
    model/contentfilter.h
    font/fontmanager.cpp
    font/fontmanager.h
    font/textshaper.cpp
    font/textshaper.h
    font/sfnt.cpp
    font/sfnt.h
    font/hersheyfont.cpp
    font/hersheyfont.h
    layout/layoutengine.cpp
    layout/layoutengine.h
    layout/linebreaker.cpp
    layout/linebreaker.h
    pdf/pdfwriter.cpp
    pdf/pdfwriter.h
    pdf/pdfgenerator.cpp
    pdf/pdfgenerator.h
    pdf/pdfboxrenderer.cpp
    pdf/pdfboxrenderer.h
    # Rendering base class
    render/boxtreerenderer.cpp
    render/boxtreerenderer.h
    # Widgets
    widgets/pagelayoutwidget.cpp
    widgets/pagelayoutwidget.h
    widgets/sidebar.cpp
    widgets/sidebar.h
    widgets/styledockwidget.cpp
    widgets/styledockwidget.h
    widgets/stylepropertieseditor.cpp
    widgets/stylepropertieseditor.h
    widgets/tablestylepropertieseditor.cpp
    widgets/tablestylepropertieseditor.h
    widgets/footnoteconfigwidget.cpp
    widgets/footnoteconfigwidget.h
    widgets/preferencesdialog.cpp
    widgets/preferencesdialog.h
    widgets/rtfcopyoptionsdialog.cpp
    widgets/rtfcopyoptionsdialog.h
    widgets/languagepickerdialog.cpp
    widgets/languagepickerdialog.h
    widgets/toolview.cpp
    widgets/toolview.h
    widgets/filebrowserdock.cpp
    widgets/filebrowserdock.h
    widgets/tocwidget.cpp
    widgets/tocwidget.h
    widgets/pdfexportdialog.cpp
    widgets/pdfexportdialog.h
    widgets/documenttab.cpp
    widgets/documenttab.h
    widgets/palettepickerwidget.cpp
    widgets/palettepickerwidget.h
    widgets/fontpairingpickerwidget.cpp
    widgets/fontpairingpickerwidget.h
    widgets/paletteeditordialog.cpp
    widgets/paletteeditordialog.h
    widgets/fontpairingeditordialog.cpp
    widgets/fontpairingeditordialog.h
    widgets/themepickerdock.cpp
    widgets/themepickerdock.h
)

target_include_directories(PrettyReaderCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/app
        ${CMAKE_CURRENT_SOURCE_DIR}/markdown
        ${CMAKE_CURRENT_SOURCE_DIR}/canvas
        ${CMAKE_CURRENT_SOURCE_DIR}/print
        ${CMAKE_CURRENT_SOURCE_DIR}/style
        ${CMAKE_CURRENT_SOURCE_DIR}/typography
        ${CMAKE_CURRENT_SOURCE_DIR}/export
        ${CMAKE_CURRENT_SOURCE_DIR}/widgets
        # PDF pipeline directories (Phase 4)
        ${CMAKE_CURRENT_SOURCE_DIR}/model
        ${CMAKE_CURRENT_SOURCE_DIR}/font
        ${CMAKE_CURRENT_SOURCE_DIR}/layout
        ${CMAKE_CURRENT_SOURCE_DIR}/pdf
        ${CMAKE_CURRENT_SOURCE_DIR}/render
        ${CMAKE_CURRENT_BINARY_DIR}
)

# MD4C system library
find_library(MD4C_LIBRARY NAMES md4c REQUIRED)
find_path(MD4C_INCLUDE_DIR NAMES md4c.h REQUIRED)

# libhyphen for hyphenation
find_library(HYPHEN_LIBRARY NAMES hyphen REQUIRED)
find_path(HYPHEN_INCLUDE_DIR NAMES hyphen.h REQUIRED)

# Bundle theme JSON files as Qt resources
qt_add_resources(PrettyReaderCore "themes"
    PREFIX "/themes"
    BASE themes
    FILES
        themes/default.json
        themes/academic.json
        themes/hershey.json
)

# Bundle color palette JSON files as Qt resources
qt_add_resources(PrettyReaderCore "palettes"
    PREFIX "/palettes"
    BASE themes/palettes
    FILES
        themes/palettes/default-light.json
        themes/palettes/academic-light.json
        themes/palettes/hershey-ink.json
        themes/palettes/solarized-light.json
        themes/palettes/solarized-dark.json
        themes/palettes/dracula.json
        themes/palettes/nord.json
        themes/palettes/catppuccin-mocha.json
        themes/palettes/catppuccin-latte.json
        themes/palettes/gruvbox-dark.json
        themes/palettes/gruvbox-light.json
        themes/palettes/tokyo-night.json
        themes/palettes/rose-pine.json
        themes/palettes/rose-pine-dawn.json
        themes/palettes/one-dark.json
        themes/palettes/one-light.json
        themes/palettes/everforest-dark.json
        themes/palettes/everforest-light.json
        themes/palettes/flexoki-dark.json
        themes/palettes/flexoki-light.json
        themes/palettes/github-light.json
)

# Bundle font pairing JSON files as Qt resources
qt_add_resources(PrettyReaderCore "pairings"
    PREFIX "/pairings"
    BASE themes/pairings
    FILES
        themes/pairings/noto-serif-sans-jetbrains.json
        themes/pairings/noto-serif-source-code.json
        themes/pairings/hershey-serif-sans-roman.json
        themes/pairings/liberation-serif-sans-mono.json
        themes/pairings/dejavu-family.json
        themes/pairings/georgia-trebuchet-courier.json
        themes/pairings/georgia-inter-jetbrains.json
        themes/pairings/noto-source-family.json
        themes/pairings/ubuntu-family.json
        themes/pairings/fira-family.json
        themes/pairings/typewriter.json
        themes/pairings/sans-forward.json
        themes/pairings/serif-scholar.json
        themes/pairings/inverted-contrast.json
        themes/pairings/libertine-biolinum-hack.json
)

# Bundle theme preset JSON files as Qt resources
qt_add_resources(PrettyReaderCore "presets"
    PREFIX "/presets"
    BASE themes/presets
    FILES
        themes/presets/default.json
        themes/presets/academic.json
        themes/presets/hershey.json
)

# Bundle hyphenation dictionaries as Qt resources
qt_add_resources(PrettyReaderCore "dicts"
    PREFIX "/dicts"
    BASE dicts
    FILES
        dicts/hyph_en_US.dic
        dicts/hyph_en_GB.dic
)

# Bundle fallback symbol font as Qt resource
qt_add_resources(PrettyReaderCore "fonts"
    PREFIX "/fonts"
    BASE dicts
    FILES
        dicts/PrettySymbolsFallback.ttf
)

# Bundle glyph artwork (checkbox SVGs) as Qt resources
qt_add_resources(PrettyReaderCore "glyphs"
    PREFIX "/glyphs"
    BASE glyphs
    FILES
        glyphs/checkbox-unchecked.svg
        glyphs/checkbox-checked.svg
)

# Bundle Hershey vector font data as Qt resources
qt_add_resources(PrettyReaderCore "hershey"
    PREFIX "/hershey"
    BASE hershey
    FILES
        hershey/astrology.jhf
        hershey/cursive.jhf
        hershey/cyrilc_1.jhf
        hershey/cyrillic.jhf
        hershey/futural.jhf
        hershey/futuram.jhf
        hershey/gothgbt.jhf
        hershey/gothgrt.jhf
        hershey/gothiceng.jhf
        hershey/gothicger.jhf
        hershey/gothicita.jhf
        hershey/gothitt.jhf
        hershey/greek.jhf
        hershey/greekc.jhf
        hershey/greeks.jhf
        hershey/japanese.jhf
        hershey/markers.jhf
        hershey/mathlow.jhf
        hershey/mathupp.jhf
        hershey/meteorology.jhf
        hershey/music.jhf
        hershey/rowmand.jhf
        hershey/rowmans.jhf
        hershey/rowmant.jhf
        hershey/scriptc.jhf
        hershey/scripts.jhf
        hershey/symbolic.jhf
        hershey/timesg.jhf
        hershey/timesi.jhf
        hershey/timesib.jhf
        hershey/timesr.jhf
        hershey/timesrb.jhf
)

target_include_directories(PrettyReaderCore
    PRIVATE
        ${MD4C_INCLUDE_DIR}
        ${HYPHEN_INCLUDE_DIR}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
        ${HARFBUZZ_SUBSET_INCLUDE_DIRS}
)

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
        ${HYPHEN_LIBRARY}
        # PDF pipeline dependencies (Phase 4)
        Poppler::Qt6
        Freetype::Freetype
        ICU::uc
        ICU::i18n
        ZLIB::ZLIB
        ${FONTCONFIG_LIBRARIES}
        ${HARFBUZZ_LIBRARIES}
        ${HARFBUZZ_ICU_LIBRARIES}
        ${HARFBUZZ_SUBSET_LIBRARIES}
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
