// TODO 为什么data
// flash要一次性写入，因为无法判断是否有更新，每次都会更新所有的data，在所有data不正确的时候不会到下一个步骤
// app中不擦除checksum和length可以在bootloader中反悔，不更新程序，在app中即使误触发，在bootloader也可以不更新程序然后跳转到app

#include "bootloader.h"

// #define ERUNNING_MODE_DEBUG

#ifdef ERUNNING_MODE_DEBUG
uint8_t u8DebugMsgFunc = DEBUG_MSG_FUNC_OPEN;
#else
uint8_t u8DebugMsgFunc = DEBUG_MSG_FUNC_CLOSE;
#endif

static volatile uint8_t sU8AppInfoBuf[ARR_LENGTH_APP_INFO] = {0};
uint32_t u32DataFlashAppUpgradeFlag = 0;
static uint32_t sU32LastCount = 0;
static uint16_t sU16BootloaderState = 0;

uint32_t u32Checksum = 0;
uint32_t u32Length = 0;
uint32_t u32Offset = 0;
uint32_t u32SingleWFlashLength = 0;
uint32_t u32ChecksumFromCal = 0;

int bootloader_init() {
  write_bootloader_state(EN_BOOTLOADER_STATE_BOOTLOADER_INIT_START);
  bsp_init();
  print_msg_summary();
  bootloader_data_init();
  if (u8DebugMsgFunc || 1) {
    printf("Bootloader init done!\r\n");
  }

  return 0;
}

int bootloader_run() {
  write_bootloader_state(EN_BOOTLOADER_STATE_BOOTLOADER_RUN_START);
  static uint8_t u8FirstJumpToAppFlag = EN_RESULT_DEFAULT;
  if (EN_RESULT_DEFAULT == u8FirstJumpToAppFlag) {
    u8FirstJumpToAppFlag = jump_to_app();
    if (u8DebugMsgFunc && EN_RESULT_FAILED) {
      printf("Bootloader jumped to app unsuccessfully!\r\n");
    }
  }

  parse_data_from_uart();
  write_to_flash_from_mcu_reg();
  jump_to_app_after_flash_app_upgrade();
  bsp_telm_state_check();
  return 0;
}

void parse_data_from_uart(void) {
  write_bootloader_state(EN_BOOTLOADER_STATE_PARSE_DATA_FROM_UART_START);
  delay_us(3000);
  if (g_uart1_rx_count) {
    if (sU32LastCount ==
        g_uart1_rx_count) /* 新周期内,没有收到任何数据,认为本次数据接收完成 */
    {
      // printf("uart1 reception completed!\r\n");
      printf("uart1 has received %d Bytes\r\n", g_uart1_rx_count);

      if (u8DebugMsgFunc) {
        for (int i = 0; i < g_uart1_rx_count; i++) {
          printf("%02X ", u8Uart1RxBuf[i]);
          if (0XF == (i & 0XF)) {
            printf("\r\n");
          }
        }
        printf("\r\n");
      }

      write_to_mcu_reg_from_usart(u8Uart1RxBuf, g_uart1_rx_count);
      UART1_Receive(u8Uart1RxBuf, USART_REC_LEN);
    } else
      sU32LastCount = g_uart1_rx_count;
  }
}

void write_buf_for_app_info(uint8_t *u8Buf, uint8_t length, uint8_t index) {
  for (int i = 0; i < length; i++) {
    sU8AppInfoBuf[i + index] = u8Buf[i];
  }
}

void write_app_info(void) {
  EraseSector(OFFSET_FLASH_ADDR_APP_INFO_START);
  ProgramPage((OFFSET_FLASH_ADDR_APP_INFO_START), sizeof(sU8AppInfoBuf),
              sU8AppInfoBuf);
}

void print_bootloader_info_from_app_info(void) {
  printf("u32DataFlashAppUpgradeFlag:= %08X\r\n",
         (*((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                         OFFSET_FLASH_ADDR_APP_INFO_START))));
  printf("u32DataFlashAppChecksum:= %08X\r\n",
         (*((uint32_t *)(OFFSET_APP_INFO_APP_CHECKSUM +
                         OFFSET_FLASH_ADDR_APP_INFO_START))));
  printf("u32DataFlashAppLength:= %08X\r\n",
         (*((uint32_t *)(OFFSET_APP_INFO_APP_LENGTH +
                         OFFSET_FLASH_ADDR_APP_INFO_START))));
}

void printFlash(uint8_t *r_ptr, uint32_t length) {
  uint32_t i;
  uint8_t dat;
  for (i = 0; i < length; i++) {
    if (!((uint8_t)r_ptr & 0XF)) {
      printf("%08X :", (uint32_t)r_ptr);
    }
    dat = *r_ptr;
    printf("%02X ", dat);
    if (0XF == ((uint32_t)r_ptr & 0XF)) {
      printf("\r\n");
    }
    r_ptr++;
  }
  printf("\r\n");
}

