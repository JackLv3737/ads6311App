/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_ina201_driver.h
* Version      : 1.0.0
* Description  : head file for adaps_ina201_driver.c
* Creation Date: 2023/09/25
* Author       : junjie.liang
***********************************************************************************************************************/

#ifndef __ADAPS_INA201_H__
#define __ADAPS_INA201_H__

#include <stdbool.h>

#define INA_VREF                                    VREF
#define INA_ADC_MAX                                 4096
#define INA_SAFETY_VOLTAGE_VALUE                    2000 //100mv
#define INA_RESET_MAX_VALUE                         3

uint32_t ina201_cmp_result(void);
void ina201_reset(void);
GPIO_PinState ina201_get_alarm_flag(void);
void ina201_update(void);
uint8_t ina201_get_reset_value(void);
void ina201_set_reset_value(uint8_t value);

uint16_t ina201_get_ina_safety_voltage_value(void);
void ina201_set_ina_safety_voltage_value(uint16_t value);

uint8_t ina201_get_ina_reset_max_value(void);
void ina201_set_ina_reset_max_value(uint8_t value);

#endif /* __ADAPS_INA201_H__ */
