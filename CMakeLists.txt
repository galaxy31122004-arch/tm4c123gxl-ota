cmake_minimum_required(VERSION 3.20)

project(tm4c123gxl_ota_host C)

include(CTest)
include(cmake/tm4c_host.cmake)

add_library(ota_common STATIC
    tm4c123gxl/common/src/ota_crc32.c
)
target_include_directories(ota_common PUBLIC tm4c123gxl/common/inc)
tm4c_host_enable_strict_warnings(ota_common)

add_executable(crc32 tests/c/test_crc32.c)
target_include_directories(crc32 PRIVATE tm4c123gxl/common/inc)
target_link_libraries(crc32 PRIVATE ota_common)
tm4c_host_enable_strict_warnings(crc32)
add_test(NAME crc32 COMMAND crc32)

add_executable(config tests/c/test_config.c)
target_include_directories(config PRIVATE tm4c123gxl/common/inc)
tm4c_host_enable_strict_warnings(config)
add_test(NAME config COMMAND config)
