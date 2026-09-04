cmake_minimum_required(VERSION 3.20)
project(robohero_teleop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Third party directories
set(EIGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/eigen")
set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/onnxruntime")
set(PUGIXML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/pugixml")
set(OPENCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/opencv")
set(FIRMWARE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/include")

# RPATH configuration for standalone execution
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_BUILD_RPATH
    "${ONNXRUNTIME_DIR}/lib"
    "${OPENCV_DIR}/lib"
)

# Find Qt (Qt6 or Qt5)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find PkgConfig dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)

# Sources
set(TELEOP_SOURCES
    src/main.cxx
    src/MainWindow.cxx
    src/SettingsDialog.cxx
    src/CameraThread.cxx
    src/PoseEstimator.cxx
    src/PoseRetargeter.cxx
    src/UrdfViewerWidget.cxx
    src/UrdfLimits.cxx
    src/TeleopConfig.cxx
    src/MqttClient.cxx
    ${PUGIXML_DIR}/src/pugixml.cpp
    teleop_resources.qrc
)

add_executable(robohero_teleop ${TELEOP_SOURCES})

target_include_directories(robohero_teleop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FIRMWARE_INC_DIR}
    ${EIGEN_DIR}
    ${PUGIXML_DIR}/src
    ${ONNXRUNTIME_DIR}/include
    ${OPENCV_DIR}/include/opencv4
    ${MOSQUITTO_INCLUDE_DIRS}
    ${LIBCONFIGPP_INCLUDE_DIRS}
)

target_link_directories(robohero_teleop PRIVATE
    ${ONNXRUNTIME_DIR}/lib
    ${OPENCV_DIR}/lib
    ${MOSQUITTO_LIBRARY_DIRS}
    ${LIBCONFIGPP_LIBRARY_DIRS}
)

target_link_libraries(robohero_teleop PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    opencv_core
    opencv_imgproc
    opencv_videoio
    tbb
    OpenGL::GL
    OpenGL::GLU
    ${MOSQUITTO_LIBRARIES}
    ${LIBCONFIGPP_LIBRARIES}
    onnxruntime
)

target_link_options(robohero_teleop PRIVATE "-Wl,--allow-shlib-undefined" "-Wl,--disable-new-dtags")

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(robohero_teleop PRIVATE Qt6::OpenGLWidgets)
endif()
