#ifndef BL_HAL_H
#define BL_HAL_H

#include <stddef.h>
#include <stdint.h>
#include "ota_config.h"
#include "ota_types.h"

typedef enum { BL_FLASH_ERASE = 0, BL_FLASH_PROGRAM = 1 } bl_flash_operation_t;
typedef enum { OTA_ERROR_NONE = 0, OTA_ERROR_FLASH_ERASE, OTA_ERROR_FLASH_PROGRAM, OTA_ERROR_FLASH_VERIFY } ota_error_t;

int bl_flash_range_allowed(uint32_t address, size_t length, ota_slot_t active, bl_flash_operation_t operation);
size_t bl_hal_flash_program_padded_length(size_t length);
uint32_t bl_hal_millis(void);
void bl_hal_watchdog_service(void);
void bl_hal_reset(void);
ota_error_t bl_hal_flash_read(uint32_t address, void *dst, size_t length);
ota_error_t bl_hal_flash_erase(uint32_t address, size_t length, ota_slot_t active);
ota_error_t bl_hal_flash_program(uint32_t address, const void *src, size_t length, ota_slot_t active);
int bl_hal_uart1_read(uint8_t *byte, uint32_t timeout_ms);
void bl_hal_uart1_write(uint8_t byte);
int bl_hal_esp_read(uint8_t *byte, uint32_t timeout_ms);
void bl_hal_esp_write(uint8_t byte);
void bl_hal_uart_wait_tx_complete(void);
void bl_hal_uart0_log(const char *message);

#endif
