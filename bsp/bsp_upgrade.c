/**
 * @file bsp_upgrade.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdio.h>
#include "bsp_upgrade.h"

#define APP_INFO_SINGLE_DATA_LENGTH 0X4

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

typedef enum en_adaps_hawk_mcu_bootloader_update_state {

  UPDATE_STATE_IDLE = 0U,
  UPDATE_STATE_START = 1U,
  UPDATE_STATE_DONE = 2U,
} en_adaps_hawk_mcu_bootloader_update_state_t;

typedef enum en_adaps_hawk_mcu_bootloader_app_upgrade_flag {

  APP_UPGRADE_FLAG_NO_UPDATE = 0XA,
  APP_UPGRADE_FLAG_UPDATE = 0XB,
} en_adaps_hawk_mcu_bootloader_app_upgrade_flag_t;


static volatile uint8_t sU8AppInfoBuf[ARR_LENGTH_APP_INFO] = {0};
static uint32_t sU32DataFlashAppUpgradeFlag = 0;
static uint32_t sU32LastCount = 0;
static uint16_t sU16BootloaderState = 0;

static uint32_t sU32Checksum = 0;
static uint32_t sU32Length = 0;

int bsp_upgrade_init(void)
{
  *((uint32_t *)(sU8AppInfoBuf + OFFSET_APP_INFO_APP_UPGRADE_FLAG)) =
      *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                     OFFSET_FLASH_ADDR_APP_INFO_START));
  *((uint32_t *)(sU8AppInfoBuf + OFFSET_APP_INFO_APP_CHECKSUM)) =
      *((uint32_t *)(OFFSET_APP_INFO_APP_CHECKSUM +
                     OFFSET_FLASH_ADDR_APP_INFO_START));
  *((uint32_t *)(sU8AppInfoBuf + OFFSET_APP_INFO_APP_LENGTH)) =
      *((uint32_t *)(OFFSET_APP_INFO_APP_LENGTH +
                     OFFSET_FLASH_ADDR_APP_INFO_START));

  sU32DataFlashAppUpgradeFlag =
      *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                     OFFSET_FLASH_ADDR_APP_INFO_START));
  printf("sU32DataFlashAppUpgradeFlag:= %#X\r\n", sU32DataFlashAppUpgradeFlag);
  sU32Checksum = *((uint32_t *)(OFFSET_APP_INFO_APP_CHECKSUM +
                               OFFSET_FLASH_ADDR_APP_INFO_START));
  printf("sU32Checksum:= %#X\r\n", sU32Checksum);

  sU32Length = *((uint32_t *)(OFFSET_APP_INFO_APP_LENGTH +
                             OFFSET_FLASH_ADDR_APP_INFO_START));
  printf("sU32Length:= %#X\r\n", sU32Length);
  return 0;
}

static void write_buf_for_app_info(uint8_t *u8Buf, uint8_t length, uint8_t index) {
  for (int i = 0; i < length; i++) {
    sU8AppInfoBuf[i + index] = u8Buf[i];
  }
}

static void write_app_info(void) {
  EraseSector(OFFSET_FLASH_ADDR_APP_INFO_START);
  ProgramPage((OFFSET_FLASH_ADDR_APP_INFO_START), sizeof(sU8AppInfoBuf),
              sU8AppInfoBuf);
}

int bsp_upgrade_trigger(void) {
  do {
    uint8_t u8TmpBuf[4] = {0};
    u8TmpBuf[0] = APP_UPGRADE_FLAG_UPDATE;

    write_buf_for_app_info(u8TmpBuf, APP_INFO_SINGLE_DATA_LENGTH,
                           OFFSET_APP_INFO_APP_UPGRADE_FLAG);
    write_app_info();
    sU32DataFlashAppUpgradeFlag =
        *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                       OFFSET_FLASH_ADDR_APP_INFO_START));
  } while (APP_UPGRADE_FLAG_UPDATE != sU32DataFlashAppUpgradeFlag);
  NVIC_SystemReset();
  return 0;
}
