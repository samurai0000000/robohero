# Official Windows binaries for Qt. Never build Qt from source.
# Override with -DMOTION_QT_PREFIX=...

set(MOTION_QT_VERSION "6.8.3")

# --- Qt official MSVC binaries via aqtinstall ---
if(NOT MOTION_QT_PREFIX)
    set(_qt_prefix "${CMAKE_BINARY_DIR}/qt-prebuilt/${MOTION_QT_VERSION}/msvc2022_64")
    if(NOT EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
        find_program(MOTION_PYTHON NAMES python py)
        if(NOT MOTION_PYTHON)
            message(FATAL_ERROR "Python is required to download official Qt binaries (aqtinstall)")
        endif()
        execute_process(
            COMMAND "${MOTION_PYTHON}" -c "import aqt"
            RESULT_VARIABLE _aqt_ok
            OUTPUT_QUIET ERROR_QUIET
        )
        if(NOT _aqt_ok EQUAL 0)
            message(STATUS "Installing aqtinstall (download official Qt, do not compile it)")
            execute_process(
                COMMAND "${MOTION_PYTHON}" -m pip install --user aqtinstall
                RESULT_VARIABLE _pip_st
            )
            if(NOT _pip_st EQUAL 0)
                message(FATAL_ERROR "pip install aqtinstall failed")
            endif()
        endif()
        set(_qt_out "${CMAKE_BINARY_DIR}/qt-prebuilt")
        file(MAKE_DIRECTORY "${_qt_out}")
        message(STATUS "Downloading official Qt ${MOTION_QT_VERSION} win64_msvc2022_64 (qtbase only)")
        execute_process(
            COMMAND "${MOTION_PYTHON}" -m aqt install-qt windows desktop
                    "${MOTION_QT_VERSION}" win64_msvc2022_64
                    --outputdir "${_qt_out}"
                    --archives qtbase
            RESULT_VARIABLE _qt_st
        )
        if(NOT _qt_st EQUAL 0 OR NOT EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
            message(FATAL_ERROR "aqtinstall of Qt ${MOTION_QT_VERSION} failed (exit ${_qt_st})")
        endif()
    endif()
    set(MOTION_QT_PREFIX "${_qt_prefix}" CACHE PATH "Official Qt MSVC prebuilts" FORCE)
endif()
list(PREPEND CMAKE_PREFIX_PATH "${MOTION_QT_PREFIX}")
set(Qt6_DIR "${MOTION_QT_PREFIX}/lib/cmake/Qt6" CACHE PATH "Qt6 CMake config" FORCE)
message(STATUS "Using prebuilt Qt: ${MOTION_QT_PREFIX}")
