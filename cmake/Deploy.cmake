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
    # here by version and SHA-256, and packs itself with them. The runtime is
    # the static one of the AppImage project, which needs no FUSE library on the
    # machine an AppImage runs on. To move to newer ones, change the versions
    # and the four checksums together.
    set(APPIMAGETOOL_VERSION 1.9.1)
    set(APPIMAGE_RUNTIME_VERSION 20251108)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(APPIMAGETOOL_SHA256 ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0)
        set(APPIMAGE_RUNTIME_SHA256 2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(APPIMAGETOOL_SHA256 f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158)
        set(APPIMAGE_RUNTIME_SHA256 00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444)
    else()
        message(FATAL_ERROR "No appimagetool is pinned for ${CMAKE_SYSTEM_PROCESSOR}.")
    endif()
    # Both land beside the linuxdeployqt of the build tree, under the names it
    # looks for there, so that it can pack itself.
    set(APPIMAGETOOL_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/appimagetool)
    set(APPIMAGE_RUNTIME_FILE ${CMAKE_CURRENT_BINARY_DIR}/appimage-runtime)
    file(
        DOWNLOAD
        "https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/appimagetool-${CMAKE_SYSTEM_PROCESSOR}.AppImage"
        ${APPIMAGETOOL_EXECUTABLE}
        EXPECTED_HASH SHA256=${APPIMAGETOOL_SHA256}
        TLS_VERIFY TRUE
    )
    file(CHMOD ${APPIMAGETOOL_EXECUTABLE} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    file(
        DOWNLOAD
        "https://github.com/AppImage/type2-runtime/releases/download/${APPIMAGE_RUNTIME_VERSION}/runtime-${CMAKE_SYSTEM_PROCESSOR}"
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
