/**
 * @file bsp_i2c.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_I2C_H__
#include <stdint.h>

#define I2C_DATA_MAX_LENGTH     100
#define I2C_DATA_MIN_LENGTH     0
/**
 * @brief bsp i2c init
 * 
 * @return int 
 */
int bsp_i2c_init(void);

int bsp_i2c_write(uint8_t chn, uint8_t DevAddr, uint8_t regaddr, uint8_t *pbuf, uint16_t datalen);

int bsp_i2c_read(uint8_t chn, uint8_t DevAddr, uint8_t regaddr, uint8_t *pbuf, uint16_t datalen);

#define i2c_write(m_DevAddr, m_regaddr, m_pbuf, m_datalen) \
    bsp_i2c_write(0, m_DevAddr, m_regaddr, m_pbuf, m_datalen)

#define i2c_read(m_DevAddr, m_regaddr, m_pbuf, m_datalen) \
    bsp_i2c_read(0, m_DevAddr, m_regaddr, m_pbuf, m_datalen)


#endif /*__BSP_I2C_H__*/
