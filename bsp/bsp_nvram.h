/**
 * @file bsp_nvram.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2024-03-06
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef __BSP_NVRAM_H__
#include <stdint.h>

typedef enum bsp_nvram_partition_id_e
{
    BSP_NVRAM_PARTITION1 = 0,
    BSP_NVRAM_PARTITION2,
    BSP_NVRAM_PARTITION3,
}bsp_nvram_partition_id_t;


int bsp_nvram_init(void);

int bsp_nvram_read(uint32_t partition_id, uint32_t offset, uint8_t *buf, uint32_t size);
int bsp_nvram_write(uint32_t partition_id, uint32_t offset, uint8_t *buf, uint32_t size);



#endif /*__BSP_NVRAM_H__*/

