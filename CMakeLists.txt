cmake_minimum_required(VERSION 3.16)
project(secure_resource_app_core C)

add_library(secure_resource_app_core src/secure_app.c)
target_include_directories(secure_resource_app_core PUBLIC include)
target_compile_features(secure_resource_app_core PUBLIC c_std_99)

include(CTest)
if(BUILD_TESTING)
    add_executable(secure_app_tests tests/test_secure_app.c)
    target_link_libraries(secure_app_tests PRIVATE secure_resource_app_core)
    add_test(NAME secure_app_tests COMMAND secure_app_tests)
endif()
