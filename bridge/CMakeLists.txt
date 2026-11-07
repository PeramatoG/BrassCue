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

if(BUILD_TESTING)
  add_executable(brasscue-serial-test tests/serial_test.cpp)
  target_compile_features(brasscue-serial-test PRIVATE cxx_std_17)
  add_test(NAME brasscue-serial COMMAND brasscue-serial-test)
endif()

# Windows wxWidgets UI builds by default. CI can explicitly disable it for headless checks.
if(WIN32 AND BRASSCUE_BUILD_GUI)
  find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base)
  add_executable(brasscue-gui WIN32 gui/main.cpp)
  target_compile_features(brasscue-gui PRIVATE cxx_std_17)
  target_compile_definitions(brasscue-gui PRIVATE NOMINMAX)
  target_link_libraries(brasscue-gui PRIVATE wx::core wx::base ws2_32)
endif()
