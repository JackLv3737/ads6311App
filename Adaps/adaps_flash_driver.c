/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_flash_driver.c
* Version      : 1.0.0
* Description  : This file is for py25q40 dirver
* Creation Date: 2023/08/04
* Author       : 
***********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "adaps_flash_driver.h"
#include "bsp_spi.h"
#include "board_config.h"

//#define FLASH_SIMULATE

#define FLASH_LEN_ADDR          0x92
#define FLASH_START             0x93
#define FLASH_DEV_ADDR_H        0x90
#define FLASH_DEV_ADDR_L        0x91
#define FLASH_STATE_ADDR        0x07
#define FLASH_STATE_DONE        0x00
#define FLASH_STATE_BUSY        0x01
#define FLASH_STATE_READY       0x02

#define SPI_CHIP_PY25Q40 FLASH_SPI_CHAN


#ifdef FLASH_SIMULATE
uint8_t s_simu_buf[4096]; /*数据缓冲区*/
#endif

// test list : PY25Q40
static uint8_t s_w_buf[PY25Q40_SECTOR_SIZE]; /*数据缓冲区*/

uint8_t adaps_flash_busy(void)
{
    uint8_t status;
    uint8_t cmd = PY25Q40_CMDS_RDSR;
    uint8_t ret = 0;

    bsp_spi_lock(SPI_CHIP_PY25Q40, 1);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    spi_read_data(SPI_CHIP_PY25Q40, &status, sizeof(status));
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
    ret = status & 0x1;

    return ret;
}

void adaps_flash_write_enable(void)
{
    uint8_t cmd = PY25Q40_CMDS_WREN;

    bsp_spi_lock(SPI_CHIP_PY25Q40, 0);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
}

void adaps_flash_write_disable(void)
{
    uint8_t cmd = PY25Q40_CMDS_WRDI;
    bsp_spi_lock(SPI_CHIP_PY25Q40, 0);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd)); 
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
}

void adaps_flash_write(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint8_t cmd = PY25Q40_CMDS_PP;
    uint8_t sendaddr[3];

    sendaddr[0] = (uint8_t)((addr >> 16) & 0xFF); 
    sendaddr[1] = (uint8_t)((addr >> 8) & 0xFF); 
    sendaddr[2] = (uint8_t)( addr & 0xFF); 
    while(adaps_flash_busy());
    
    adaps_flash_write_enable();

    bsp_spi_lock(SPI_CHIP_PY25Q40, 1);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    spi_write_data(SPI_CHIP_PY25Q40, sendaddr, sizeof(sendaddr));
    spi_write_data(SPI_CHIP_PY25Q40, data, len);
    bsp_spi_unlock(SPI_CHIP_PY25Q40);

    adaps_flash_write_disable();
}

void adaps_flash_read(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint8_t cmd = PY25Q40_CMDS_READ;
    uint8_t sendaddr[3];

    sendaddr[0] = (uint8_t)((addr >> 16) & 0xFF); 
    sendaddr[1] = (uint8_t)((addr >> 8) & 0xFF); 
    sendaddr[2] = (uint8_t)( addr & 0xFF); 

    while(adaps_flash_busy());

    //gpio_write(GPIOB, GPIO_PIN_12, RESET);
    bsp_spi_lock(SPI_CHIP_PY25Q40, 1);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    spi_write_data(SPI_CHIP_PY25Q40, sendaddr, sizeof(sendaddr));
    spi_read_data(SPI_CHIP_PY25Q40, data, len);
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
    //gpio_write(GPIOB, GPIO_PIN_12, SET);
}

/*This func would take max 3s*/
void adaps_flash_chiperase(void)
{
    uint8_t cmd = PY25Q40_CMDS_CE; 

    adaps_flash_write_enable();
    while(adaps_flash_busy());
    bsp_spi_lock(SPI_CHIP_PY25Q40, 0);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
    while(adaps_flash_busy());
    adaps_flash_write_disable();
}

void adaps_flash_sectorerase(uint32_t addr)
{
    uint8_t cmd = PY25Q40_CMDS_SE; 
    uint8_t sendaddr[3];

    sendaddr[0] = (uint8_t)((addr >> 16) & 0xFF); 
    sendaddr[1] = (uint8_t)((addr >> 8) & 0xFF); 
    sendaddr[2] = (uint8_t)( addr & 0xFF); 

    adaps_flash_write_enable();
    while(adaps_flash_busy());
    bsp_spi_lock(SPI_CHIP_PY25Q40, 1);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    spi_write_data(SPI_CHIP_PY25Q40, sendaddr, sizeof(sendaddr));
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
    while(adaps_flash_busy());
    adaps_flash_write_disable();
}

