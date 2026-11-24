#ifndef _BOOTLOADER_H__
#define _BOOTLOADER_H__

#include "BAT32A239.h"
#include "bsp_basic.h"
#include "bsp_telematic.h"
#include "clk.h"
#include "flash.h"
#include "sci.h"
#include "userdefine.h"
#include <stdio.h>

#define MSG_SUMMARY         "ADAPS_HAWK_DEVKIT_3_0_CMS_BOOTLOADER"

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_REVISION 0

#define FW_VERSION_NUM                                                         \
  ((FW_VERSION_MAJOR << 12) | (FW_VERSION_MINOR << 8) | (FW_VERSION_REVISION))

#define HW_VERSION_MAJOR    3
#define HW_VERSION_MINOR    0
#define HW_VERSION_REVISION 0

#define HW_VERSION_NUM                                                         \
  ((HW_VERSION_MAJOR << 12) | (HW_VERSION_MINOR << 8) | (HW_VERSION_REVISION))

#define DEV_ID                      0XA239

#define DATA_ADDR_BASE              0x00500200

#define MAIN_FLASH_ADDR_BASE        0X0
#define APP_INFO_ADDR_BASE          0X00500200

#define APP_INFO_SINGLE_DATA_LENGTH 0X4

typedef enum en_result {
  EN_RESULT_DEFAULT = 0U,
  EN_RESULT_SUCCESSFUL = 0U,
  EN_RESULT_FAILED = 1U,
} en_result_t;

typedef enum en_flash_addr {
  OFFSET_FLASH_ADDR_START = 0X0,
  OFFSET_FLASH_ADDR_BOOTLOADER_START = 0X0,
  OFFSET_FLASH_ADDR_BOOTLOADER_END = 0X4BFF,
  OFFSET_FLASH_BOOTLOADER_LENGTH = 0X4C00,

  OFFSET_FLASH_ADDR_APP_INFO_START = 0X4C00,
  OFFSET_FLASH_ADDR_APP_INFO_END = 0X4C0C,

  OFFSET_FLASH_ADDR_APP_START = 0X5000,
  OFFSET_FLASH_ADDR_APP_END = 0X3FFFF,
  OFFSET_FLASH_TOTAL_LENGTH = 0X40000,
} en_flash_addr_t;

#define Max_APP_LENGTH                                                         \
  (OFFSET_FLASH_TOTAL_LENGTH - OFFSET_FLASH_ADDR_APP_START)
#define ERASE_SECTOR_SINGLE_LENGTH 0X200

typedef enum en_app_info_addr {

  OFFSET_APP_INFO_APP_UPGRADE_FLAG = 0U, // APP:= 0XA, BOOTLOADER:= 0XB
  OFFSET_APP_INFO_APP_CHECKSUM = 4U,
  OFFSET_APP_INFO_APP_LENGTH = 8U,
  OFFSET_LENGTH_APP_INFO,

} en_app_info_addr_t;

typedef enum en_app_info_arr_idx {

  ARR_IDX_APP_INFO_APP_UPGRADE_FLAG_31_24 = 0U,
  ARR_IDX_APP_INFO_APP_UPGRADE_FLAG_23_16,
  ARR_IDX_APP_INFO_APP_UPGRADE_FLAG_15_8,
  ARR_IDX_APP_INFO_APP_UPGRADE_FLAG_7_0,

  ARR_IDX_APP_INFO_APP_CHECKSUM_31_24,
  ARR_IDX_APP_INFO_APP_CHECKSUM_23_16,
  ARR_IDX_APP_INFO_APP_CHECKSUM_15_8,
  ARR_IDX_APP_INFO_APP_CHECKSUM_7_0,

  ARR_IDX_APP_INFO_APP_LENGTH_31_24,
  ARR_IDX_APP_INFO_APP_LENGTH_23_16,
  ARR_IDX_APP_INFO_APP_LENGTH_15_8,
  ARR_IDX_APP_INFO_APP_LENGTH_7_0,

  ARR_LENGTH_APP_INFO,

} en_app_info_arr_idx_t;

typedef enum en_bootloader_state {
  EN_BOOTLOADER_STATE_BOOTLOADER_INIT_START = 0U,
  EN_BOOTLOADER_STATE_BSP_INIT_START,
  EN_BOOTLOADER_STATE_BSP_CLK_INIT_START,
  EN_BOOTLOADER_STATE_BSP_UART_INIT_START,
  EN_BOOTLOADER_STATE_BOOTLOADER_DATA_INIT_START,
  EN_BOOTLOADER_STATE_BOOTLOADER_RUN_START,
  EN_BOOTLOADER_STATE_JUMP_TO_APP_START,
  EN_BOOTLOADER_STATE_PARSE_DATA_FROM_UART_START,
  EN_BOOTLOADER_STATE_WRITE_TO_MCU_REG_FROM_USART_START,
  EN_BOOTLOADER_STATE_WRITE_TO_FLASH_FROM_MCU_REG_START,
  EN_BOOTLOADER_STATE_JUMP_TO_APP_AFTER_FLASH_APP_UPGRADE_START,

} en_bootloader_state_t;

extern uint32_t u32DataFlashAppUpgradeFlag;
extern uint8_t u8DebugMsgFunc;

extern uint32_t u32Offset;
extern uint32_t u32Checksum;
extern uint32_t u32Length;
extern uint32_t u32SingleWFlashLength;
extern uint32_t u32ChecksumFromCal;

int bootloader_init(void);
void print_msg_summary(void);
int bootloader_run(void);

void printFlash(uint8_t *r_ptr, uint32_t length);
void print_bootloader_info_from_app_info(void);
void write_buf_for_app_info(uint8_t *u8Buf, uint8_t length, uint8_t index);
void write_app_info(void);
void parse_data_from_uart(void);
en_result_t jump_to_app(void);
uint8_t BspCpuJump(uint32_t addr);
void jump_to_app_after_flash_app_upgrade(void);
void write_bootloader_state(uint16_t state);
#endif // _BOOTLOADER_H__
