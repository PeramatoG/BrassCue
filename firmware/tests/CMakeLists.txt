add_executable(brasscue-input-test input_test.cpp)
target_compile_features(brasscue-input-test PRIVATE cxx_std_17)
add_test(NAME brasscue-input COMMAND brasscue-input-test)
