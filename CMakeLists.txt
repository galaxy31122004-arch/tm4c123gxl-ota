cmake_minimum_required(VERSION 3.20)

project(tm4c123gxl_ota_host C)

include(CTest)
include(cmake/tm4c_host.cmake)

add_library(ota_common STATIC
    tm4c123gxl/common/src/ota_crc32.c
    tm4c123gxl/common/src/ota_protocol.c
    tm4c123gxl/common/src/ota_metadata.c
    tm4c123gxl/common/src/ota_boot.c
    tm4c123gxl/common/src/ota_image.c
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

add_executable(protocol tests/c/test_protocol.c)
target_link_libraries(protocol PRIVATE ota_common)
tm4c_host_enable_strict_warnings(protocol)
add_test(NAME protocol COMMAND protocol)

add_executable(metadata tests/c/test_metadata.c)
target_link_libraries(metadata PRIVATE ota_common)
tm4c_host_enable_strict_warnings(metadata)
add_test(NAME metadata COMMAND metadata)

add_executable(boot tests/c/test_boot.c)
target_link_libraries(boot PRIVATE ota_common)
tm4c_host_enable_strict_warnings(boot)
add_test(NAME boot COMMAND boot)

add_executable(image tests/c/test_image.c)
target_include_directories(image PRIVATE tm4c123gxl/common/inc)
target_link_libraries(image PRIVATE ota_common)
tm4c_host_enable_strict_warnings(image)
add_test(NAME image COMMAND image)

add_library(bl_hal_host STATIC tm4c123gxl/bootloader/src/bl_hal_tm4c.c)
target_include_directories(bl_hal_host PUBLIC tm4c123gxl/bootloader/inc tm4c123gxl/common/inc)
target_link_libraries(bl_hal_host PRIVATE ota_common)
tm4c_host_enable_strict_warnings(bl_hal_host)
add_executable(flash_guard tests/c/test_flash_guard.c)
target_include_directories(flash_guard PRIVATE tm4c123gxl/bootloader/inc tm4c123gxl/common/inc)
target_link_libraries(flash_guard PRIVATE bl_hal_host)
tm4c_host_enable_strict_warnings(flash_guard)
add_test(NAME flash_guard COMMAND flash_guard)

add_library(bl_jump_host STATIC tm4c123gxl/bootloader/src/bl_jump.c)
target_include_directories(bl_jump_host PUBLIC tm4c123gxl/bootloader/inc tm4c123gxl/common/inc)
tm4c_host_enable_strict_warnings(bl_jump_host)
target_compile_options(bl_jump_host PRIVATE -Wno-error=int-to-pointer-cast)
add_executable(jump tests/c/test_jump.c)
target_link_libraries(jump PRIVATE bl_jump_host)
tm4c_host_enable_strict_warnings(jump)
add_test(NAME jump COMMAND jump)

add_library(bl_update_host STATIC tm4c123gxl/bootloader/src/bl_update.c)
target_include_directories(bl_update_host PUBLIC tm4c123gxl/bootloader/inc tm4c123gxl/common/inc)
target_link_libraries(bl_update_host PRIVATE ota_common)
tm4c_host_enable_strict_warnings(bl_update_host)
add_executable(update tests/c/test_update.c)
target_include_directories(update PRIVATE tm4c123gxl/bootloader/inc tm4c123gxl/common/inc)
target_link_libraries(update PRIVATE bl_update_host ota_common)
tm4c_host_enable_strict_warnings(update)
add_test(NAME update COMMAND update)

add_library(boot_confirm_host STATIC tm4c123gxl/application/src/boot_confirm.c)
target_include_directories(boot_confirm_host PUBLIC tm4c123gxl/application/inc tm4c123gxl/common/inc)
target_link_libraries(boot_confirm_host PRIVATE ota_common)
tm4c_host_enable_strict_warnings(boot_confirm_host)
add_executable(confirmation tests/c/test_confirmation.c)
target_include_directories(confirmation PRIVATE tm4c123gxl/application/inc tm4c123gxl/common/inc)
target_link_libraries(confirmation PRIVATE boot_confirm_host)
tm4c_host_enable_strict_warnings(confirmation)
add_test(NAME confirmation COMMAND confirmation)

add_library(bl_main_host STATIC tm4c123gxl/bootloader/src/bl_main.c)
target_include_directories(bl_main_host PUBLIC tm4c123gxl/bootloader/inc tm4c123gxl/application/inc tm4c123gxl/common/inc)
target_compile_definitions(bl_main_host PRIVATE main=bl_main_entry)
target_link_libraries(bl_main_host PRIVATE ota_common bl_hal_host bl_jump_host bl_update_host boot_confirm_host)
tm4c_host_enable_strict_warnings(bl_main_host)
add_executable(main_services tests/c/test_main_services.c)
target_include_directories(main_services PRIVATE tm4c123gxl/bootloader/inc tm4c123gxl/common/inc)
target_link_libraries(main_services PRIVATE bl_main_host)
tm4c_host_enable_strict_warnings(main_services)
add_test(NAME main_services COMMAND main_services)

add_library(esp_at_rpc STATIC tm4c_uart_esp32_bridge/esp_at_rpc.c)
target_include_directories(esp_at_rpc PUBLIC tm4c_uart_esp32_bridge)
tm4c_host_enable_strict_warnings(esp_at_rpc)
add_executable(test_esp_at_rpc tests/c/test_esp_at_rpc.c)
target_link_libraries(test_esp_at_rpc PRIVATE esp_at_rpc)
tm4c_host_enable_strict_warnings(test_esp_at_rpc)
add_test(NAME esp_at_rpc COMMAND test_esp_at_rpc)

add_library(esp_at_controller STATIC
    tm4c_uart_esp32_bridge/esp_at_controller.c
)
target_include_directories(esp_at_controller PUBLIC tm4c_uart_esp32_bridge)
target_link_libraries(esp_at_controller PUBLIC esp_at_rpc)
tm4c_host_enable_strict_warnings(esp_at_controller)
add_executable(test_esp_at_controller tests/c/test_esp_at_controller.c)
target_link_libraries(test_esp_at_controller PRIVATE esp_at_controller)
tm4c_host_enable_strict_warnings(test_esp_at_controller)
add_test(NAME esp_at_controller COMMAND test_esp_at_controller)
