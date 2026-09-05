cmake_minimum_required(VERSION 3.20)
project(robohero_teleop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EIGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/eigen")
set(FIRMWARE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/include")

# Windows: official Qt/OpenCV binaries. Do not add them to vcpkg.json.
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
    teleop_resources.qrc
)

set_property(SOURCE teleop_resources.qrc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
)

if(WIN32)
    find_package(OpenCV REQUIRED)
    find_package(unofficial-mosquitto CONFIG REQUIRED)
    find_package(pugixml CONFIG REQUIRED)

    # Official DirectML-enabled ONNX Runtime (vcpkg onnxruntime[directml]
    # fails to build against the current vcpkg flatbuffers).
    set(ORT_DML_VERSION "1.23.0")
    set(ORT_DML_ROOT "${CMAKE_BINARY_DIR}/onnxruntime-directml-${ORT_DML_VERSION}")
    set(ORT_DML_NUPKG "${CMAKE_BINARY_DIR}/Microsoft.ML.OnnxRuntime.DirectML.${ORT_DML_VERSION}.nupkg")
    set(ORT_DML_LIB "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime.lib")
    if(NOT EXISTS "${ORT_DML_LIB}")
        message(STATUS "Downloading ONNX Runtime DirectML ${ORT_DML_VERSION}")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/${ORT_DML_VERSION}"
            "${ORT_DML_NUPKG}"
            SHOW_PROGRESS
            STATUS ORT_DL_STATUS
        )
        list(GET ORT_DL_STATUS 0 ORT_DL_CODE)
        if(NOT ORT_DL_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ONNX Runtime DirectML: ${ORT_DL_STATUS}")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${ORT_DML_NUPKG}" DESTINATION "${ORT_DML_ROOT}")
    endif()
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_IMPLIB "${ORT_DML_LIB}"
        IMPORTED_LOCATION "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${ORT_DML_ROOT}/build/native/include"
    )

    find_package(unofficial-libconfig CONFIG QUIET)
    if(unofficial-libconfig_FOUND)
        set(TELEOP_LIBCONFIG_TARGET unofficial::libconfig::libconfig++)
    else()
        find_package(libconfig CONFIG QUIET)
        find_package(libconfig++ CONFIG QUIET)
        if(TARGET libconfig::libconfig++)
            set(TELEOP_LIBCONFIG_TARGET libconfig::libconfig++)
        elseif(TARGET libconfig++::libconfig++)
            set(TELEOP_LIBCONFIG_TARGET libconfig++::libconfig++)
        else()
            message(FATAL_ERROR "libconfig++ CMake target not found")
        endif()
    endif()

    add_executable(robohero_teleop WIN32 ${TELEOP_SOURCES})
    target_sources(robohero_teleop PRIVATE robohero_teleop.rc)

    target_include_directories(robohero_teleop PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(robohero_teleop PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        ${OpenCV_LIBS}
        OpenGL::GL
        OpenGL::GLU
        unofficial::mosquitto::mosquitto
        ${TELEOP_LIBCONFIG_TARGET}
        onnxruntime
        pugixml::pugixml
        shell32
    )

    if(MSVC)
        target_compile_definitions(robohero_teleop PRIVATE
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
    add_custom_command(TARGET robohero_teleop POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
            "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:robohero_teleop>"
        COMMENT "Copying ONNX Runtime DirectML DLLs"
    )
    if(OpenCV_LIB_PATH)
        get_filename_component(TELEOP_OPENCV_BIN "${OpenCV_LIB_PATH}/../bin" ABSOLUTE)
        file(GLOB TELEOP_OPENCV_DLLS "${TELEOP_OPENCV_BIN}/*.dll")
        if(TELEOP_OPENCV_DLLS)
            add_custom_command(TARGET robohero_teleop POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    ${TELEOP_OPENCV_DLLS}
                    "$<TARGET_FILE_DIR:robohero_teleop>"
                COMMENT "Copying OpenCV prebuilt DLLs"
            )
        endif()
    endif()
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET robohero_teleop POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-compiler-runtime
                "$<TARGET_FILE:robohero_teleop>"
            COMMENT "Deploying Qt runtime with windeployqt"
        )
    endif()
else()
    set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/onnxruntime")
    set(PUGIXML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/pugixml")
    set(OPENCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/opencv")

    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH
        "${ONNXRUNTIME_DIR}/lib"
        "${OPENCV_DIR}/lib"
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
    pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)

    add_executable(robohero_teleop
        ${TELEOP_SOURCES}
        ${PUGIXML_DIR}/src/pugixml.cpp
    )

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
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(robohero_teleop PRIVATE Qt6::OpenGLWidgets)
endif()
