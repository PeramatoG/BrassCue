cmake_minimum_required(VERSION 3.20)
project(BrassCue VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
  add_compile_options(/utf-8)
endif()

# Build the desktop UI by default on Windows. Keep the portable CLI and tests
# available without wxWidgets on Linux or in explicit headless configurations.
if(WIN32)
  set(BRASSCUE_GUI_DEFAULT ON)
else()
  set(BRASSCUE_GUI_DEFAULT OFF)
endif()
option(BRASSCUE_BUILD_GUI "Build the wxWidgets simulation application" ${BRASSCUE_GUI_DEFAULT})

option(BRASSCUE_ENABLE_WEB_PANEL "Enable the optional local wxWebView presentation" OFF)
if(BRASSCUE_ENABLE_WEB_PANEL AND NOT BRASSCUE_BUILD_GUI)
  message(FATAL_ERROR "BRASSCUE_ENABLE_WEB_PANEL requires BRASSCUE_BUILD_GUI=ON")
endif()

include(CTest)
add_subdirectory(bridge)

if(BUILD_TESTING)
  add_subdirectory(firmware/tests)
endif()
