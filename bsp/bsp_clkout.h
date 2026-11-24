/**
 * @file bsp_clkout.h
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdint.h>

enum bsp_clkout_channel_e
{
    BSP_CLKOUT_CHN0 = 0,
    BSP_CLKOUT_CHN1
};


int bsp_clkout_init(void);
int bsp_clkout_enable(uint8_t channel, uint8_t enable);


