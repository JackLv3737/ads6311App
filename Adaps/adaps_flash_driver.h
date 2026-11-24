/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_flash_driver.h
* Version      : 1.0.0
* Description  : head file for adaps_py25q40_driver.c
* Creation Date: 2023/08/04
* Author       : 
***********************************************************************************************************************/
#ifndef ADAPS_PY25Q40_H
#define ADAPS_PY25Q40_H
#include <stdint.h>

#define PY25Q40_CMDS_WREN                       0x06        //write enable
#define PY25Q40_CMDS_WRDI                       0x04        //write disable

#define PY25Q40_CMDS_RDSR                       0x05        //read status register
#define PY25Q40_CMDS_WRSR                       0x01        //write status register   

#define PY25Q40_CMDS_PP                         0x02        //page program
#define PY25Q40_CMDS_READ                       0x03        //read data bytes

#define PY25Q40_CMDS_CE                         0xC7        //chip erase
#define PY25Q40_CMDS_SE                         0x20        //sector erase

#define TESTADDR                                0
#define TESESECTORADDR                          0x0007F000

#define FLASH_OP_SIZE_MAX   8192

#define PY25Q40_PAGE_SIZE                       256
#define PY25Q40_SECTOR_SIZE                     4096
#define SPI_MAX_TRANLEN                         0x8000
#define FLASH_CHECKSUM_SIZE                     1
#define FLASH_FLAG_READ                         1
#define FLASH_FLAG_WRITE                        0
#define FLASH_DATA_MAX_SIZE                     512
#define FLASH_MAX_SIZE_ADDR                     0x08


void adaps_flash_chipid(void);

void adaps_flash_write(uint32_t addr, uint8_t *data, uint16_t len);
void adaps_flash_read(uint32_t addr, uint8_t *data, uint16_t len);
void adaps_flash_chiperase(void);
void adaps_flash_sectorerase(uint32_t addr);
void adaps_flash_write_page(uint32_t address, uint8_t *buf, uint16_t count);
void adaps_flash_write_pages(uint32_t address, uint8_t *buf, uint32_t count);
void adaps_flash_write_more_bytes(uint32_t address, uint8_t *buf, uint32_t count);
void adaps_flash_read_more_bytes(uint32_t address, uint8_t *buf, uint32_t count);
int adaps_flash_need_erase(uint8_t *addr, uint8_t *data, uint32_t len);
int adaps_flash_op_config(uint8_t flag, uint8_t *buf);
int adaps_flash_op_exec(void);
int adaps_flash_op_state(void);

#endif
