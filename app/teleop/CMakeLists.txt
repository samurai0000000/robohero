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
    "${CMAKE_CURRENT_SOURCE_DIR}/robohero_teleop.rc.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/installer/robohero_teleop.nsi.in"
)
file(READ "${_ROOT_README_FILE}" _ROOT_README_CONTENT)
string(REGEX MATCH "<!-- robohero-version:[ \t]*([0-9]+\\.[0-9]+\\.[0-9]+)[ \t]*-->" _VER_MATCH "${_ROOT_README_CONTENT}")
if(NOT _VER_MATCH)
    message(FATAL_ERROR "Could not extract <!-- robohero-version: x.y.z --> from ${_ROOT_README_FILE}")
endif()
set(PROJ_VER "${CMAKE_MATCH_1}")

project(robohero_teleop VERSION ${PROJ_VER} LANGUAGES C CXX)

math(EXPR ROBOHERO_VERSION_CODE
    "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build robohero_teleop CppUTest suite" OFF)
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

set(TELEOP_LIB_SOURCES
    src/MainWindow.cxx
    src/SettingsDialog.cxx
    src/CameraThread.cxx
    src/PoseEstimator.cxx
    src/PoseRetargeter.cxx
    src/UrdfViewerWidget.cxx
    src/UrdfLimits.cxx
    src/TeleopConfig.cxx
    src/MqttClient.cxx
    ${COMMON_DIR}/SelfUpdateDialog.cxx
)

set(TELEOP_QOBJECT_HEADERS
    src/MainWindow.hxx
    src/SettingsDialog.hxx
    src/CameraThread.hxx
    src/UrdfViewerWidget.hxx
    src/MqttClient.hxx
    ${COMMON_DIR}/SelfUpdateDialog.hxx
)

set_property(SOURCE teleop_resources.qrc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/yolov8n-pose.onnx"
)

