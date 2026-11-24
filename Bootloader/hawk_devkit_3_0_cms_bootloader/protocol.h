#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <stdio.h>

#include "bootloader.h"

#define USART_REC_LEN_FOR_APP_INFO 6

typedef enum en_adaps_hawk_mcu_reg_addr_version {
  OFFSET_VERSION_LENGTH = 0X2,
  OFFSET_VERSION = 0X10,
  OFFSET_FIRMWARE_VERSION = 0X10,
  OFFSET_HARDWARE_VERSION = 0X11,

} en_adaps_hawk_mcu_reg_addr_version_t;

typedef enum en_adaps_hawk_dvk_uc930_mcu_reg_addr {
  STATUS_REG_BASE = 0x0,
  OFFSET_DEV_ID = 0x0,
  OFFSET_TEMP_A = 0x1,
  OFFSET_TEMP_B = 0x2,
  OFFSET_VOL_DV11 = 0x3,
  OFFSET_VOL_SV11 = 0x4,
  OFFSET_CURRENT_PVDD = 0x5,
  OFFSET_ERR_CODE = 0x6,

  OFFSET_TEMP_RX_BOARD = 0x0C,

  OFFSET_FIRMWARE_VER = 0x10,
  OFFSET_HARDWARE_VER = 0x11,

  OFFSET_TELM_METHOD = 0x1F,

  CONTROL_REG_BASE = 0x80,
  OFFSET_VOL_PVDD = 0x80,
  OFFSET_VOL_VOP = 0x81,
  OFFSET_DEBUG = 0x82,
  OFFSET_VCSEL_ENABLE = 0x88,
  OFFSET_DV11 = 0xB0,
  OFFSET_SV11 = 0xB1,
  OFFSET_AV11 = 0xB2,
  OFFSET_MP_VCC11A = 0xB3,
  OFFSET_IOV18 = 0xB4,
  OFFSET_MP_VCC18A = 0xB5,
  OFFSET_IOV33_DRV = 0xB6,
  OFFSET_IOV33_TRIG = 0xB7,
  OFFSET_AV33 = 0xB8,
  OFFSET_SV33 = 0xB9,
  OFFSET_VCC_3V3 = 0xBA,
  OFFSET_VOP = 0xBB,
  OFFSET_PVDD = 0xBC,
  OFFSET_ALL = 0xBD,
  OFFSET_HEART_BEAT_SWITCH = 0xBE,
  OFFSET_VCC_5V_DRV = 0xBF,

  OFFSET_TX_FIXED_CHN = 0xF0, // for roi cali

  REG_MAX = 0xFF,
} en_adaps_hawk_dvk_uc930_mcu_reg_addr_t;

#define ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET 0X20

typedef enum en_adaps_hawk_mcu_bootloader_reg_addr_u16 {

  OFFSET_APPLICATION_TYPE_U16 = 0U, // APP:= 0XA, BOOTLOADER:= 0XB
  OFFSET_UPGRADE_PROGRAM_FLAG_U16,

  OFFSET_CHECKSUM_31_16_U16,
  OFFSET_CHECKSUM_15_0_U16, 
  OFFSET_CHECKSUM_STATE_U16, // 4

  OFFSET_LENGTH_31_16_U16,
  OFFSET_LENGTH_15_0_U16,
  OFFSET_LENGTH_STATE_U16,

  OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16, // 8
  OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16,
  OFFSET_SINGLE_W_FLASH_LENGTH_STATE_U16,

  OFFSET_UPDATE_APP_STATE_U16,  

  OFFSET_UPDATE_ALL_APP_STATE_U16, // C

  OFFSET_SYS_SOFT_RESET_U16,

  OFFSET_MCU_BOOTLOADER_STATE_REG_ADDR_U16,

  OFFSET_MCU_BOOTLOADER_ERROR_REG_ADDR_U16,

  OFFSET_MCU_SPI_CHECKSUM_ERROR_REG_ADDR_U16,

  OFFSET_APP_UPGRADE_FLAG_31_16_U16,
  OFFSET_APP_UPGRADE_FLAG_15_0_U16,
  OFFSET_APP_UPGRADE_FLAG_STATE_U16,

  OFFSET_DEBUG_MSG_FUNC_REG_ADDR_U16, // 0X0:不输出debug信息 0X1:输出debug信息
  OFFSET_DEBUG_MSG_FUNC_STATE_REG_ADDR_U16,

  OFFSET_BOOTLOADER_STATE_U16,

  OFFSET_LENGTH_MCU_BOOTLOADER_REG_ADDR_U16,

} en_adaps_hawk_mcu_bootloader_reg_addr_u16_t;

