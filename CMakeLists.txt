cmake_minimum_required(VERSION 3.19)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

include(Options)

project(linuxdeployqt
    LANGUAGES CXX
    VERSION 2.4.0
    HOMEPAGE_URL omergoktas.com
    DESCRIPTION "Qt dependency deployment tool for Linux."
)

include(Setup)
include(Deploy)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Core)

qt_add_executable(linuxdeployqt
    main.cpp
    shared.h
    shared.cpp
)

install(TARGETS linuxdeployqt DESTINATION bin)

deploy(linuxdeployqt)
