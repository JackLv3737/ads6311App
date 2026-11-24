/***********************************************************************************************************************
 * Copyright (C) . All rights reserved.
 ***********************************************************************************************************************/

/***********************************************************************************************************************
 * File Name    : adaps_ina201_driver.c
 * Version      : 1.0.0
 * Description  : This file is application for ina201 dirver
 * Creation Date: 2023/09/25
 * Author       : junjie.liang
 ***********************************************************************************************************************/
#include <stdio.h>
#include "board_config.h"
#include "bsp_gpio.h"
#include "bsp_basic.h"
#include "adaps_ina201_driver.h"
#include "bsp_adc.h"
#include "adaps_soc_protocol.h"

#define CHECK_TIME_WINDOW   10000 //unit:ms

static uint8_t sU8AlarmCount = 0;

static uint16_t sU16InaSafetyVoltageValue = INA_SAFETY_VOLTAGE_VALUE;

static uint8_t sU8InaResetMaxValue = INA_RESET_MAX_VALUE;

static uint32_t s_alarm_start_time = 0;

uint32_t ina201_cmp_result(void)
{
    return adc_get_result(ADC_CHANNEL_CURRENT_PVDD);
}
void ina201_reset(void)
{
    gpio_write(INA_RST_PORT, INA_RST_PIN, GPIO_PIN_SET);
    delay_ms(1);
    gpio_write(INA_RST_PORT, INA_RST_PIN, GPIO_PIN_RESET);
    delay_ms(INA_RESET_KEEP_TIME);
    gpio_write(INA_RST_PORT, INA_RST_PIN, GPIO_PIN_SET);
}
GPIO_PinState ina201_get_alarm_flag(void)
{

    return gpio_read(INA_CMP_PORT, INA_CMP_PIN);
}

void ina201_update(void)
{
    float result;
    if (GPIO_PIN_SET == ina201_get_alarm_flag()){
        result = (float)ina201_cmp_result();
        result = result / INA_ADC_MAX * INA_VREF;
    
        if (get_dbgflag() & ADAPS_DBG_TRACE_INA_OUT_UPDATE){
            printf("ina201 alarm flag = %d, vol result = %d\r\n", ina201_get_alarm_flag(), result);
        }

        if (ina201_get_ina_reset_max_value() > sU8AlarmCount){
            ina201_reset();
            if(sU8AlarmCount == 0){
                s_alarm_start_time = bsp_tick_get();
            }
            sU8AlarmCount++;
            printf("[tm%u]ina201 reset, reset count %d, vol result = %d\r\n", bsp_tick_get(), sU8AlarmCount, result);
        }else if (INA_RESET_MAX_VALUE == sU8AlarmCount){
            reg_control_set(OFFSET_PVDD, POWER_DISABLE);
            reg_control_set(OFFSET_ERR_CODE, (reg_val_get(OFFSET_ERR_CODE) | ADAPS_PVDD_PEAK_CURRENT_OVER_ERR));
            sU8AlarmCount++;
            printf("[tm%u]ina201 reset max times reach, shutdown pvdd \r\n", bsp_tick_get());
        }
    }else{
        if (get_dbgflag() & ADAPS_DBG_TRACE_INA_OUT_UPDATE){
            printf("ina201_get_alarm_flag:= 0\r\n");
        }
    }

    if(sU8AlarmCount && (sU8AlarmCount < INA_RESET_MAX_VALUE)
    && ((bsp_tick_get() - s_alarm_start_time) > CHECK_TIME_WINDOW)){
        printf("[tm%u]ina201: reset alarm times(%d) as out of check time window\r\n", bsp_tick_get(), sU8AlarmCount);
        ina201_reset();
        sU8AlarmCount = 0;
    }
    
}

uint8_t ina201_get_reset_value(void)
{
    return sU8AlarmCount;
}
void ina201_set_reset_value(uint8_t value)
{
    sU8AlarmCount = value;
}

uint16_t ina201_get_ina_safety_voltage_value(void)
{
    return sU16InaSafetyVoltageValue;
}
void ina201_set_ina_safety_voltage_value(uint16_t value)
{
    sU16InaSafetyVoltageValue = value;
}

uint8_t ina201_get_ina_reset_max_value(void)
{
    return sU8InaResetMaxValue;
}

void ina201_set_ina_reset_max_value(uint8_t value)
{
    sU8InaResetMaxValue = value;
}
