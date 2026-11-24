/**
 * @file bsp_adc.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_ADC_H__
#include <stdint.h>
#include "board_config.h"


#define ADC_AVERAGE_TIMES_MAX 100
#define ADC_AVERAGE_TIMES_MIN 0

#define ADC_INIT_ALL    0xFFFFFFFF


int bsp_adc_init(uint32_t init_chn_mask);
uint32_t bsp_adc_get_result(uint32_t ch);
uint32_t bsp_adc_get_result_average(uint32_t ch, uint8_t times);


#define adc_get_result(m_ch) bsp_adc_get_result(m_ch)
#define adc_get_result_average(m_ch, m_times) bsp_adc_get_result_average(m_ch, m_times)


#endif /*__BSP_ADC_H__*/
