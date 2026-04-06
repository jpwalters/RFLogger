cmake_minimum_required(VERSION 3.21)
project(RFLogger VERSION 0.1.0 LANGUAGES C CXX)

# Allow CI to inject the release version from a git tag (e.g. -DRFLOGGER_VERSION=0.1)
set(RFLOGGER_VERSION "" CACHE STRING "Override project version from git tag in CI")
if(RFLOGGER_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    set(PROJECT_VERSION       "${RFLOGGER_VERSION}")
    set(PROJECT_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(PROJECT_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(PROJECT_VERSION_PATCH "${CMAKE_MATCH_3}")
elseif(RFLOGGER_VERSION MATCHES "^([0-9]+)\\.([0-9]+)$")
    set(PROJECT_VERSION       "${RFLOGGER_VERSION}.0")
    set(PROJECT_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(PROJECT_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(PROJECT_VERSION_PATCH "0")
elseif(RFLOGGER_VERSION)
    message(WARNING "RFLOGGER_VERSION '${RFLOGGER_VERSION}' is not MAJOR.MINOR[.PATCH] — ignoring.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(RFLOGGER_BUILD_TESTS "Build RFLogger tests" ON)
option(RFLOGGER_ENABLE_RTLSDR "Enable RTL-SDR device support" ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# ── Qt 6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network SerialPort PrintSupport)
qt_standard_project_setup()

# ── External: QCustomPlot ─────────────────────────────────────────────────────
# QCustomPlot is downloaded at configure time. If the download fails, place
# qcustomplot.h and qcustomplot.cpp manually in external/qcustomplot/.
include(FetchContent)

set(QCUSTOMPLOT_LOCAL_DIR "${CMAKE_SOURCE_DIR}/external/qcustomplot")
if(EXISTS "${QCUSTOMPLOT_LOCAL_DIR}/qcustomplot.h" AND EXISTS "${QCUSTOMPLOT_LOCAL_DIR}/qcustomplot.cpp")
    message(STATUS "Using local QCustomPlot from ${QCUSTOMPLOT_LOCAL_DIR}")
    set(QCUSTOMPLOT_SOURCE_DIR "${QCUSTOMPLOT_LOCAL_DIR}")
else()
    FetchContent_Declare(
        qcustomplot_src
        URL https://www.qcustomplot.com/release/2.1.1/QCustomPlot-source.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(qcustomplot_src)
    set(QCUSTOMPLOT_SOURCE_DIR "${qcustomplot_src_SOURCE_DIR}")
    message(STATUS "Downloaded QCustomPlot to ${QCUSTOMPLOT_SOURCE_DIR}")
endif()

add_library(qcustomplot STATIC
    "${QCUSTOMPLOT_SOURCE_DIR}/qcustomplot.h"
    "${QCUSTOMPLOT_SOURCE_DIR}/qcustomplot.cpp"
)
target_link_libraries(qcustomplot PUBLIC Qt6::Widgets Qt6::PrintSupport)
target_include_directories(qcustomplot PUBLIC "${QCUSTOMPLOT_SOURCE_DIR}")

# ── External: KissFFT ─────────────────────────────────────────────────────────
FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        131.1.0
    GIT_SHALLOW    TRUE
)
set(KISSFFT_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(KISSFFT_TEST OFF CACHE BOOL "" FORCE)
set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)
set(KISSFFT_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(kissfft)

# ── External: RTL-SDR (optional) ──────────────────────────────────────────────
if(RFLOGGER_ENABLE_RTLSDR)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    find_package(RtlSdr QUIET)
    if(RTLSDR_FOUND)
        message(STATUS "RTL-SDR found: ${RTLSDR_INCLUDE_DIRS}")
    else()
        message(STATUS "RTL-SDR not found — RTL-SDR device support disabled")
    endif()
endif()

# ── Version header ────────────────────────────────────────────────────────────
configure_file(
    "${CMAKE_SOURCE_DIR}/src/config/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# ── Application sources ──────────────────────────────────────────────────────
set(RFLOGGER_SOURCES
    src/main.cpp
    src/theme/DarkTheme.h
    src/theme/DarkTheme.cpp
    src/data/SweepData.h
    src/data/SweepData.cpp
    src/data/ScanSession.h
    src/data/ScanSession.cpp
    src/data/FrequencyMarker.h
    src/devices/ISpectrumDevice.h
    src/devices/DeviceManager.h
    src/devices/DeviceManager.cpp
    src/devices/RFExplorerDevice.h
    src/devices/RFExplorerDevice.cpp
    src/devices/TinySADevice.h
    src/devices/TinySADevice.cpp
    src/export/WwbExporter.h
    src/export/WwbExporter.cpp
    src/export/GenericCsvExporter.h
    src/export/GenericCsvExporter.cpp
    src/ui/SpectrumWidget.h
    src/ui/SpectrumWidget.cpp
    src/ui/WaterfallWidget.h
    src/ui/WaterfallWidget.cpp
    src/ui/DevicePanel.h
    src/ui/DevicePanel.cpp
    src/ui/CaptureControls.h
    src/ui/CaptureControls.cpp
    src/ui/MarkerPanel.h
    src/ui/MarkerPanel.cpp
    src/ui/ExportDialog.h
    src/ui/ExportDialog.cpp
    src/ui/ExportPanel.h
    src/ui/ExportPanel.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/app/SettingsManager.h
    src/app/SettingsManager.cpp
    src/app/AboutDialog.h
    src/app/AboutDialog.cpp
    src/app/UpdateChecker.h
    src/app/UpdateChecker.cpp
    src/app/CrashHandler.h
    src/app/CrashHandler.cpp
    resources/rflogger.qrc
)

if(WIN32)
    list(APPEND RFLOGGER_SOURCES
        resources/rflogger.rc
    )
endif()

# RTL-SDR device only if library found
if(RTLSDR_FOUND)
    list(APPEND RFLOGGER_SOURCES
        src/devices/RtlSdrDevice.h
        src/devices/RtlSdrDevice.cpp
    )
endif()

if(WIN32)
    qt_add_executable(RFLogger WIN32 ${RFLOGGER_SOURCES})
elseif(APPLE)
    qt_add_executable(RFLogger MACOSX_BUNDLE ${RFLOGGER_SOURCES})
    set_target_properties(RFLogger PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.rflogger.app"
        MACOSX_BUNDLE_BUNDLE_NAME "RFLogger"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    )
else()
    qt_add_executable(RFLogger ${RFLOGGER_SOURCES})
endif()

target_include_directories(RFLogger PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}/generated"
)

target_link_libraries(RFLogger PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::SerialPort
    Qt6::PrintSupport
    qcustomplot
    kissfft::kissfft
)

if(RTLSDR_FOUND)
    target_compile_definitions(RFLogger PRIVATE RFLOGGER_HAS_RTLSDR)
    target_include_directories(RFLogger PRIVATE ${RTLSDR_INCLUDE_DIRS})
    target_link_libraries(RFLogger PRIVATE ${RTLSDR_LIBRARIES})
endif()

option(RFLOGGER_DEBUG_MENU "Show Debug menu with crash-test actions" OFF)
if(RFLOGGER_DEBUG_MENU)
    target_compile_definitions(RFLogger PRIVATE RFLOGGER_DEBUG_MENU)
endif()

# ── Dev-time Qt deployment (Windows) ─────────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET RFLogger POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                    --no-compiler-runtime
                    --no-translations
                    "$<TARGET_FILE:RFLogger>"
            COMMENT "Running windeployqt to deploy Qt DLLs..."
        )
    else()
        message(WARNING "windeployqt not found — Qt DLLs will not be auto-deployed to build dir")
    endif()
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(RFLOGGER_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# ── Qt deploy script (bundles Qt DLLs on install) ────────────────────────────
if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(
        TARGET RFLogger
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
endif()

# ── Install / CPack ──────────────────────────────────────────────────────────
install(TARGETS RFLogger
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32 OR APPLE)
    install(SCRIPT "${deploy_script}")
endif()

# Linux desktop integration files (used by AppImage / system installs)
if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/resources/rflogger.desktop"
            DESTINATION share/applications)
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/rflogger.png"
            DESTINATION share/icons/hicolor/32x32/apps)
endif()

set(CPACK_PACKAGE_NAME "RFLogger")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "RF Logger Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RF Spectrum Logger for Pro Audio")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# ── Windows NSIS installer settings ──────────────────────────────────────────
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "RFLogger ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "RFLogger")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icons/rflogger.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icons/rflogger.ico")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/jpwalters/RFLogger")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RFLogger.lnk' '$INSTDIR\\\\bin\\\\RFLogger.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\RFLogger.lnk'")
endif()

include(CPack)
