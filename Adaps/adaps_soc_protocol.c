/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_soc_protocol.c
* Version      : 1.0.0
* Description  : This file is for the protocol between soc
* Creation Date: 2023/05/30
* Author       : wang.yi
***********************************************************************************************************************/
#include <stdio.h>
#include "board_config.h"
#include "system_config.h"
#include "bsp_pwm.h"
#include "bsp_gpio.h"
#include "bsp_timer.h"
#include "adaps_pid.h"
#include "adaps_pmic_driver.h"
#include "adaps_tx_switch.h"
#include "adaps_soc_protocol.h"


#define REG_SIZE    (REG_MAX + 1)
static int16_t reg[REG_SIZE] = {
BOARD_MCU_MODEL
};

static uint8_t s_reg_flag[REG_SIZE] = {0};

void reg_set_val(uint16_t index, int16_t val)
{
    if (index < REG_SIZE)
    {
        reg[index] = val;
        s_reg_flag[index] |= REG_FLAG_UPDATE;
    }
}

void reg_set_val_not_set_flag(uint16_t index, int16_t val)
{
    if (index < REG_SIZE){
        reg[index] = val;
    }
}

void reg_status_update(uint16_t index, int16_t val)
{
    if (index < CONTROL_REG_BASE)
    {
        if(reg[index] != val){
            reg[index] = val;
            s_reg_flag[index] |= REG_FLAG_UPDATE;
        }
    }
}

void reg_err_code_update(uint16_t errcode, uint16_t is_set)
{
    if(is_set){
        reg[OFFSET_ERR_CODE] |= errcode;
    }else{
        reg[OFFSET_ERR_CODE] &= (~errcode);
    }
    s_reg_flag[OFFSET_ERR_CODE] |= REG_FLAG_UPDATE;
}

void reg_warn_code_update(uint16_t warncode, uint16_t is_set)
{
    if(is_set){
        reg[OFFSET_WARN_CODE] |= warncode;
    }else{
        reg[OFFSET_WARN_CODE] &= (~warncode);
    }
    s_reg_flag[OFFSET_WARN_CODE] |= REG_FLAG_UPDATE;
}


int16_t reg_val_get(uint16_t index)
{
    int16_t val = 0;

    if (index <= REG_MAX)
    {
        val = reg[index];
    }

    return val;
}

