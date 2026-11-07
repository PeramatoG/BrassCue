cmake_minimum_required(VERSION 3.20)
project(BrassCue VERSION 0.1.0 LANGUAGES CXX)

option(BRASSCUE_BUILD_GUI "Build the Windows wxWidgets simulation application" OFF)

include(CTest)
add_subdirectory(bridge)

if(BUILD_TESTING)
  add_subdirectory(firmware/tests)
endif()
