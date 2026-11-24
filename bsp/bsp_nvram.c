/**
 * @file bsp_nvram.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2024-03-06
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stdio.h>
#include <string.h>
#include "BAT32A239.h"
#include "userdefine.h"
#include "board_config.h"
#include "bsp_nvram.h"
#include "flash.h"



//data flash
#define PART1_SECTOR_SIZE   512
#define PART1_ADDR_START    0x500200
#define PART1_SIZE  PART1_SECTOR_SIZE //one sector size : 512 bytes

static int bsp_nvram_part1_read(uint32_t offset, uint8_t *buf, uint32_t size)
{   
    if((offset + size) > PART1_SIZE){
        return -1;
    }
    memcpy(buf, (char*)(PART1_ADDR_START + offset), size);
    return 0;
}

static int bsp_nvram_part1_write(uint32_t offset, uint8_t *buf, uint32_t size)
{
    int ret;
    
    if((offset + size) > PART1_SIZE){
        return -1;
    }

    ret = flash_write(PART1_ADDR_START + offset, size, buf);
    if(MD_OK != ret){
        printf("part1 flash write fail, ret = %d\n", ret);
        return -1;
    }

    return 0;
    
}

int bsp_nvram_init(void)
{
    return 0;
}

int bsp_nvram_read(uint32_t partition_id, uint32_t offset, uint8_t *buf, uint32_t size)
{
    int ret = -1;
    if(NULL == buf){
        return -1;
    }
    switch(partition_id)
    {
        case BSP_NVRAM_PARTITION1:
            ret = bsp_nvram_part1_read(offset, buf, size);
        break;
        default:

        break;
    }
    
    return ret;
}


int bsp_nvram_write(uint32_t partition_id, uint32_t offset, uint8_t *buf, uint32_t size)
{
    int ret = -1;
    if(NULL == buf){
        return -1;
    }
    switch(partition_id)
    {
        case BSP_NVRAM_PARTITION1:
            ret = bsp_nvram_part1_write(offset, buf, size);
        break;
        default:

        break;
    }
    
    return ret;
}