void bootloader_data_init(void) {
  write_bootloader_state(EN_BOOTLOADER_STATE_BOOTLOADER_DATA_INIT_START);
  *((uint32_t *)(sU8AppInfoBuf + OFFSET_APP_INFO_APP_UPGRADE_FLAG)) =
      *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                     OFFSET_FLASH_ADDR_APP_INFO_START));
  *((uint32_t *)(sU8AppInfoBuf + OFFSET_APP_INFO_APP_CHECKSUM)) =
      *((uint32_t *)(OFFSET_APP_INFO_APP_CHECKSUM +
                     OFFSET_FLASH_ADDR_APP_INFO_START));
  *((uint32_t *)(sU8AppInfoBuf + OFFSET_APP_INFO_APP_LENGTH)) =
      *((uint32_t *)(OFFSET_APP_INFO_APP_LENGTH +
                     OFFSET_FLASH_ADDR_APP_INFO_START));

  u32DataFlashAppUpgradeFlag =
      *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                     OFFSET_FLASH_ADDR_APP_INFO_START));
  printf("u32DataFlashAppUpgradeFlag:= %#X\r\n", u32DataFlashAppUpgradeFlag);
  u32Checksum = *((uint32_t *)(OFFSET_APP_INFO_APP_CHECKSUM +
                               OFFSET_FLASH_ADDR_APP_INFO_START));
  printf("u32Checksum:= %#X\r\n", u32Checksum);

  u32Length = *((uint32_t *)(OFFSET_APP_INFO_APP_LENGTH +
                             OFFSET_FLASH_ADDR_APP_INFO_START));
  printf("u32Length:= %#X\r\n", u32Length);
  // 配置一些寄存器的初始值
  u16Reg[OFFSET_APPLICATION_TYPE_U16] = APPLICATION_TYPE_BOOTLOADER;
  u16Reg[OFFSET_UPGRADE_PROGRAM_FLAG_U16] =
      (uint16_t)u32DataFlashAppUpgradeFlag;
  u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16] = 0X0;
  // u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16] = 0X400;
  u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16] = 0X2800;
  u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_STATE_U16] = UPDATE_STATE_START;
  u16Reg[OFFSET_UPDATE_ALL_APP_STATE_U16] = UPDATE_ALL_APP_STATE_FAILURE;
  local_generate_crc32_table();
}

en_result_t jump_to_app(void) {
  write_bootloader_state(EN_BOOTLOADER_STATE_JUMP_TO_APP_START);
  if (UPGRADE_PROGRAM_FLAG_UPDATE != u32DataFlashAppUpgradeFlag) {
    if ((0 != u32Length)) {
      if (Max_APP_LENGTH > u32Length) {
        if ((((*(volatile uint32_t *)(OFFSET_FLASH_ADDR_APP_START + 4)) &
              (OFFSET_FLASH_ADDR_APP_START)) == OFFSET_FLASH_ADDR_APP_START)) {
          printf("There is an app in FLASH!\r\n");
          u32ChecksumFromCal = local_crc32(
              0, ((volatile uint8_t *)(OFFSET_FLASH_ADDR_APP_START)),
              u32Length);
          printf("u32Checksum:= %#X\r\n", u32Checksum);
          printf("u32ChecksumFromCal:= %#X\r\n", u32ChecksumFromCal);
          if (u32Checksum == u32ChecksumFromCal) {
            printf("Start executing FLASH user code!\r\n");
            BspCpuJump(OFFSET_FLASH_ADDR_APP_START);
            return EN_RESULT_SUCCESSFUL;
          } else {
            printf("checksum error!\r\n");
            return EN_RESULT_FAILED;
          }
        } else {
          printf("No firmware to run!\r\n");
          return EN_RESULT_FAILED;
        }
      } else {
        printf("u32Length > MaxProgramSize!\r\n");
        return EN_RESULT_FAILED;
      }
    } else {
      printf("u32Length:= 0!\r\n");
      return EN_RESULT_FAILED;
    }
  } else {
    printf("u32DataFlashAppUpgradeFlag:= %#X\r\n", u32DataFlashAppUpgradeFlag);
    return EN_RESULT_FAILED;
  }
}

void AppRemap(uint32_t vector_addr) {
  __disable_irq();
  SCB->VTOR = vector_addr; /*vector address*/
  __DMB();
}

