/**
 * @file bsp_spi.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_SPI_H__
#include <stdint.h>


/**
 * @brief bsp spi init
 * 
 * @return int 
 */
int bsp_spi_init(void);
int bsp_spi_write_data(uint8_t channel, uint8_t *pdata, uint16_t size);
int bsp_spi_read_data(uint8_t channel, uint8_t *pdata, uint16_t size);
int bsp_spi_write_and_read_data(uint8_t channel, uint8_t *pdata_w, uint8_t *pdata_r, uint16_t size);

int bsp_spi_lock(uint8_t channel, uint8_t cs_keep_active_flag);
int bsp_spi_unlock(uint8_t channel);

#define spi_write_data bsp_spi_write_data
#define spi_read_data bsp_spi_read_data

#define spi_write_and_read_data bsp_spi_write_and_read_data



#endif /*__BSP_SPI_H__*/
