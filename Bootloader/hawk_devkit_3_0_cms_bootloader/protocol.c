
#include "protocol.h"

volatile uint8_t u8FlashAppReg[OFFSET_LENGTH_MCU_FLASH_APP];
uint16_t u16Reg[OFFSET_LENGTH_MCU_BOOTLOADER_REG_ADDR_U16] = {0XB, 0X0};


uint16_t u16RegVersion[OFFSET_VERSION_LENGTH] = {FW_VERSION_NUM,
                                                 HW_VERSION_NUM};

uint16_t reg_val_get(uint16_t index)
{
    // printf("index:= 0X%X\r\n", index);
    // return index;
    if ((ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET <= index) && ((ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET + OFFSET_LENGTH_MCU_BOOTLOADER_REG_ADDR_U16) > index))
    {
        return u16Reg[index - ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET];
    }
    else if ((ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET <= index) && ((ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET + OFFSET_LENGTH_MCU_FLASH_APP / 2) > index))
    {
        return u8FlashAppReg[(index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2] | (u8FlashAppReg[(index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2 + 1] << 8);
    }
    else if ((OFFSET_FIRMWARE_VERSION <= index) && (OFFSET_HARDWARE_VERSION >= index))
    {
        return u16RegVersion[index - OFFSET_VERSION];
    }
    else if (OFFSET_DEV_ID == index)
    {
        return DEV_ID;
    }else if(OFFSET_TELM_METHOD == index){
#ifdef HW_TELM_USING_IIC
        return 0x01; //IIC
#else
        return 0xFF; //SPI
#endif
    }else
    {
        return 0XFF;
    }
}

void reg_control_set(uint16_t index, uint16_t val)
{
    // printf("index:= 0X%X\r\n", index);
    // printf("val:= 0X%X\r\n", val);
    if ((ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET <= index) && ((ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET + OFFSET_LENGTH_MCU_BOOTLOADER_REG_ADDR_U16) > index))
    {
        u16Reg[index - ADAPS_HAWK_MCU_BOOTLOADER_REG_ADDR_OFFSET] = val;
    }
    else if ((ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET <= index) && ((ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET + OFFSET_LENGTH_MCU_FLASH_APP / 2) > index))
    {

        u8FlashAppReg[(index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2] = (uint8_t)val;
        u8FlashAppReg[(index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2 + 1] = (uint8_t)(val >> 8);
        // printf("u8FlashAppReg[%d]:= 0X%X\r\n", (index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2, u8FlashAppReg[(index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2]);
        // printf("u8FlashAppReg[%d]:= 0X%X\r\n", (index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2 + 1, u8FlashAppReg[(index - ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) * 2 + 1]);
    }
    else
    {
        return;
    }
}

void write_to_mcu_reg_from_usart(uint8_t *data, uint32_t length) {
  write_bootloader_state(EN_BOOTLOADER_STATE_PARSE_DATA_FROM_UART_START);
  if (NULL == data) {
    return;
  }
  switch (data[1]) {
  case UART_CMD_WRITE_APP_CHECKSUM:

  {
    if (USART_REC_LEN_FOR_APP_INFO != length) {
      return;
    } else {

      u16Reg[OFFSET_CHECKSUM_31_16_U16] = data[2] << 8 | data[3];
      u16Reg[OFFSET_CHECKSUM_15_0_U16] = data[4] << 8 | data[5];

      u16Reg[OFFSET_CHECKSUM_STATE_U16] = 1;

      if (u8DebugMsgFunc) {
        printf("u16Reg[OFFSET_CHECKSUM_31_16_U16]:= %04X\r\n",
               u16Reg[OFFSET_CHECKSUM_31_16_U16]);
        printf("u16Reg[OFFSET_CHECKSUM_15_0_U16]:= %04X\r\n",
               u16Reg[OFFSET_CHECKSUM_15_0_U16]);
      }
    }
  }

  break;

  case UART_CMD_WRITE_APP_LENGTH:

  {
    if (USART_REC_LEN_FOR_APP_INFO != length) {
      return;
    } else {
      u16Reg[OFFSET_LENGTH_31_16_U16] = data[2] << 8 | data[3];
      u16Reg[OFFSET_LENGTH_15_0_U16] = data[4] << 8 | data[5];

      u16Reg[OFFSET_LENGTH_STATE_U16] = 1;
      if (u8DebugMsgFunc) {
        printf("u16Reg[OFFSET_LENGTH_31_16_U16]:= %04X\r\n",
               u16Reg[OFFSET_LENGTH_31_16_U16]);
        printf("u16Reg[OFFSET_LENGTH_15_0_U16]:= %04X\r\n",
               u16Reg[OFFSET_LENGTH_15_0_U16]);
      }
    }
  }

  break;

  case UART_CMD_WRITE_APP_DATA:
  {
    // printf("UART_CMD_WRITE_APP_DATA\r\n");
    if (2 > length) {
      return;
    } else {

      if ((u32Length == 0) && (u32Length <= u32Offset))
      {
          return;
      }
      else
      {
          for (int i = 2, j = 0; i < length; i += 2, j++)
          {
              uint16_t val = data[i + 1] << 8 | data[i];
              reg_control_set(j + ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET,
              val);
          }
          u16Reg[OFFSET_UPDATE_APP_STATE_U16] = 1;
      }
    }
  } break;
  case UART_CMD_WRITE_APP_SINGLE_W_FLASH_LENGTH:

  {
    if (USART_REC_LEN_FOR_APP_INFO != length) {
      return;
    } else {
      u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16] = data[2] << 8 | data[3];
      u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16] = data[4] << 8 | data[5];

      u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_STATE_U16] = 1;
      if (u8DebugMsgFunc) {
        printf("u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16]:= %04X\r\n",
               u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16]);
        printf("u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16]:= %04X\r\n",
               u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16]);
      }
    }
  }

  break;

  case UART_CMD_WRITE_APP_UPGRADE_FLAG:

  {
    if (USART_REC_LEN_FOR_APP_INFO != length) {
      return;
    } else {
      u16Reg[OFFSET_APP_UPGRADE_FLAG_31_16_U16] = data[2] << 8 | data[3];
      u16Reg[OFFSET_APP_UPGRADE_FLAG_15_0_U16] = data[4] << 8 | data[5];

      u16Reg[OFFSET_APP_UPGRADE_FLAG_STATE_U16] = 1;
      if (u8DebugMsgFunc) {
        printf("u16Reg[OFFSET_APP_UPGRADE_FLAG_31_16_U16]:= %04X\r\n",
               u16Reg[OFFSET_APP_UPGRADE_FLAG_31_16_U16]);
        printf("u16Reg[OFFSET_APP_UPGRADE_FLAG_15_0_U16]:= %04X\r\n",
               u16Reg[OFFSET_APP_UPGRADE_FLAG_15_0_U16]);
      }
    }
  }

  break;

  default:
    break;
  }
}