typedef enum en_adaps_hawk_mcu_bootloader_application_type {

  APPLICATION_TYPE_APP = 0XA,
  APPLICATION_TYPE_BOOTLOADER = 0XB,
} en_adaps_hawk_mcu_bootloader_application_type_t;

typedef enum en_adaps_hawk_mcu_bootloader_upgrade_program_flag {

  UPGRADE_PROGRAM_FLAG_NO_UPDATE = 0XA,
  UPGRADE_PROGRAM_FLAG_UPDATE = 0XB,
} en_adaps_hawk_mcu_bootloader_upgrade_program_flag_t;

typedef enum en_adaps_hawk_mcu_bootloader_sys_soft_reset {

  SYS_SOFT_NO_RESET = 0U,
  SYS_SOFT_RESET = 1U,
} en_adaps_hawk_mcu_bootloader_sys_soft_reset_t;

typedef enum en_adaps_hawk_mcu_bootloader_update_all_app_state {

  UPDATE_ALL_APP_STATE_SUCCESS = 0X6666,
  UPDATE_ALL_APP_STATE_FAILURE = 0XFFFF,
} en_adaps_hawk_mcu_bootloader_update_all_app_state_t;

typedef enum en_adaps_hawk_mcu_bootloader_update_state {

  UPDATE_STATE_IDLE = 0U,
  UPDATE_STATE_START = 1U,
  UPDATE_STATE_DONE = 2U,
} en_adaps_hawk_mcu_bootloader_update_state_t;

typedef enum en_debug_msg_func_type {

  DEBUG_MSG_FUNC_CLOSE = 0X0,
  DEBUG_MSG_FUNC_OPEN = 0X1,
} en_debug_msg_func_type_t;

#define ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET 0X1000

typedef enum en_adaps_hawk_mcu_flash_app_reg_addr {

  OFFSET_FLASH_APP_START = 0X0,
  OFFSET_FLASH_END = 0X27FF,

  OFFSET_LENGTH_MCU_FLASH_APP,

} en_adaps_hawk_mcu_flash_app_reg_addr_t;

typedef enum en_uart_cmd {
  UART_CMD_WRITE_APP_CHECKSUM = 0U,
  UART_CMD_WRITE_APP_LENGTH,
  UART_CMD_WRITE_APP_DATA,
  UART_CMD_WRITE_APP_SINGLE_W_FLASH_LENGTH,
  UART_CMD_WRITE_APP_UPGRADE_FLAG,
} en_uart_cmd_t;


extern volatile uint8_t u8FlashAppReg[OFFSET_LENGTH_MCU_FLASH_APP];
extern uint16_t u16Reg[OFFSET_LENGTH_MCU_BOOTLOADER_REG_ADDR_U16];

#define CRC32_TABLE_BUF_LEN (256)
#define POLYNOMIAL          0xEDB88320

uint16_t reg_val_get(uint16_t index);
void reg_control_set(uint16_t index, uint16_t val);

void write_to_mcu_reg_from_usart(uint8_t *data, uint32_t length);

void write_to_flash_from_mcu_reg(void);

void local_generate_crc32_table(void);

uint32_t local_crc32(uint32_t initial, uint8_t *buf, uint32_t len);

#endif // __PROTOCOL_H
