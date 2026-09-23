cmake_minimum_required(VERSION 3.19)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

include(Options)

project(linuxdeployqt
    LANGUAGES C CXX
    VERSION 2.6.1
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

# checkrt runs from the AppRun of every AppDir that gets the bundled C++
# runtime, and decides whether to use it; see checkrt.c. It needs nothing but
# the C library. linuxdeployqt carries it as a resource and copies it into each
# AppDir.
add_executable(checkrt checkrt.c)
set_target_properties(checkrt PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED TRUE
    C_EXTENSIONS FALSE
    AUTOMOC FALSE
    AUTOUIC FALSE
    AUTORCC FALSE
    # $<0:> keeps multi-config generators from adding a per-config subdirectory
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets$<0:>
)
target_link_libraries(checkrt PRIVATE ${CMAKE_DL_LIBS})
target_link_options(checkrt PRIVATE -s)

set(CHECKRT_ASSET ${CMAKE_CURRENT_BINARY_DIR}/assets/checkrt)
set_source_files_properties(${CHECKRT_ASSET} PROPERTIES
    GENERATED TRUE
    QT_RESOURCE_ALIAS assets/checkrt
)

qt_add_resources(
    linuxdeployqt
    linuxdeployqt_assets
    FILES
        assets/AppRun
        ${CHECKRT_ASSET}
)
add_dependencies(linuxdeployqt checkrt)

install(TARGETS linuxdeployqt DESTINATION bin)

deploy(linuxdeployqt)