void adaps_flash_chipid(void)
{
    uint8_t txbuf[3] = {0};
    uint8_t rxbuf[2] = {0};
    uint8_t cmd = 0x90;  

    bsp_spi_lock(SPI_CHIP_PY25Q40, 1);
    spi_write_data(SPI_CHIP_PY25Q40, &cmd, sizeof(cmd));
    spi_write_data(SPI_CHIP_PY25Q40, txbuf, sizeof(txbuf));
    spi_read_data(SPI_CHIP_PY25Q40, rxbuf, sizeof(rxbuf));
    bsp_spi_unlock(SPI_CHIP_PY25Q40);
    printf("flash chip id %d\n", *(uint16_t*)rxbuf);
}

void adaps_flash_test(void)
{
    char w_buf[100];
    char r_buf[100];

    // adaps_py25q40_chipid();

    memset(w_buf, 0, sizeof(w_buf));
    memset(r_buf, 0, sizeof(r_buf));

    for (uint8_t i = 0; i < 100; i++)
    {
        w_buf[i] = i;
        printf("%d ",w_buf[i]);
    }
    printf("\n");
    adaps_flash_chiperase();
    adaps_flash_write(TESTADDR, w_buf, sizeof(w_buf));
    adaps_flash_read(TESTADDR, r_buf, sizeof(r_buf));

    for (uint8_t i = 0; i < 100; i++)
    {
        printf("%d ",r_buf[i]);
    }
    printf("\n");

    adaps_flash_sectorerase(TESESECTORADDR);
    adaps_flash_write(TESESECTORADDR, w_buf, sizeof(w_buf));
    adaps_flash_read(TESESECTORADDR, r_buf, sizeof(r_buf));

    printf("setctor w/r test:\n");
    for (uint8_t i = 0; i < 100; i++)
    {
        printf("%d ",r_buf[i]);
    }
    printf("\n");
}



void adaps_flash_write_page(uint32_t address, uint8_t *buf, uint16_t count) {
    adaps_flash_write( address, buf, count);
}

/*写1个扇区内的多个页，address是该扇区内任意地址，address+count应在该扇区内*/
void adaps_flash_write_pages(uint32_t address, uint8_t *buf, uint32_t count) {
    uint32_t remain_of_page, need_to_write;
    uint32_t sector_first_address, sector_offset;
    uint8_t *buf_ptr;


    /*获取指定地址所在扇区的扇区首地址*/
    sector_first_address = address & (~(PY25Q40_SECTOR_SIZE - 1));

    /*获取指定地址在所在扇区内的偏移量*/
    sector_offset = address % PY25Q40_SECTOR_SIZE;

    // 读出整个扇区
    adaps_flash_read_more_bytes(sector_first_address, s_w_buf,PY25Q40_SECTOR_SIZE);

    /*判断是否需要擦除*/
    if (adaps_flash_need_erase(&s_w_buf[sector_offset], buf, count)) {
        adaps_flash_sectorerase(sector_first_address);
        memcpy(&s_w_buf[sector_offset], buf, count);
        address = sector_first_address;
        count = PY25Q40_SECTOR_SIZE;
        remain_of_page = PY25Q40_PAGE_SIZE;
        need_to_write = remain_of_page;
        buf_ptr = s_w_buf;
    }
    else{
        // 获取本页还剩多少个字节空间可写入
        remain_of_page = PY25Q40_PAGE_SIZE - address % PY25Q40_PAGE_SIZE;
        /*下一次最多可写remain_of_page个字节*/
        need_to_write = remain_of_page;
        buf_ptr = buf;
    }

    //printk("sector_first_address=%d,sector_offset=%d\n", sector_first_address, sector_offset);
    //printk("address=%d,count=%d\n", address, count);

    if (count <= need_to_write) {
        /*需要写入的字节数少于剩余空间  直接写入实际字节数*/
        adaps_flash_write_page(address, buf_ptr, count);
    } else {
        do {
            //printk("address=%d\n,need_to_write=%d\n", address, need_to_write);
            adaps_flash_write_page(address, buf_ptr, need_to_write);
            if (need_to_write == count) {
                break;
            } else {
                buf_ptr += need_to_write;
                address += need_to_write;
                count -= need_to_write;
                if (count > PY25Q40_PAGE_SIZE) {
                    need_to_write = PY25Q40_PAGE_SIZE;
                } else {
                    need_to_write = count;
                }
            }
        } while (1);
    }
}

