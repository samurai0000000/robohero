cmake_minimum_required(VERSION 3.20)
project(robohero_motion VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EIGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/eigen")
set(FIRMWARE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/include")

# Windows: official Qt binaries via cmake/FetchPrebuiltWindows.cmake
if(WIN32)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchPrebuiltWindows.cmake")
endif()

# Find Qt (Qt6 or Qt5)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
endif()

find_package(OpenGL REQUIRED)

set(MOTION_SOURCES
    src/main.cxx
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
    motion_resources.qrc
)

set_property(SOURCE motion_resources.qrc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
)

if(WIN32)
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

    add_executable(robohero_motion WIN32 ${MOTION_SOURCES})
    target_sources(robohero_motion PRIVATE robohero_motion.rc)

    target_include_directories(robohero_motion PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
    )

    target_link_libraries(robohero_motion PRIVATE
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
        target_link_libraries(robohero_motion PRIVATE Qt6::OpenGLWidgets)
    endif()

    if(MSVC)
        target_compile_definitions(robohero_motion PRIVATE
            _USE_MATH_DEFINES
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    endif()

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
else()
    set(PUGIXML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/pugixml")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
    pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)

    add_executable(robohero_motion
        ${MOTION_SOURCES}
        ${PUGIXML_DIR}/src/pugixml.cpp
    )

    target_include_directories(robohero_motion PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${PUGIXML_DIR}/src
        ${MOSQUITTO_INCLUDE_DIRS}
        ${LIBCONFIGPP_INCLUDE_DIRS}
    )

    target_link_directories(robohero_motion PRIVATE
        ${MOSQUITTO_LIBRARY_DIRS}
        ${LIBCONFIGPP_LIBRARY_DIRS}
    )

    target_link_libraries(robohero_motion PRIVATE
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
        target_link_libraries(robohero_motion PRIVATE Qt6::OpenGLWidgets)
    endif()
endif()
