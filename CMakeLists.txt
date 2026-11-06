cmake_minimum_required(VERSION 3.20)
project(BrassCue VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
add_subdirectory(bridge)

if(BUILD_TESTING)
  add_subdirectory(firmware/tests)
endif()