void write_to_flash_from_mcu_reg() {
  write_bootloader_state(EN_BOOTLOADER_STATE_WRITE_TO_FLASH_FROM_MCU_REG_START);
  if (UPDATE_STATE_START == u16Reg[OFFSET_CHECKSUM_STATE_U16]) {

    uint8_t u8TmpBuf[4] = {0};
    u8TmpBuf[0] = (uint8_t)u16Reg[OFFSET_CHECKSUM_15_0_U16];
    u8TmpBuf[1] = (uint8_t)(u16Reg[OFFSET_CHECKSUM_15_0_U16] >> 8);
    u8TmpBuf[2] = (uint8_t)u16Reg[OFFSET_CHECKSUM_31_16_U16];
    u8TmpBuf[3] = (uint8_t)(u16Reg[OFFSET_CHECKSUM_31_16_U16] >> 8);
    write_buf_for_app_info(u8TmpBuf, APP_INFO_SINGLE_DATA_LENGTH,
                             OFFSET_APP_INFO_APP_CHECKSUM);
    write_app_info();

    u32Checksum =
        *((uint32_t *)(OFFSET_APP_INFO_APP_CHECKSUM + OFFSET_FLASH_ADDR_APP_INFO_START));
    u16Reg[OFFSET_CHECKSUM_STATE_U16] = UPDATE_STATE_DONE;

    print_bootloader_info_from_app_info();
  }

  if (UPDATE_STATE_START == u16Reg[OFFSET_LENGTH_STATE_U16]) {
    uint8_t u8TmpBuf[4] = {0};
    u8TmpBuf[0] = (uint8_t)u16Reg[OFFSET_LENGTH_15_0_U16];
    u8TmpBuf[1] = (uint8_t)(u16Reg[OFFSET_LENGTH_15_0_U16] >> 8);
    u8TmpBuf[2] = (uint8_t)u16Reg[OFFSET_LENGTH_31_16_U16];
    u8TmpBuf[3] = (uint8_t)(u16Reg[OFFSET_LENGTH_31_16_U16] >> 8);
    write_buf_for_app_info(u8TmpBuf, APP_INFO_SINGLE_DATA_LENGTH,
                             OFFSET_APP_INFO_APP_LENGTH);
    write_app_info();
    u32Length =
        *((uint32_t *)(OFFSET_APP_INFO_APP_LENGTH + OFFSET_FLASH_ADDR_APP_INFO_START));
    u16Reg[OFFSET_LENGTH_STATE_U16] = UPDATE_STATE_DONE;
    print_bootloader_info_from_app_info();
  }

  if (UPDATE_STATE_START == u16Reg[OFFSET_APP_UPGRADE_FLAG_STATE_U16]){
    uint8_t u8TmpBuf[4] = {0};
    u8TmpBuf[0] = (uint8_t)u16Reg[OFFSET_APP_UPGRADE_FLAG_15_0_U16];
    u8TmpBuf[1] = (uint8_t)(u16Reg[OFFSET_APP_UPGRADE_FLAG_15_0_U16] >> 8);
    u8TmpBuf[2] = (uint8_t)u16Reg[OFFSET_APP_UPGRADE_FLAG_31_16_U16];
    u8TmpBuf[3] = (uint8_t)(u16Reg[OFFSET_APP_UPGRADE_FLAG_31_16_U16] >> 8);
    write_buf_for_app_info(u8TmpBuf, APP_INFO_SINGLE_DATA_LENGTH,
                             OFFSET_APP_INFO_APP_UPGRADE_FLAG);
    write_app_info();
    u32DataFlashAppUpgradeFlag = *((
        uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG + OFFSET_FLASH_ADDR_APP_INFO_START));
    u16Reg[OFFSET_UPGRADE_PROGRAM_FLAG_U16] =
        (uint16_t)u32DataFlashAppUpgradeFlag;
    u16Reg[OFFSET_APP_UPGRADE_FLAG_STATE_U16] = UPDATE_STATE_DONE;
    print_bootloader_info_from_app_info();
  }

  if ((u16Reg[OFFSET_UPGRADE_PROGRAM_FLAG_U16] != u32DataFlashAppUpgradeFlag) &&
      ((0XA == u16Reg[OFFSET_UPGRADE_PROGRAM_FLAG_U16]) ||
       (0XB == u16Reg[OFFSET_UPGRADE_PROGRAM_FLAG_U16]))) {
    uint8_t u8TmpBuf[4] = {u16Reg[OFFSET_UPGRADE_PROGRAM_FLAG_U16]};
    write_buf_for_app_info(u8TmpBuf, APP_INFO_SINGLE_DATA_LENGTH,
                           OFFSET_APP_INFO_APP_UPGRADE_FLAG);
    write_app_info();
    u32DataFlashAppUpgradeFlag =
        *((uint32_t *)(OFFSET_APP_INFO_APP_UPGRADE_FLAG +
                       OFFSET_FLASH_ADDR_APP_INFO_START));
    print_bootloader_info_from_app_info();
    if (0XA == u32DataFlashAppUpgradeFlag) {
      NVIC_SystemReset();
    }
  }

  if (UPDATE_STATE_START == u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_STATE_U16]) {
    u32SingleWFlashLength = u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16];
    u32SingleWFlashLength |= u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16]
                             << 16;
    u16Reg[OFFSET_SINGLE_W_FLASH_LENGTH_STATE_U16] = UPDATE_STATE_DONE;
    printf("u32SingleWFlashLength:= %#X\r\n", u32SingleWFlashLength);
  }

  if (UPDATE_STATE_START == u16Reg[OFFSET_UPDATE_APP_STATE_U16]) {

    if ((0 == u32Length) || (u32Length <= u32Offset)) {
      u16Reg[OFFSET_UPDATE_APP_STATE_U16] = UPDATE_STATE_IDLE;
      return;
    }

    int32_t i32Length = u32Length;
    int32_t i32Offset = u32Offset;
    int32_t i32SingleWFlashLength = u32SingleWFlashLength;
    int32_t i32RemainingLength = i32Length - i32Offset;

    uint32_t u32CurWFlashLength = 0;
    if (u32Length <= u32SingleWFlashLength) {
      u32CurWFlashLength = u32Length;
    } else {
      if (i32RemainingLength >= i32SingleWFlashLength) {
        u32CurWFlashLength = (uint32_t)i32SingleWFlashLength;
      } else {
        u32CurWFlashLength = (uint32_t)i32RemainingLength;
      }
    }

    printf("u32CurWFlashLength:= %08X\r\n", u32CurWFlashLength);
    if (u8DebugMsgFunc) {
      for (int i = 0; i < u32CurWFlashLength / 2; i++) {
        if (!((i + ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) & 0X7)) {
          printf("%08X:", i + ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET);
        }
        uint16_t u16RegVal =
            reg_val_get(i + ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET);
        printf("%02X ", (uint8_t)(u16RegVal));
        printf("%02X ", (uint8_t)(u16RegVal >> 8));
        if (0X7 == ((i + ADAPS_HAWK_MCU_FLASH_APP_REG_ADDR_OFFSET) & 0X7)) {
          printf("\r\n");
        }
      }

      printf("\r\n");
    }

    uint32_t u32ChecksumCalFromMcuReg = 0;
    u32ChecksumCalFromMcuReg =
        local_crc32(0, u8FlashAppReg, u32CurWFlashLength);
    printf("u32ChecksumCalFromMcuReg:= %#X\r\n", u32ChecksumCalFromMcuReg);

    if (0 == u32Offset) {
      uint32_t u32OffsetEraseSector = 0;
      while (u32OffsetEraseSector <= u32Length) {
        EraseSector(OFFSET_FLASH_ADDR_APP_START + u32OffsetEraseSector);
        u32OffsetEraseSector += ERASE_SECTOR_SINGLE_LENGTH;
      }
    }

    // EraseSector(OFFSET_FLASH_ADDR_APP_START + u32Offset);
    ProgramPage((OFFSET_FLASH_ADDR_APP_START + u32Offset), u32CurWFlashLength,
                u8FlashAppReg);

    if (u8DebugMsgFunc) {
      printFlash(OFFSET_FLASH_ADDR_APP_START + u32Offset, u32CurWFlashLength);
    }
    u32Offset += u32CurWFlashLength;

    for (int i = 0; i < OFFSET_LENGTH_MCU_FLASH_APP; i++) {
      u8FlashAppReg[i] = 0XFF;
    }

    u16Reg[OFFSET_UPDATE_APP_STATE_U16] = UPDATE_STATE_DONE;
    printf("u32Offset:= %#X\r\n", u32Offset);
  }
}

static uint32_t g_crc32_table[CRC32_TABLE_BUF_LEN] = {0};

void local_generate_crc32_table(void) {
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t crc = 0;

  for (i = 0; i < 256; i++) {
    crc = i;
    for (j = 8; j > 0; j--) {
      if (crc & 1) {
        crc = (crc >> 1) ^ POLYNOMIAL;
      } else {
        crc >>= 1;
      }
    }

    g_crc32_table[i] = crc;
  }
}

uint32_t local_crc32(uint32_t initial, uint8_t *buf, size_t len) {
  uint32_t c = initial ^ 0xFFFFFFFF;
  uint32_t i = 0;
  for (i = 0; i < len; i++) {
    c = g_crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
  }

  return c ^ 0xFFFFFFFF;
}
