#
# FetchPrebuiltQtWindows.cmake
#
# Copyright (C) 2026, Charles Chiou
#
# Retrieves prebuilt Windows Qt6 libraries (win64_mingw) via aqtinstall when
# cross-compiling on Linux with MinGW-w64. Never builds Qt from source.
#

set(ROBOHERO_QT_WIN_VERSION "6.8.3" CACHE STRING "Prebuilt Windows Qt6 version")
set(ROBOHERO_QT_WIN_ARCH "win64_mingw" CACHE STRING "Prebuilt Windows Qt6 arch")
set(ROBOHERO_QT_WIN_SUBDIR "mingw_64" CACHE STRING "Prebuilt Windows Qt6 subdir")

get_filename_component(_ROBOHERO_WORK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(_SHARED_AIMON_QT_ROOT "${_ROBOHERO_WORK_DIR}/aimon/build/qt-prebuilt/${ROBOHERO_QT_WIN_VERSION}/${ROBOHERO_QT_WIN_SUBDIR}")

if(DEFINED ENV{QT_MINGW_ROOT} AND EXISTS "$ENV{QT_MINGW_ROOT}/include/QtCore/qconfig.h")
    set(ROBOHERO_QT_MINGW_ROOT "$ENV{QT_MINGW_ROOT}")
elseif(DEFINED ROBOHERO_QT_PREBUILT_ROOT AND EXISTS "${ROBOHERO_QT_PREBUILT_ROOT}/${ROBOHERO_QT_WIN_VERSION}/${ROBOHERO_QT_WIN_SUBDIR}/include/QtCore/qconfig.h")
    set(ROBOHERO_QT_MINGW_ROOT "${ROBOHERO_QT_PREBUILT_ROOT}/${ROBOHERO_QT_WIN_VERSION}/${ROBOHERO_QT_WIN_SUBDIR}")
elseif(EXISTS "${_SHARED_AIMON_QT_ROOT}/include/QtCore/qconfig.h")
    set(ROBOHERO_QT_MINGW_ROOT "${_SHARED_AIMON_QT_ROOT}")
else()
    if(NOT DEFINED ROBOHERO_QT_PREBUILT_ROOT)
        set(ROBOHERO_QT_PREBUILT_ROOT "${CMAKE_BINARY_DIR}/qt-prebuilt")
    endif()
    set(ROBOHERO_QT_MINGW_ROOT "${ROBOHERO_QT_PREBUILT_ROOT}/${ROBOHERO_QT_WIN_VERSION}/${ROBOHERO_QT_WIN_SUBDIR}")
endif()

if(NOT EXISTS "${ROBOHERO_QT_MINGW_ROOT}/include/QtCore/qconfig.h")
    find_program(AQT_EXECUTABLE NAMES aqt PATHS "$ENV{HOME}/.local/bin" NO_CMAKE_FIND_ROOT_PATH)
    if(NOT AQT_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        message(STATUS "Installing aqtinstall via pip...")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m pip install --user aqtinstall
            RESULT_VARIABLE _pip_rc
        )
        if(NOT _pip_rc EQUAL 0)
            message(FATAL_ERROR "Failed to install aqtinstall via pip")
        endif()
        find_program(AQT_EXECUTABLE NAMES aqt PATHS "$ENV{HOME}/.local/bin" NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    endif()

    file(MAKE_DIRECTORY "${ROBOHERO_QT_PREBUILT_ROOT}")
    message(STATUS "Downloading prebuilt Windows Qt ${ROBOHERO_QT_WIN_VERSION} (${ROBOHERO_QT_WIN_ARCH}) to ${ROBOHERO_QT_PREBUILT_ROOT}...")
    execute_process(
        COMMAND "${AQT_EXECUTABLE}" install-qt windows desktop
                "${ROBOHERO_QT_WIN_VERSION}" "${ROBOHERO_QT_WIN_ARCH}"
                -O "${ROBOHERO_QT_PREBUILT_ROOT}"
                --archives qtbase icu
        RESULT_VARIABLE _aqt_rc
    )
    if(NOT _aqt_rc EQUAL 0)
        message(FATAL_ERROR "aqt install-qt failed with exit code ${_aqt_rc}")
    endif()
endif()

set(ROBOHERO_QT_INCLUDE_DIR "${ROBOHERO_QT_MINGW_ROOT}/include")
set(ROBOHERO_QT_LIB_DIR "${ROBOHERO_QT_MINGW_ROOT}/lib")
set(ROBOHERO_QT_BIN_DIR "${ROBOHERO_QT_MINGW_ROOT}/bin")
set(ROBOHERO_QT_PLUGINS_DIR "${ROBOHERO_QT_MINGW_ROOT}/plugins")

set(ROBOHERO_QT_INCLUDE_DIRS
    "${ROBOHERO_QT_INCLUDE_DIR}"
    "${ROBOHERO_QT_INCLUDE_DIR}/QtCore"
    "${ROBOHERO_QT_INCLUDE_DIR}/QtGui"
    "${ROBOHERO_QT_INCLUDE_DIR}/QtWidgets"
    "${ROBOHERO_QT_INCLUDE_DIR}/QtOpenGL"
    "${ROBOHERO_QT_INCLUDE_DIR}/QtOpenGLWidgets"
)

set(ROBOHERO_QT_LIBRARIES
    "${ROBOHERO_QT_LIB_DIR}/libQt6OpenGLWidgets.a"
    "${ROBOHERO_QT_LIB_DIR}/libQt6OpenGL.a"
    "${ROBOHERO_QT_LIB_DIR}/libQt6Widgets.a"
    "${ROBOHERO_QT_LIB_DIR}/libQt6Gui.a"
    "${ROBOHERO_QT_LIB_DIR}/libQt6Core.a"
)

find_program(ROBOHERO_HOST_MOC_EXECUTABLE
    NAMES moc
    PATHS /usr/lib/qt6/libexec /usr/libexec/qt6
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_program(ROBOHERO_HOST_RCC_EXECUTABLE
    NAMES rcc
    PATHS /usr/lib/qt6/libexec /usr/libexec/qt6
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

if(NOT TARGET Qt6::Core)
    add_library(Qt6::Core UNKNOWN IMPORTED)
    set_target_properties(Qt6::Core PROPERTIES
        IMPORTED_LOCATION "${ROBOHERO_QT_LIB_DIR}/libQt6Core.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBOHERO_QT_INCLUDE_DIR};${ROBOHERO_QT_INCLUDE_DIR}/QtCore"
    )
endif()

if(NOT TARGET Qt6::Gui)
    add_library(Qt6::Gui UNKNOWN IMPORTED)
    set_target_properties(Qt6::Gui PROPERTIES
        IMPORTED_LOCATION "${ROBOHERO_QT_LIB_DIR}/libQt6Gui.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBOHERO_QT_INCLUDE_DIR};${ROBOHERO_QT_INCLUDE_DIR}/QtGui"
        INTERFACE_LINK_LIBRARIES Qt6::Core
    )
endif()

if(NOT TARGET Qt6::Widgets)
    add_library(Qt6::Widgets UNKNOWN IMPORTED)
    set_target_properties(Qt6::Widgets PROPERTIES
        IMPORTED_LOCATION "${ROBOHERO_QT_LIB_DIR}/libQt6Widgets.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBOHERO_QT_INCLUDE_DIR};${ROBOHERO_QT_INCLUDE_DIR}/QtWidgets"
        INTERFACE_LINK_LIBRARIES "Qt6::Gui;Qt6::Core"
    )
endif()

if(NOT TARGET Qt6::OpenGL)
    add_library(Qt6::OpenGL UNKNOWN IMPORTED)
    set_target_properties(Qt6::OpenGL PROPERTIES
        IMPORTED_LOCATION "${ROBOHERO_QT_LIB_DIR}/libQt6OpenGL.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBOHERO_QT_INCLUDE_DIR};${ROBOHERO_QT_INCLUDE_DIR}/QtOpenGL"
        INTERFACE_LINK_LIBRARIES "Qt6::Gui;Qt6::Core"
    )
endif()

if(NOT TARGET Qt6::OpenGLWidgets)
    add_library(Qt6::OpenGLWidgets UNKNOWN IMPORTED)
    set_target_properties(Qt6::OpenGLWidgets PROPERTIES
        IMPORTED_LOCATION "${ROBOHERO_QT_LIB_DIR}/libQt6OpenGLWidgets.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBOHERO_QT_INCLUDE_DIR};${ROBOHERO_QT_INCLUDE_DIR}/QtOpenGLWidgets"
        INTERFACE_LINK_LIBRARIES "Qt6::OpenGL;Qt6::Widgets;Qt6::Gui;Qt6::Core"
    )
endif()

function(robohero_qt_wrap_cpp out_var)
    set(_moc_files)
    foreach(_hdr IN LISTS ARGN)
        get_filename_component(_abs_hdr "${_hdr}" ABSOLUTE)
        get_filename_component(_hdr_we "${_hdr}" NAME_WE)
        set(_moc_out "${CMAKE_CURRENT_BINARY_DIR}/moc_${_hdr_we}.cxx")
        add_custom_command(
            OUTPUT "${_moc_out}"
            COMMAND "${ROBOHERO_HOST_MOC_EXECUTABLE}"
                "-I${ROBOHERO_QT_INCLUDE_DIR}"
                "-I${ROBOHERO_QT_INCLUDE_DIR}/QtCore"
                "-I${ROBOHERO_QT_INCLUDE_DIR}/QtGui"
                "-I${ROBOHERO_QT_INCLUDE_DIR}/QtWidgets"
                "-I${ROBOHERO_QT_INCLUDE_DIR}/QtOpenGL"
                "-I${ROBOHERO_QT_INCLUDE_DIR}/QtOpenGLWidgets"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/src"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/../common"
                "${_abs_hdr}"
                -o "${_moc_out}"
            DEPENDS "${_abs_hdr}"
            COMMENT "Running Qt6 moc on ${_hdr_we}.hxx"
            VERBATIM
        )
        list(APPEND _moc_files "${_moc_out}")
    endforeach()
    set(${out_var} ${${out_var}} ${_moc_files} PARENT_SCOPE)
endfunction()

function(robohero_qt_add_resources out_var qrc_file)
    get_filename_component(_abs_qrc "${qrc_file}" ABSOLUTE)
    get_filename_component(_qrc_we "${qrc_file}" NAME_WE)
    set(_rcc_out "${CMAKE_CURRENT_BINARY_DIR}/qrc_${_qrc_we}.cxx")
    add_custom_command(
        OUTPUT "${_rcc_out}"
        COMMAND "${ROBOHERO_HOST_RCC_EXECUTABLE}"
            --no-zstd
            --name "${_qrc_we}"
            "${_abs_qrc}"
            -o "${_rcc_out}"
        DEPENDS "${_abs_qrc}" ${ARGN}
        COMMENT "Running Qt6 rcc on ${_qrc_we}.qrc"
        VERBATIM
    )
    set(${out_var} ${${out_var}} "${_rcc_out}" PARENT_SCOPE)
endfunction()

function(robohero_stage_mingw_qt_runtime target_name)
    set(_WIN_QT_DLLS
        Qt6Core.dll
        Qt6Gui.dll
        Qt6Widgets.dll
        Qt6OpenGL.dll
        Qt6OpenGLWidgets.dll
    )
    set(_WIN_MINGW_DLLS
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
    )
    set(_WIN_PLATFORM_PLUGINS
        qwindows.dll
        qdirect2d.dll
        qminimal.dll
        qoffscreen.dll
    )

    set(_STAGED_FILES)
    set(_STAGE_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/platforms"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/styles"
    )

    foreach(_dll IN LISTS _WIN_QT_DLLS)
        list(APPEND _STAGED_FILES "${CMAKE_CURRENT_BINARY_DIR}/${_dll}")
        list(APPEND _STAGE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ROBOHERO_QT_BIN_DIR}/${_dll}"
                "${CMAKE_CURRENT_BINARY_DIR}/${_dll}"
        )
    endforeach()

    foreach(_dll IN LISTS _WIN_MINGW_DLLS)
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" "-print-file-name=${_dll}"
            OUTPUT_VARIABLE _MINGW_DLL_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT EXISTS "${_MINGW_DLL_PATH}")
            set(_MINGW_DLL_PATH "${ROBOHERO_QT_BIN_DIR}/${_dll}")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_MINGW_DLL_PATH}"
                "${CMAKE_CURRENT_BINARY_DIR}/${_dll}"
        )
        list(APPEND _STAGED_FILES "${CMAKE_CURRENT_BINARY_DIR}/${_dll}")
        list(APPEND _STAGE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_MINGW_DLL_PATH}"
                "${CMAKE_CURRENT_BINARY_DIR}/${_dll}"
        )
    endforeach()

    foreach(_plug IN LISTS _WIN_PLATFORM_PLUGINS)
        list(APPEND _STAGED_FILES "${CMAKE_CURRENT_BINARY_DIR}/platforms/${_plug}")
        list(APPEND _STAGE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ROBOHERO_QT_PLUGINS_DIR}/platforms/${_plug}"
                "${CMAKE_CURRENT_BINARY_DIR}/platforms/${_plug}"
        )
    endforeach()

    if(EXISTS "${ROBOHERO_QT_PLUGINS_DIR}/styles/qmodernwindowsstyle.dll")
        list(APPEND _STAGED_FILES "${CMAKE_CURRENT_BINARY_DIR}/styles/qmodernwindowsstyle.dll")
        list(APPEND _STAGE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ROBOHERO_QT_PLUGINS_DIR}/styles/qmodernwindowsstyle.dll"
                "${CMAKE_CURRENT_BINARY_DIR}/styles/qmodernwindowsstyle.dll"
        )
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        ${_STAGE_COMMANDS}
        COMMENT "Staging Qt6 and MinGW runtime DLLs and plugins alongside ${target_name}.exe"
    )

    set(ROBOHERO_STAGED_RUNTIME_FILES ${_STAGED_FILES} PARENT_SCOPE)
endfunction()

message(STATUS "Using prebuilt Windows Qt6 at: ${ROBOHERO_QT_MINGW_ROOT}")