uint8_t BspCpuJump(uint32_t addr) {
  if (addr != OFFSET_FLASH_ADDR_APP_START) {
    return 1;
  }
  __disable_irq(); // 关闭所有中断
  UART1_Stop();
  SysTick->CTRL = 0; // 关闭系统定时器!!!!!!!!!!!
  __set_MSP(*(__IO uint32_t *)OFFSET_FLASH_ADDR_APP_START);
  AppRemap(OFFSET_FLASH_ADDR_APP_START);
  ((void (*)())(*(volatile unsigned long *)(OFFSET_FLASH_ADDR_APP_START +
                                            0x04)))(); // to APP
  return 0;
}

void jump_to_app_after_flash_app_upgrade(void) {
  write_bootloader_state(
      EN_BOOTLOADER_STATE_JUMP_TO_APP_AFTER_FLASH_APP_UPGRADE_START);
  if (((u32Length) == u32Offset) && ((u32Length) > 0) && (u32Offset > 0)) {
    printf("Program update completed!\r\n");

    if (Max_APP_LENGTH > u32Length) {
      if ((((*(volatile uint32_t *)(OFFSET_FLASH_ADDR_APP_START + 4)) &
            (OFFSET_FLASH_ADDR_APP_START)) == OFFSET_FLASH_ADDR_APP_START)) {
        printf("There is an app in FLASH!\r\n");
        u32ChecksumFromCal = local_crc32(
            0, ((volatile uint8_t *)(OFFSET_FLASH_ADDR_APP_START)), u32Length);
        printf("u32Checksum:= %#X\r\n", u32Checksum);
        printf("u32ChecksumFromCal:= %#X\r\n", u32ChecksumFromCal);
        if (u32Checksum == u32ChecksumFromCal) {
          printf("To reset upgrade program flag!\r\n");
          do {
            uint8_t u8TmpBuf[4] = {0};
            u8TmpBuf[0] = UPGRADE_PROGRAM_FLAG_NO_UPDATE;

            write_buf_for_app_info(u8TmpBuf, APP_INFO_SINGLE_DATA_LENGTH,
                                   OFFSET_APP_INFO_APP_UPGRADE_FLAG);
            write_app_info();
            u32DataFlashAppUpgradeFlag =
                *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                               OFFSET_FLASH_ADDR_APP_INFO_START));
            u16Reg[OFFSET_APP_UPGRADE_FLAG_STATE_U16] = UPDATE_STATE_DONE;
            print_bootloader_info_from_app_info();
          } while (UPGRADE_PROGRAM_FLAG_NO_UPDATE !=
                   u32DataFlashAppUpgradeFlag);

          u16Reg[OFFSET_UPDATE_ALL_APP_STATE_U16] =
              UPDATE_ALL_APP_STATE_SUCCESS;

          int timeout = 5;
          while (timeout > 0) {
            if (SYS_SOFT_RESET == u16Reg[OFFSET_SYS_SOFT_RESET_U16]) {
              break;
            } else {
              delay_us(1000000);
              timeout--;
            }
          }
          NVIC_SystemReset();

        } else {
          printf("checksum error!\r\n");
          u16Reg[OFFSET_UPDATE_ALL_APP_STATE_U16] =
              UPDATE_ALL_APP_STATE_FAILURE;
          u32Offset = 0;
        }
      } else {
        printf("No firmware to run!\r\n");
        u16Reg[OFFSET_UPDATE_ALL_APP_STATE_U16] = UPDATE_ALL_APP_STATE_FAILURE;

        for (int i = 0; i < u32Offset; i += 4) {
          printf("0X%08X",
                 (*((volatile uint32_t *)(OFFSET_FLASH_ADDR_APP_START + i))));
          printf("\t");
          if (0 == i % 8) {
            printf("\r\n");
          }
        }

        u32Offset = 0;
      }
    } else {
      printf("u32Length > Max_APP_LENGTH!\r\n");
      u16Reg[OFFSET_UPDATE_ALL_APP_STATE_U16] = UPDATE_ALL_APP_STATE_FAILURE;
      u32Offset = 0;
    }
  }
}

void print_msg_summary(void) {
  if (u8DebugMsgFunc || 1) {
    printf("%s\r\n", MSG_SUMMARY);
    printf("HW_VERSION_NUM: %X.%X.%X\r\n", HW_VERSION_MAJOR, HW_VERSION_MINOR,
           HW_VERSION_REVISION);
    printf("FW_VERSION_NUM: %X.%X.%X\r\n", FW_VERSION_MAJOR, FW_VERSION_MINOR,
           FW_VERSION_REVISION);
  }
}

void write_bootloader_state(uint16_t state) {
  sU16BootloaderState = state;
  u16Reg[OFFSET_BOOTLOADER_STATE_U16] = state;
  if (u8DebugMsgFunc && 0) {
    printf("Bootloader State:= %d\r\n", state);
  }
}