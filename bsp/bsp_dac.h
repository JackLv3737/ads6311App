/**
 * @file bsp_dac.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-28
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#ifndef __BSP_ADC_H__
#include <stdint.h>


int bsp_dac_init(void);

int bsp_dac_set_value(uint16_t val);


#endif //__BSP_ADC_H__
