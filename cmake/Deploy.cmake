include_guard(GLOBAL)

function(deploy TARGET)
    if(APP_DEPLOY_AS_PART_OF_ALL)
        set(ALL ALL)
    endif()

    if(NOT TARGET deploy)
        add_custom_target(deploy ${ALL} DEPENDS ${TARGET})
    endif()

    set(DEPLOY_PREFIX_PATH ${APP_DEPLOY_PREFIX}/${TARGET}.AppDir)

    find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt)
    find_program(APPIMAGETOOL_EXECUTABLE appimagetool)
    find_program(PATCHELF_EXECUTABLE patchelf REQUIRED)

    if(NOT LINUXDEPLOYQT_EXECUTABLE)
        message(STATUS "Could NOT find linuxdeployqt, downloading...")
        set(LINUXDEPLOYQT_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/linuxdeployqt.run)
        set(LINUXDEPLOYQT_URL "https://github.com/omergoktas/linuxdeployqt/releases/download/latest/linuxdeployqt-${CMAKE_SYSTEM_PROCESSOR}.AppImage")
        file(DOWNLOAD ${LINUXDEPLOYQT_URL} ${LINUXDEPLOYQT_EXECUTABLE} SHOW_PROGRESS)
        file(CHMOD ${LINUXDEPLOYQT_EXECUTABLE} PERMISSIONS OWNER_READ OWNER_EXECUTE)
    endif()

    if(NOT APPIMAGETOOL_EXECUTABLE)
        message(STATUS "Could NOT find appimagetool, downloading...")
        set(APPIMAGETOOL_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/appimagetool)
        set(APPIMAGETOOL_URL "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${CMAKE_SYSTEM_PROCESSOR}.AppImage")
        file(DOWNLOAD ${APPIMAGETOOL_URL} ${APPIMAGETOOL_EXECUTABLE} SHOW_PROGRESS)
        file(CHMOD ${APPIMAGETOOL_EXECUTABLE} PERMISSIONS OWNER_READ OWNER_EXECUTE)
    endif()

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/icon.desktop
        ${DEPLOY_PREFIX_PATH}/${TARGET}.desktop @ONLY)

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${DEPLOY_PREFIX_PATH}/usr/bin
        ${DEPLOY_PREFIX_PATH}/usr/lib
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${APPIMAGETOOL_EXECUTABLE} --appimage-extract
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/squashfs-root/usr/bin ${DEPLOY_PREFIX_PATH}/usr/bin
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/squashfs-root/usr/lib ${DEPLOY_PREFIX_PATH}/usr/lib
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${CMAKE_COMMAND} -E rm -f ${DEPLOY_PREFIX_PATH}/usr/bin/AppRun
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PATCHELF_EXECUTABLE} $<TARGET_FILE:${TARGET}> ${DEPLOY_PREFIX_PATH}/usr/bin
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        icon.svg ${DEPLOY_PREFIX_PATH}/${TARGET}.svg
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_custom_command(TARGET deploy VERBATIM
        COMMAND ARCH=${CMAKE_SYSTEM_PROCESSOR} ${LINUXDEPLOYQT_EXECUTABLE}
        ${DEPLOY_PREFIX_PATH}/usr/bin/$<TARGET_FILE_NAME:${TARGET}>
        -appimage -no-translations -qmake=${QMAKE_EXECUTABLE}
        WORKING_DIRECTORY ${APP_DEPLOY_PREFIX}
    )
endfunction()
