include_guard(GLOBAL)

function(deploy TARGET)
    if(APP_DEPLOY_AS_PART_OF_ALL)
        set(ALL ALL)
    endif()

    if(NOT TARGET deploy)
        add_custom_target(deploy ${ALL} DEPENDS ${TARGET})
    endif()

    set(DEPLOY_PREFIX_PATH ${APP_DEPLOY_PREFIX}/${TARGET}.AppDir)
    set(APPIMAGE_OUTPUT_ARGUMENT)
    if(APP_DEPLOY_OUTPUT_PATH)
        set(APPIMAGE_OUTPUT_ARGUMENT "-appimage-output=${APP_DEPLOY_OUTPUT_PATH}")
    endif()

    find_program(APPIMAGETOOL_EXECUTABLE appimagetool)
    find_program(PATCHELF_EXECUTABLE patchelf REQUIRED)

    if(NOT APPIMAGETOOL_EXECUTABLE)
        message(STATUS "Could NOT find appimagetool, downloading...")
        set(APPIMAGETOOL_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/appimagetool)
        set(APPIMAGETOOL_URL "https://github.com/AppImage/AppImageKit/releases/download/13/obsolete-appimagetool-${CMAKE_SYSTEM_PROCESSOR}.AppImage")
        file(
            DOWNLOAD ${APPIMAGETOOL_URL} ${APPIMAGETOOL_EXECUTABLE}
            SHOW_PROGRESS
            STATUS APPIMAGETOOL_DOWNLOAD_STATUS
        )
        list(GET APPIMAGETOOL_DOWNLOAD_STATUS 0 APPIMAGETOOL_DOWNLOAD_RESULT)
        if(NOT APPIMAGETOOL_DOWNLOAD_RESULT EQUAL 0)
            list(GET APPIMAGETOOL_DOWNLOAD_STATUS 1 APPIMAGETOOL_DOWNLOAD_ERROR)
            message(FATAL_ERROR "Could NOT download appimagetool: ${APPIMAGETOOL_DOWNLOAD_ERROR}")
        endif()
        file(CHMOD ${APPIMAGETOOL_EXECUTABLE} PERMISSIONS OWNER_READ OWNER_EXECUTE)
    endif()

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

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${APPIMAGETOOL_EXECUTABLE} --appimage-extract
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/squashfs-root/usr/bin ${DEPLOY_PREFIX_PATH}/usr/bin
    )

    add_custom_command(TARGET deploy VERBATIM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/squashfs-root/usr/lib ${DEPLOY_PREFIX_PATH}/usr/lib
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
        -appimage ${APPIMAGE_OUTPUT_ARGUMENT}
        -no-translations -qmake=${QMAKE_EXECUTABLE}
        WORKING_DIRECTORY ${APP_DEPLOY_PREFIX}
    )
endfunction()