if(MSVC)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    # Windows MSVC: official Qt/OpenCV binaries. Do not add them to vcpkg.json.
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchPrebuiltWindows.cmake")

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    endif()
    find_package(OpenGL REQUIRED)
    find_package(OpenCV REQUIRED)
    find_package(unofficial-mosquitto CONFIG REQUIRED)
    find_package(pugixml CONFIG REQUIRED)

    # Official DirectML-enabled ONNX Runtime
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

    enable_language(RC)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/robohero_teleop.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.rc"
        @ONLY
    )

    add_library(robohero_teleop_lib STATIC
        ${TELEOP_LIB_SOURCES}
        ${TELEOP_QOBJECT_HEADERS}
        teleop_resources.qrc
    )
    selfupdate_target_sources(robohero_teleop_lib)

    target_include_directories(robohero_teleop_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )

    target_compile_definitions(robohero_teleop_lib PUBLIC
        _USE_MATH_DEFINES
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    target_link_libraries(robohero_teleop_lib PUBLIC
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
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(robohero_teleop_lib PUBLIC Qt6::OpenGLWidgets)
    endif()

    add_executable(robohero_teleop WIN32
        src/main.cxx
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.rc"
    )
    target_link_libraries(robohero_teleop PRIVATE robohero_teleop_lib)

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../tools/Qt6/bin"
    )
    add_custom_command(TARGET robohero_teleop POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
            "$<TARGET_FILE_DIR:robohero_teleop>"
        COMMENT "Copying ONNX Runtime DirectML DLLs"
    )
    if(EXISTS "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll")
        add_custom_command(TARGET robohero_teleop POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll"
                "$<TARGET_FILE_DIR:robohero_teleop>"
        )
    endif()
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
elseif(MINGW)
    include("${COMMON_DIR}/cmake/FetchPrebuiltQtWindows.cmake")

    add_compile_options(
        -D_GLIBCXX_USE_INIT_PRIORITY_ATTRIBUTE=0
        -D_stdcall=__stdcall
        -D_Frees_ptr_opt_=
    )

    # Retrieve official ONNX Runtime DirectML NuGet package
    set(ORT_DML_VERSION "1.23.0")
    set(ORT_DML_ROOT "${CMAKE_BINARY_DIR}/_deps/onnxruntime-dml")
    set(ORT_DML_NUPKG "${CMAKE_BINARY_DIR}/Microsoft.ML.OnnxRuntime.DirectML.${ORT_DML_VERSION}.nupkg")
    set(ORT_DML_LIB "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime.lib")
    if(NOT EXISTS "${ORT_DML_LIB}")
        message(STATUS "Downloading ONNX Runtime DirectML ${ORT_DML_VERSION} for MinGW-w64 cross-build...")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/${ORT_DML_VERSION}"
            "${ORT_DML_NUPKG}"
            SHOW_PROGRESS
            STATUS ORT_DL_STATUS
        )
        list(GET ORT_DL_STATUS 0 ORT_DL_CODE)
        if(NOT ORT_DL_CODE EQUAL 0)
            file(REMOVE "${ORT_DML_NUPKG}")
            message(FATAL_ERROR "Failed to download ONNX Runtime DirectML: ${ORT_DL_STATUS}")
        endif()
        file(MAKE_DIRECTORY "${ORT_DML_ROOT}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xzf "${ORT_DML_NUPKG}"
            WORKING_DIRECTORY "${ORT_DML_ROOT}"
            RESULT_VARIABLE _ort_extract_rc
        )
        if(NOT _ort_extract_rc EQUAL 0 OR NOT EXISTS "${ORT_DML_LIB}")
            message(FATAL_ERROR "Failed to extract ONNX Runtime DirectML nupkg (rc=${_ort_extract_rc})")
        endif()
    endif()

    set(TELEOP_MINGW_GENERATED_SOURCES)
    robohero_qt_wrap_cpp(TELEOP_MINGW_GENERATED_SOURCES ${TELEOP_QOBJECT_HEADERS})
    robohero_qt_add_resources(TELEOP_MINGW_GENERATED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/teleop_resources.qrc"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/yolov8n-pose.onnx"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/robohero_icon.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/robohero_head.png"
    )

    add_library(robohero_teleop_lib STATIC
        ${TELEOP_LIB_SOURCES}
        ${COMMON_DIR}/mingw_compat/WinMosquittoCompat.cxx
        src/mingw_compat/MinGwLibconfigCompat.cxx
        src/mingw_compat/MinGwOpenCvCompat.cxx
        ${PUGIXML_DIR}/src/pugixml.cpp
        ${TELEOP_MINGW_GENERATED_SOURCES}
    )
    selfupdate_target_sources(robohero_teleop_lib)

    target_include_directories(robohero_teleop_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mingw_compat
        ${COMMON_DIR}
        ${COMMON_DIR}/mingw_compat
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${PUGIXML_DIR}/src
    )

    target_include_directories(robohero_teleop_lib SYSTEM PUBLIC
        ${ROBOHERO_QT_INCLUDE_DIRS}
        "${ORT_DML_ROOT}/build/native/include"
    )

    target_link_libraries(robohero_teleop_lib PUBLIC
        ${ROBOHERO_QT_LIBRARIES}
        "${ORT_DML_LIB}"
        opengl32
        glu32
        vfw32
        avicap32
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
        "${CMAKE_CURRENT_SOURCE_DIR}/robohero_teleop.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.rc"
        @ONLY
    )

    set_source_files_properties(
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.rc"
        PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/robohero_icon.ico"
    )

    add_executable(robohero_teleop WIN32
        src/main.cxx
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.rc"
    )

    target_link_libraries(robohero_teleop PRIVATE
        robohero_teleop_lib
    )

    robohero_stage_mingw_qt_runtime(robohero_teleop)

    add_custom_command(TARGET robohero_teleop POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/onnxruntime.dll"
        COMMENT "Staging ONNX Runtime DirectML DLL alongside robohero_teleop.exe"
    )
    if(EXISTS "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll")
        add_custom_command(TARGET robohero_teleop POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ORT_DML_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll"
                "${CMAKE_CURRENT_BINARY_DIR}/onnxruntime_providers_shared.dll"
        )
    endif()

    find_program(MAKENSIS_EXECUTABLE NAMES makensis NO_CMAKE_FIND_ROOT_PATH REQUIRED)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/robohero_teleop.nsi.in"
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.nsi"
        @ONLY
    )

    set(ROBOHERO_TELEOP_INSTALLER_EXE
        "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop-setup-${PROJECT_VERSION}.exe"
    )

    add_custom_command(
        OUTPUT "${ROBOHERO_TELEOP_INSTALLER_EXE}"
        COMMAND "${CMAKE_COMMAND}" -E remove -f "${ROBOHERO_TELEOP_INSTALLER_EXE}"
        COMMAND "${MAKENSIS_EXECUTABLE}" -V2 "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.nsi"
        DEPENDS
            robohero_teleop
            "${CMAKE_CURRENT_BINARY_DIR}/robohero_teleop.nsi"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/robohero_icon.ico"
            "${_ROOT_README_FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Building Windows NSIS installer robohero_teleop-setup-${PROJECT_VERSION}.exe"
        VERBATIM
    )

    add_custom_target(installer ALL
        DEPENDS "${ROBOHERO_TELEOP_INSTALLER_EXE}"
    )
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/onnxruntime")
    set(OPENCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/opencv")

    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH
        "${ONNXRUNTIME_DIR}/lib"
        "${OPENCV_DIR}/lib"
    )

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    endif()
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
    pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)

    add_library(robohero_teleop_lib STATIC
        ${TELEOP_LIB_SOURCES}
        ${TELEOP_QOBJECT_HEADERS}
        ${PUGIXML_DIR}/src/pugixml.cpp
        teleop_resources.qrc
    )
    selfupdate_target_sources(robohero_teleop_lib)

    target_include_directories(robohero_teleop_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}
        ${FIRMWARE_INC_DIR}
        ${EIGEN_DIR}
        ${PUGIXML_DIR}/src
        ${ONNXRUNTIME_DIR}/include
        ${OPENCV_DIR}/include/opencv4
        ${MOSQUITTO_INCLUDE_DIRS}
        ${LIBCONFIGPP_INCLUDE_DIRS}
    )

    target_link_directories(robohero_teleop_lib PUBLIC
        ${ONNXRUNTIME_DIR}/lib
        ${OPENCV_DIR}/lib
        ${MOSQUITTO_LIBRARY_DIRS}
        ${LIBCONFIGPP_LIBRARY_DIRS}
    )

    target_link_libraries(robohero_teleop_lib PUBLIC
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
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(robohero_teleop_lib PUBLIC Qt6::OpenGLWidgets)
    endif()

    target_link_options(robohero_teleop_lib PUBLIC "-Wl,--allow-shlib-undefined" "-Wl,--disable-new-dtags")

    add_executable(robohero_teleop
        src/main.cxx
    )
    target_link_libraries(robohero_teleop PRIVATE robohero_teleop_lib)

    if(BUILD_TESTING)
        enable_testing()
        pkg_check_modules(CPPUTEST REQUIRED cpputest)
        find_package(rapidcheck CONFIG QUIET)
        if(TARGET rapidcheck)
            set(RAPIDCHECK_LIBRARIES rapidcheck)
        else()
            find_library(RAPIDCHECK_LIBRARIES NAMES rapidcheck REQUIRED)
        endif()

        add_executable(test_teleop_selfupdate
            test/TestTeleopSelfUpdate.cxx
        )
        target_compile_definitions(test_teleop_selfupdate PRIVATE
            CPPUTEST_MEM_LEAK_DETECTION_DISABLED
        )
        target_include_directories(test_teleop_selfupdate PRIVATE
            ${CPPUTEST_INCLUDE_DIRS}
        )
        target_link_directories(test_teleop_selfupdate PRIVATE
            ${CPPUTEST_LIBRARY_DIRS}
        )
        target_link_libraries(test_teleop_selfupdate PRIVATE
            robohero_teleop_lib
            ${CPPUTEST_LIBRARIES}
            ${RAPIDCHECK_LIBRARIES}
        )
        add_test(NAME test_teleop_selfupdate COMMAND test_teleop_selfupdate)
    endif()
endif()