void reg_control_set(uint16_t index, int16_t val)
{
    if (index >= CONTROL_REG_BASE && index <= REG_MAX)
    {
        reg[index] = val;
        s_reg_flag[index] |= REG_FLAG_CTRL;
        switch (index)
        {
            case OFFSET_VOL_PVDD:
                if(PVDD_MIN_VAL > val){
                    val = PVDD_MIN_VAL;
                    reg[index] = PVDD_MIN_VAL;
                }
                set_target_pvdd(val);
                break;
            case OFFSET_VOL_VOP:
                val = val * (-1);
                set_target_VOP(val);
                break;
            case OFFSET_VCSEL_ENABLE:
                break;
            case OFFSET_TX_FIXED_CHN:
                txswt_set_fixed_chn(val);
                break;
            case OFFSET_TX_FIXED_CHN2:
            case OFFSET_TX_FIXED_CHN3:
            case OFFSET_TX_FIXED_CHN4:
            case OFFSET_TX_FIXED_CHN5:
            case OFFSET_TX_FIXED_CHN6:
            case OFFSET_TX_FIXED_CHN7:
            case OFFSET_TX_FIXED_CHN8:
                txswt_config_fixed_chn(index + 1 - OFFSET_TX_FIXED_CHN2, val);
                break;
            case OFFSET_TX_FIXED_CHN9:
            case OFFSET_TX_FIXED_CHN10:
            case OFFSET_TX_FIXED_CHN11:
            case OFFSET_TX_FIXED_CHN12:
            case OFFSET_TX_FIXED_CHN13:
            case OFFSET_TX_FIXED_CHN14:
            case OFFSET_TX_FIXED_CHN15:
            case OFFSET_TX_FIXED_CHN16:
                txswt_config_fixed_chn(index + 8 - OFFSET_TX_FIXED_CHN9, val);
                break;
            case OFFSET_TX_HIT_CHANNEL:
                txswt_set_hit_chn(val);
            break;
            case OFFSET_DV11:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(DV11_EN_PORT,DV11_EN,(GPIO_PinState)val);
                break;
            case OFFSET_SV11:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(SV11_EN_PORT,SV11_EN,(GPIO_PinState)val);
                break;
            case OFFSET_AV11:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(AV11_EN_PORT,AV11_EN,(GPIO_PinState)val);
                break;
            case OFFSET_MP_VCC11A:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(MP_VCC11A_EN_PORT, MP_VCC11A_EN, (GPIO_PinState)val);
                break;
            case OFFSET_IOV18:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(IOV18_EN_PORT, IOV18_EN, (GPIO_PinState)val);
                break;
            case OFFSET_MP_VCC18A:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(MP_VCC18_EN_PORT, MP_VCC18_EN, (GPIO_PinState)val);
                break;
            case OFFSET_IOV33_DRV:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(IOV33_DRV_EN_PORT, IOV33_DRV_EN, (GPIO_PinState)val);
                break;
            case OFFSET_IOV33_TRIG:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(IOV33_TRIG_EN_PORT, IOV33_TRIG_EN, (GPIO_PinState)val);
                break;
            case OFFSET_AV33:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(AV33_EN_PORT, AV33_EN, (GPIO_PinState)val);
                break;
            case OFFSET_SV33:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(SV33_EN_PORT, SV33_EN, (GPIO_PinState)val);
                break;
            case OFFSET_VCC_3V3:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(VCC_3V3_EN_PORT, VCC_3V3_EN, (GPIO_PinState)val);
                break;
            case OFFSET_VOP:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(VOP_EN_PORT, VOP_EN, (GPIO_PinState)val);
                break;
            case OFFSET_PVDD:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(PVDD_EN_PORT, PVDD_EN, (GPIO_PinState)val);
                break;
            case OFFSET_POWER_CTRL_ALL:
                // we need pull down RX_DRV_ERR gpio pin when power down to avoid power leak.
                // please refer to http://support.adapsphotonics.com/issues/2729 for detailed info.
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(RX_DRV_ERR_PORT, RX_DRV_ERR_PIN, (GPIO_PinState)val);
                break;   
            case OFFSET_HEART_BEAT_SWITCH:
                break;
            case OFFSET_VCC_5V_DRV:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(VCC_5V_DRV_EN_PORT, VCC_5V_DRV_EN, (GPIO_PinState)val);
                break;
            case OFFSET_UPGRADE_PROGRAM_FLAG_U16:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                break;
            case OFFSET_ERR_ACK:
                gpio_write(RK_ERR_PORT, RK_ERR_PIN, GPIO_PIN_RESET);
                break;
            case OFFSET_PVDDL_EN:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(PVDDL_EN_PORT, PVDDL_EN_PIN, val);
                break;
            case OFFSET_BMI_EN:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(BMI_EN_PORT, BMI_EN_PIN, val);
                break;
            case OFFSET_ADS6311_I2C_EN:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(ADS6311_I2C_EN_PORT, ADS6311_I2C_EN_PIN, val);
                break;
            case OFFSET_ADS6311_EN:
                val = val ? GPIO_PIN_SET : GPIO_PIN_RESET;
                gpio_write(ADS6311_EN_PORT, ADS6311_EN_PIN, val);
                break;
            default:
                break;
        }
    } else if (index <= CONTROL_REG_BASE){
        if(index == OFFSET_ERR_CODE){
            //往0x06寄存器写0，退出保护模式
            if(val == 0){
                s_reg_flag[OFFSET_ERR_CODE] |= REG_FLAG_CTRL;
            }
        }else{
            reg[index] = val;
            s_reg_flag[index] |= REG_FLAG_UPDATE;
        }
    }
}

void reg_dump(void)
{
    uint16_t val;
    printf("\r\n");
    printf("-----");
    for (int i = 0; i < 0x10; i++) {
        printf("----%x", i);
    }
    printf("\r\n");
    for (int i = 0; i < 0x10; i++) {
        printf("%04x:", (i << 4) );
        for (int j = 0; j < 0x10; j++) {
            val = (uint16_t)reg[i*0x10+j];
            printf(" %04X",val);
        }
        printf("\r\n");
    }
    for(int i=0;i<85;i++)
        printf("=");

    printf("\r\n");
}

uint16_t get_dbgflag(void)
{
    return reg[OFFSET_DEBUG];
}

void set_dbgflag(uint16_t val)
{
    reg[OFFSET_DEBUG] = val;
}

uint8_t reg_get_flag(uint16_t index)
{
    if (index <= REG_MAX){
        return s_reg_flag[index];
    }
    return 0;
}

uint8_t reg_reset_flag(uint16_t index, uint8_t mask)
{
    s_reg_flag[index] &= (~mask);
}


