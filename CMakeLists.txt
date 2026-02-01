cmake_minimum_required(VERSION 3.19)
project(PrettyReader VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Extra CMake Modules (for KDE macros)
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)

# Qt
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
qt_standard_project_setup()

# KDE Frameworks
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6ConfigWidgets REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(KF6SyntaxHighlighting REQUIRED)

add_subdirectory(src)