/*address为任意地址，count为任意长度*/
void adaps_flash_write_more_bytes(uint32_t address, uint8_t *buf, uint32_t count) {  
#ifdef FLASH_SIMULATE
    address = address % sizeof(s_simu_buf);
    memcpy(s_simu_buf + address, buf, count);
#else
    uint32_t num_of_sector, remain_of_sector, sector_offset;
    uint32_t need_to_write;  // sector_first_address

    /*计算起始sector索引*/
    num_of_sector = address / PY25Q40_SECTOR_SIZE;
    /*计算在起始sector中的偏移量*/
    sector_offset = address % PY25Q40_SECTOR_SIZE;
    /*当前地址所在扇区 还剩下多少空间*/
    remain_of_sector = PY25Q40_SECTOR_SIZE - sector_offset;

    need_to_write = remain_of_sector;

    if (count <= need_to_write) {//所写数据都在起始sector内
        adaps_flash_write_pages(address, buf, count);
    } else {
        do {
            adaps_flash_write_pages(address, buf, need_to_write);
            if (need_to_write == count) {
                break;
            } else {
                buf += need_to_write;
                address += need_to_write;
                count -= need_to_write;
                if (count > PY25Q40_SECTOR_SIZE) {
                    need_to_write = PY25Q40_SECTOR_SIZE;
                } else {
                    need_to_write = count;
                }
            }
        } while (1);
    }
#endif
}


void adaps_flash_read_more_bytes(uint32_t address, uint8_t *buf, uint32_t count)
{
#ifdef FLASH_SIMULATE
    address = address % sizeof(s_simu_buf);
    memcpy(buf, s_simu_buf + address, count);
#else
    uint32_t read_addr = address;
    uint8_t *rx_buf = buf;
    uint16_t read_len;
    uint32_t read_left = count;

    while(read_left > 0){
      read_len = (SPI_MAX_TRANLEN < read_left ? SPI_MAX_TRANLEN : read_left);

      adaps_flash_read(read_addr, rx_buf, read_len);

      read_left -= read_len;
      read_addr += read_len;
      rx_buf += read_len;
    }
#endif
}

int adaps_flash_need_erase(uint8_t *addr, uint8_t *data, uint32_t len)
{
    int i;
    uint8_t p;
    for( i = 0; i < len; i++)
    {
        p = *addr++;
        p = ~p;
        if(( p & (*data++) ) !=0)
            return 1;
    }

    return 0;
}


#if 0
//占用内存过大，先屏蔽
int adaps_flash_test_02()
{
    #define TEST_SIZE_MAX   6000
    static uint8_t s_write_buf[TEST_SIZE_MAX];/*数据缓冲区*/
    static uint8_t s_read_buf[TEST_SIZE_MAX];
    memset(s_write_buf,0,sizeof(s_write_buf));
    memset(s_read_buf,0,sizeof(s_read_buf));
    uint32_t count = rand()%(TEST_SIZE_MAX);    //最大为10个sector size
    uint32_t test_addr = rand()%0x3FFFF;    //起始地址为前3个block   
     printf("count %d  ", count);
    // printf("test_addr %d  ", test_addr);
    for (uint32_t i = 0; i < count; i++) {
        s_write_buf[i] = rand()%0x100;
    }

    // /*打印数据*/
    // printf("write data: ");
    // for (uint32_t i = 0; i < count; i++) {
    //     printf("%d ",s_write_buf[i]);
    // }
    // printf("\n");

    adaps_flash_write_more_bytes(test_addr, s_write_buf, count);
    adaps_flash_read_more_bytes(test_addr, s_read_buf, count);

    /*打印数据*/
    // printf("read data: ");
    // for (uint32_t i = 0; i < count; i++) {
    //     printf("%d ",s_read_buf[i]);
    // }
    // printf("\n");

    for(uint32_t i = 0; i < count; i++){
        if(s_write_buf[i] != s_read_buf[i]){
            printf("test false, data[%d] expect %d, but %d\n", i, s_write_buf[i], s_read_buf[i]);
            return -1;
            break;
        }
    }
    printf("flash test success \n");
    return 1;
}
#endif
