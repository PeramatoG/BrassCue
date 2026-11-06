add_executable(brasscue-bridge src/main.cpp)
target_compile_features(brasscue-bridge PRIVATE cxx_std_17)

if(WIN32)
  target_link_libraries(brasscue-bridge PRIVATE ws2_32)
endif()

if(BUILD_TESTING)
  add_executable(brasscue-core-test tests/core_test.cpp)
  target_compile_features(brasscue-core-test PRIVATE cxx_std_17)
  add_test(NAME brasscue-core COMMAND brasscue-core-test)
endif()
