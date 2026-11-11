include_guard(GLOBAL)

function(deploy TARGET)
    if(APP_DEPLOY_AS_PART_OF_ALL)
        set(ALL ALL)
    endif()

    if(NOT TARGET deploy)
        add_custom_target(deploy ${ALL} DEPENDS ${TARGET})
    endif()

    set(DEPLOY_PREFIX_PATH ${APP_DEPLOY_PREFIX}/${TARGET}.AppDir)
    set(APPIMAGE_OUTPUT_ARCH ${CMAKE_SYSTEM_PROCESSOR})
    if(APPIMAGE_OUTPUT_ARCH STREQUAL "aarch64")
        set(APPIMAGE_OUTPUT_ARCH arm64)
    endif()
    set(APPIMAGE_OUTPUT_PATH ${APP_DEPLOY_PREFIX}/${TARGET}-${APPIMAGE_OUTPUT_ARCH}.AppImage)

    find_program(PATCHELF_EXECUTABLE patchelf REQUIRED)

    # linuxdeployqt carries appimagetool and the AppImage runtime, both pinned
    # here by release and SHA-256, and packs itself with them. The runtime is
    # the static one of the AppImage project, which needs no FUSE library on the
    # machine an AppImage runs on. Both files come from this account's forks of
    # the AppImage project's repositories, github.com/omergoktas/appimagetool
    # and github.com/omergoktas/type2-runtime, which build the project's source
    # at its tags 1.9.1 and 20251108 with the project's own build scripts, so
    # each file is built from a commit that can be read, with its build log
    # kept. To move to newer ones, release them in the forks first, then change
    # the releases and the four checksums here together.
    set(APPIMAGETOOL_RELEASE 1.9.1-1)
    set(APPIMAGE_RUNTIME_RELEASE 20251108-1)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(APPIMAGETOOL_SHA256 2eb9321e9c1790f5297f16ddc3187c6a17c4772abcfaf0efc49bec85456730bd)
        set(APPIMAGE_RUNTIME_SHA256 7afaed97f0ea1c30c68e6beb3a0528db4398fe4e6b4f1a23f2077957277fecdf)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(APPIMAGETOOL_SHA256 d698e03521232c46a88c1f9f19ac18019142409dcc9dcc073fd47a4ec0a17e4f)
        set(APPIMAGE_RUNTIME_SHA256 10d582ecc70c2836afee511cfc79ea6e8733ca686cb2258d86b3c4fec1b84b0b)
    else()
        message(FATAL_ERROR "No appimagetool is pinned for ${CMAKE_SYSTEM_PROCESSOR}.")
    endif()
    # Both land beside the linuxdeployqt of the build tree, under the names it
    # looks for there, so that it can pack itself.
    set(APPIMAGETOOL_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/appimagetool)
    set(APPIMAGE_RUNTIME_FILE ${CMAKE_CURRENT_BINARY_DIR}/appimage-runtime)
    file(
        DOWNLOAD
        "https://github.com/omergoktas/appimagetool/releases/download/${APPIMAGETOOL_RELEASE}/appimagetool-${CMAKE_SYSTEM_PROCESSOR}.AppImage"
        ${APPIMAGETOOL_EXECUTABLE}
        EXPECTED_HASH SHA256=${APPIMAGETOOL_SHA256}
        TLS_VERIFY TRUE
    )
    file(CHMOD ${APPIMAGETOOL_EXECUTABLE} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    file(
        DOWNLOAD
        "https://github.com/omergoktas/type2-runtime/releases/download/${APPIMAGE_RUNTIME_RELEASE}/runtime-${CMAKE_SYSTEM_PROCESSOR}"
        ${APPIMAGE_RUNTIME_FILE}
        EXPECTED_HASH SHA256=${APPIMAGE_RUNTIME_SHA256}
        TLS_VERIFY TRUE
    )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/deploy/icon.desktop
        ${DEPLOY_PREFIX_PATH}/${TARGET}.desktop @ONLY)

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/deploy/icon.svg ${DEPLOY_PREFIX_PATH}/${TARGET}.svg
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${DEPLOY_PREFIX_PATH}/usr/bin
        ${DEPLOY_PREFIX_PATH}/usr/lib
    )

    # appimagetool is an AppImage itself. Its programs, appimagetool,
    # mksquashfs and the two it checks a package with, are static, so they go
    # into the AppDir as they are.
    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${APPIMAGETOOL_EXECUTABLE} --appimage-extract
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/squashfs-root/usr/bin ${DEPLOY_PREFIX_PATH}/usr/bin
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${APPIMAGE_RUNTIME_FILE} ${DEPLOY_PREFIX_PATH}/usr/bin/appimage-runtime
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f ${DEPLOY_PREFIX_PATH}/usr/bin/AppRun
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PATCHELF_EXECUTABLE} $<TARGET_FILE:${TARGET}> ${DEPLOY_PREFIX_PATH}/usr/bin
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env
        ARCH=${CMAKE_SYSTEM_PROCESSOR}
        APPIMAGE_EXTRACT_AND_RUN=1
        --
        $<TARGET_FILE:${TARGET}>
        ${DEPLOY_PREFIX_PATH}/usr/bin/$<TARGET_FILE_NAME:${TARGET}>
        -appimage -appimage-output=${APPIMAGE_OUTPUT_PATH}
        -no-translations -qmake=${QMAKE_EXECUTABLE}
        WORKING_DIRECTORY ${APP_DEPLOY_PREFIX}
    )
endfunction()
