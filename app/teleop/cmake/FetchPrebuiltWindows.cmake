# Official Windows binaries for Qt and OpenCV. Never build these from source.
# Override with -DTELEOP_QT_PREFIX=... and/or -DOpenCV_DIR=...

set(TELEOP_QT_VERSION "6.8.3")
set(TELEOP_OPENCV_VERSION "4.12.0")

function(teleop_download_file url dest)
    if(EXISTS "${dest}")
        return()
    endif()
    message(STATUS "Downloading ${url}")
    file(DOWNLOAD "${url}" "${dest}" SHOW_PROGRESS STATUS _st TIMEOUT 600)
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        file(REMOVE "${dest}")
        message(FATAL_ERROR "Download failed: ${_st}")
    endif()
endfunction()

# --- OpenCV official Windows pack (self-extracting 7-Zip) ---
if(NOT OpenCV_DIR)
    set(_cv_root "${CMAKE_BINARY_DIR}/opencv-prebuilt/${TELEOP_OPENCV_VERSION}")
    set(_cv_cfg "${_cv_root}/opencv/build/OpenCVConfig.cmake")
    if(NOT EXISTS "${_cv_cfg}")
        set(_cv_exe "${CMAKE_BINARY_DIR}/opencv-${TELEOP_OPENCV_VERSION}-windows.exe")
        teleop_download_file(
            "https://github.com/opencv/opencv/releases/download/${TELEOP_OPENCV_VERSION}/opencv-${TELEOP_OPENCV_VERSION}-windows.exe"
            "${_cv_exe}"
        )
        file(MAKE_DIRECTORY "${_cv_root}")
        message(STATUS "Extracting OpenCV ${TELEOP_OPENCV_VERSION} official Windows binaries")
        execute_process(
            COMMAND "${_cv_exe}" -y "-o${_cv_root}"
            RESULT_VARIABLE _cv_ex
        )
        if(NOT _cv_ex EQUAL 0 OR NOT EXISTS "${_cv_cfg}")
            message(FATAL_ERROR "Failed to extract OpenCV prebuilts (exit ${_cv_ex})")
        endif()
    endif()
    set(OpenCV_DIR "${_cv_root}/opencv/build" CACHE PATH "Official OpenCV Windows prebuilts" FORCE)
endif()
# Official pack only ships vc16. VS 2022/2026 MSVC versions are not in
# OpenCVConfig's runtime map, so pin the shipped binaries explicitly.
file(GLOB _cv_runtimes LIST_DIRECTORIES true "${OpenCV_DIR}/x64/vc*")
if(_cv_runtimes)
    list(SORT _cv_runtimes)
    list(GET _cv_runtimes -1 _cv_runtime_dir)
    get_filename_component(_cv_runtime "${_cv_runtime_dir}" NAME)
    set(OpenCV_ARCH x64 CACHE STRING "OpenCV official Windows pack architecture" FORCE)
    set(OpenCV_RUNTIME "${_cv_runtime}" CACHE STRING "OpenCV official Windows pack runtime" FORCE)
endif()
message(STATUS "Using prebuilt OpenCV: ${OpenCV_DIR} (${OpenCV_ARCH}/${OpenCV_RUNTIME})")

# --- Qt official MSVC binaries via aqtinstall ---
if(NOT TELEOP_QT_PREFIX)
    set(_qt_prefix "${CMAKE_BINARY_DIR}/qt-prebuilt/${TELEOP_QT_VERSION}/msvc2022_64")
    if(NOT EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
        find_program(TELEOP_PYTHON NAMES python py)
        if(NOT TELEOP_PYTHON)
            message(FATAL_ERROR "Python is required to download official Qt binaries (aqtinstall)")
        endif()
        execute_process(
            COMMAND "${TELEOP_PYTHON}" -c "import aqt"
            RESULT_VARIABLE _aqt_ok
            OUTPUT_QUIET ERROR_QUIET
        )
        if(NOT _aqt_ok EQUAL 0)
            message(STATUS "Installing aqtinstall (download official Qt, do not compile it)")
            execute_process(
                COMMAND "${TELEOP_PYTHON}" -m pip install --user aqtinstall
                RESULT_VARIABLE _pip_st
            )
            if(NOT _pip_st EQUAL 0)
                message(FATAL_ERROR "pip install aqtinstall failed")
            endif()
        endif()
        set(_qt_out "${CMAKE_BINARY_DIR}/qt-prebuilt")
        file(MAKE_DIRECTORY "${_qt_out}")
        message(STATUS "Downloading official Qt ${TELEOP_QT_VERSION} win64_msvc2022_64 (qtbase only)")
        execute_process(
            COMMAND "${TELEOP_PYTHON}" -m aqt install-qt windows desktop
                    "${TELEOP_QT_VERSION}" win64_msvc2022_64
                    --outputdir "${_qt_out}"
                    --archives qtbase
            RESULT_VARIABLE _qt_st
        )
        if(NOT _qt_st EQUAL 0 OR NOT EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
            message(FATAL_ERROR "aqtinstall of Qt ${TELEOP_QT_VERSION} failed (exit ${_qt_st})")
        endif()
    endif()
    set(TELEOP_QT_PREFIX "${_qt_prefix}" CACHE PATH "Official Qt MSVC prebuilts" FORCE)
endif()
list(PREPEND CMAKE_PREFIX_PATH "${TELEOP_QT_PREFIX}")
set(Qt6_DIR "${TELEOP_QT_PREFIX}/lib/cmake/Qt6" CACHE PATH "Qt6 CMake config" FORCE)
message(STATUS "Using prebuilt Qt: ${TELEOP_QT_PREFIX}")
