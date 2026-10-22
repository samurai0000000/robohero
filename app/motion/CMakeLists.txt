#
# CMakeLists.txt
#
# Copyright (C) 2026, Charles Chiou
#

cmake_minimum_required(VERSION 3.20)

set(_ROOT_README_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../README.md")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_ROOT_README_FILE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/Version.hxx.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/cmake/FetchPrebuiltQtWindows.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/robohero_motion.rc.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/installer/robohero_motion.nsi.in"
)
file(READ "${_ROOT_README_FILE}" _ROOT_README_CONTENT)
string(REGEX MATCH "<!-- robohero-version:[ \t]*([0-9]+\\.[0-9]+\\.[0-9]+)[ \t]*-->" _VER_MATCH "${_ROOT_README_CONTENT}")
if(NOT _VER_MATCH)
    message(FATAL_ERROR "Could not extract <!-- robohero-version: x.y.z --> from ${_ROOT_README_FILE}")
endif()
set(PROJ_VER "${CMAKE_MATCH_1}")

project(robohero_motion VERSION ${PROJ_VER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build robohero_motion CppUTest suite" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(ENABLE_SANITIZERS AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/Version.hxx.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.hxx"
    @ONLY
)

set(EIGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/eigen")
set(FIRMWARE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/include")
set(PUGIXML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/pugixml")
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common")

set(ROBOHERO_SELFUPDATE_CMAKE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../selfupdate/windows/selfupdate.cmake"
    CACHE FILEPATH "Path to selfupdate/windows/selfupdate.cmake"
)
if(NOT EXISTS "${ROBOHERO_SELFUPDATE_CMAKE}")
    message(FATAL_ERROR "selfupdate CMake module not found at ${ROBOHERO_SELFUPDATE_CMAKE}")
endif()
include("${ROBOHERO_SELFUPDATE_CMAKE}")

set(MOTION_LIB_SOURCES
    src/MainWindow.cxx
    src/UrdfLimits.cxx
    src/UrdfViewerWidget.cxx
    src/MqttClient.cxx
    src/MotionSequence.cxx
    src/MotionLibrary.cxx
    src/MotionLibraryWidget.cxx
    src/MotionPlayer.cxx
    src/ParametricGait.cxx
    src/TimelineWidget.cxx
    src/SettingsDialog.cxx
    ${COMMON_DIR}/SelfUpdateDialog.cxx
)

set(MOTION_QOBJECT_HEADERS
    src/MainWindow.hxx
    src/UrdfViewerWidget.hxx
    src/TimelineWidget.hxx
    src/MotionLibraryWidget.hxx
    src/MotionPlayer.hxx
    src/SettingsDialog.hxx
    src/MqttClient.hxx
    ${COMMON_DIR}/SelfUpdateDialog.hxx
)

set_property(SOURCE motion_resources.qrc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
)

if(MSVC)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchPrebuiltWindows.cmake")

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    endif()
    find_package(OpenGL REQUIRED)
    find_package(unofficial-mosquitto CONFIG REQUIRED)
    find_package(pugixml CONFIG REQUIRED)

    find_package(unofficial-libconfig CONFIG QUIET)
    if(unofficial-libconfig_FOUND)
        set(MOTION_LIBCONFIG_TARGET unofficial::libconfig::libconfig++)
    else()
        find_package(libconfig CONFIG QUIET)
        find_package(libconfig++ CONFIG QUIET)
        if(TARGET libconfig::libconfig++)
            set(MOTION_LIBCONFIG_TARGET libconfig::libconfig++)
        elseif(TARGET libconfig++::libconfig++)
            set(MOTION_LIBCONFIG_TARGET libconfig++::libconfig++)
        else()
            message(FATAL_ERROR "libconfig++ CMake target not found")
        endif()
    endif()

    enable_language(RC)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/robohero_motion.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.rc"
        @ONLY
    )

    add_library(robohero_motion_lib STATIC
        ${MOTION_LIB_SOURCES}
        ${MOTION_QOBJECT_HEADERS}
        motion_resources.qrc
    )
    selfupdate_target_sources(robohero_motion_lib)

    target_include_directories(robohero_motion_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
    )

    target_compile_definitions(robohero_motion_lib PUBLIC
        _USE_MATH_DEFINES
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    target_link_libraries(robohero_motion_lib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        OpenGL::GL
        OpenGL::GLU
        unofficial::mosquitto::mosquitto
        ${MOTION_LIBCONFIG_TARGET}
        pugixml::pugixml
        shell32
    )
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(robohero_motion_lib PUBLIC Qt6::OpenGLWidgets)
    endif()

    add_executable(robohero_motion WIN32
        src/main.cxx
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.rc"
    )
    target_link_libraries(robohero_motion PRIVATE robohero_motion_lib)

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../tools/Qt6/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET robohero_motion POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-compiler-runtime
                "$<TARGET_FILE_DIR:robohero_motion>"
            COMMENT "Deploying Qt runtime with windeployqt"
        )
    endif()
elseif(MINGW)
    include("${COMMON_DIR}/cmake/FetchPrebuiltQtWindows.cmake")

    add_compile_options(-D_GLIBCXX_USE_INIT_PRIORITY_ATTRIBUTE=0)

    set(MOTION_MINGW_GENERATED_SOURCES)
    robohero_qt_wrap_cpp(MOTION_MINGW_GENERATED_SOURCES ${MOTION_QOBJECT_HEADERS})
    robohero_qt_add_resources(MOTION_MINGW_GENERATED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/motion_resources.qrc"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
    )

    add_library(robohero_motion_lib STATIC
        ${MOTION_LIB_SOURCES}
        ${COMMON_DIR}/mingw_compat/WinMosquittoCompat.cxx
        ${PUGIXML_DIR}/src/pugixml.cpp
        ${MOTION_MINGW_GENERATED_SOURCES}
    )
    selfupdate_target_sources(robohero_motion_lib)

    target_include_directories(robohero_motion_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}
        ${COMMON_DIR}/mingw_compat
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${PUGIXML_DIR}/src
    )

    target_include_directories(robohero_motion_lib SYSTEM PUBLIC
        ${ROBOHERO_QT_INCLUDE_DIRS}
    )

    target_link_libraries(robohero_motion_lib PUBLIC
        ${ROBOHERO_QT_LIBRARIES}
        opengl32
        glu32
        ws2_32
        winhttp
        version
        shell32
        ole32
        advapi32
        user32
        gdi32
    )

    if(NOT CMAKE_RC_COMPILER)
        set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    endif()
    enable_language(RC)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/robohero_motion.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.rc"
        @ONLY
    )

    set_source_files_properties(
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.rc"
        PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/robohero_icon.ico"
    )

    add_executable(robohero_motion WIN32
        src/main.cxx
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.rc"
    )

    target_link_libraries(robohero_motion PRIVATE
        robohero_motion_lib
    )

    robohero_stage_mingw_qt_runtime(robohero_motion)

    find_program(MAKENSIS_EXECUTABLE NAMES makensis NO_CMAKE_FIND_ROOT_PATH REQUIRED)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/robohero_motion.nsi.in"
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.nsi"
        @ONLY
    )

    set(ROBOHERO_MOTION_INSTALLER_EXE
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion-setup-${PROJECT_VERSION}.exe"
    )

    add_custom_command(
        OUTPUT "${ROBOHERO_MOTION_INSTALLER_EXE}"
        COMMAND "${CMAKE_COMMAND}" -E remove -f "${ROBOHERO_MOTION_INSTALLER_EXE}"
        COMMAND "${MAKENSIS_EXECUTABLE}" -V2 "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.nsi"
        DEPENDS
            robohero_motion
            "${CMAKE_CURRENT_BINARY_DIR}/robohero_motion.nsi"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/robohero_icon.ico"
            "${_ROOT_README_FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Building Windows NSIS installer robohero_motion-setup-${PROJECT_VERSION}.exe"
        VERBATIM
    )

    add_custom_target(installer ALL
        DEPENDS "${ROBOHERO_MOTION_INSTALLER_EXE}"
    )
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    endif()
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
    pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)

    add_library(robohero_motion_lib STATIC
        ${MOTION_LIB_SOURCES}
        ${MOTION_QOBJECT_HEADERS}
        ${PUGIXML_DIR}/src/pugixml.cpp
        motion_resources.qrc
    )
    selfupdate_target_sources(robohero_motion_lib)

    target_include_directories(robohero_motion_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${PUGIXML_DIR}/src
        ${MOSQUITTO_INCLUDE_DIRS}
        ${LIBCONFIGPP_INCLUDE_DIRS}
    )

    target_link_directories(robohero_motion_lib PUBLIC
        ${MOSQUITTO_LIBRARY_DIRS}
        ${LIBCONFIGPP_LIBRARY_DIRS}
    )

    target_link_libraries(robohero_motion_lib PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        OpenGL::GL
        OpenGL::GLU
        ${MOSQUITTO_LIBRARIES}
        ${LIBCONFIGPP_LIBRARIES}
    )
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(robohero_motion_lib PUBLIC Qt6::OpenGLWidgets)
    endif()

    add_executable(robohero_motion
        src/main.cxx
    )
    target_link_libraries(robohero_motion PRIVATE robohero_motion_lib)

    if(BUILD_TESTING)
        enable_testing()
        pkg_check_modules(CPPUTEST REQUIRED cpputest)

        add_executable(test_motion_selfupdate
            test/TestMotionSelfUpdate.cxx
        )
        target_compile_definitions(test_motion_selfupdate PRIVATE
            CPPUTEST_MEM_LEAK_DETECTION_DISABLED
        )
        target_include_directories(test_motion_selfupdate PRIVATE
            ${CPPUTEST_INCLUDE_DIRS}
        )
        target_link_directories(test_motion_selfupdate PRIVATE
            ${CPPUTEST_LIBRARY_DIRS}
        )
        target_link_libraries(test_motion_selfupdate PRIVATE
            robohero_motion_lib
            ${CPPUTEST_LIBRARIES}
        )
        add_test(NAME test_motion_selfupdate COMMAND test_motion_selfupdate)
    endif()
endif()
