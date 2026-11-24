/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_pmic_driver.h
* Version      : 1.0.0
* Description  : head file for adaps_pmic_driver.c
* Creation Date: 2023/04/24
* Author       : wang.yi
***********************************************************************************************************************/
#ifndef ADAPS_PMIC_H
#define ADAPS_PMIC_H

#include <stdint.h>

enum adaps_pmic_state_e
{
    ADAPS_PMIC_STATE_UNKNOWN = 0,
    ADAPS_PMIC_POWER_UP_ING,
    ADAPS_PMIC_POWER_UP_DONE,
    ADAPS_PMIC_POWER_DOWN_ING,
    ADAPS_PMIC_POWER_DOWN_DONE,
};

typedef enum wave_state_e{
    WAVE_STATE_UNKNOWN  = 0,
    //形态
    WAVE_STATE_STABLE,   //稳定的
    WAVE_STATE_UNSTABLE, //上下波动超出阈值的
    WAVE_STATE_RISING,   ///绝对值上升的
    WAVE_STATE_FALLING,  ///绝对值下降的

    //与目标电压的关系
    WAVE_STATE_RELATE_MASK = 0xFF,
    WAVE_STATE_NEAR = (1 << 8), //在目标电压的容忍误差范围内
    WAVE_STATE_BELOW = (2 << 8), //绝对值在目标电压之下
    WAVE_STATE_ABOVE = (3 << 8), //绝对值在目标电压之上
    WAVE_STATE_CROSS = (4 << 8), //与目标电压有交叉

    WAVE_STATE_STABLE_NEAR = WAVE_STATE_STABLE | WAVE_STATE_NEAR,
    WAVE_STATE_UNSTABLE_NEAR = WAVE_STATE_UNSTABLE | WAVE_STATE_NEAR,

}wave_state_t;

#define PMIC_CHIP_ADDR                              0x1E

void pmic_write_byte(uint8_t reg, uint8_t value);
void pmic_read_byte(uint8_t reg, uint8_t* value);
void pmic_powerup_init(void);
void pmic_powerup_all(void (*power_ctrl_cb)(void));
void pmic_powerdown_all(void (*power_ctrl_cb)(void));


void pmic_power_all_update_flag_set(uint8_t flag);

uint8_t pmic_power_all_update_flag_get(void);

void pmic_power_all_update(int16_t val);

uint8_t pmic_get_state(void);
char* pmic_get_state_string(uint8_t state);
uint32_t pmic_powerup_done_time(void);

uint32_t pmic_sample_dv11_vol(uint32_t *err_flag_out);
uint32_t pmic_sample_sv11_vol(uint32_t *err_flag_out);

uint32_t pmic_sample_pvdd_vol(uint32_t *adc_val, uint8_t is_record);
uint32_t pmic_sample_pvdd2_vol(uint32_t *adc_val, uint8_t is_record);
uint32_t pmic_sample_vop_vol(uint32_t *adc_val, uint8_t is_record);
uint32_t pmic_sample_pvdd_ict_vol(void);
uint32_t pmic_get_pvdd_vol(void);
uint32_t pmic_get_pvdd2_vol(void);
void pmic_adjust_pvdd_vol(int32_t vol);
void pmic_adjust_pvdd2_vol(int32_t vol);
void pmic_set_pvdd_pwm(int32_t duty);
void pmic_set_pvdd2_pwm(int32_t duty);
uint32_t pmic_get_pvdd_duty(void);
uint32_t pmic_get_pvdd2_duty(void);
void pmic_adjust_vop_vol(int32_t vol);
void pmic_set_vop_pwm(int32_t duty);
uint32_t pmic_get_vop_duty(void);

uint32_t pmic_pvdd_adjust_is_finish(void);
uint32_t pmic_pvdd2_adjust_is_finish(void);
uint32_t pmic_vop_adjust_is_finish(void);

char *pmic_wave_state_str(uint32_t state);

uint32_t pmic_pvdd_wave_state(void);
uint32_t pmic_vop_wave_state(void);
uint32_t pmic_pvdd_avrg_vol(void);
uint32_t pmic_pvdd2_wave_state(void);
uint32_t pmic_pvdd2_avrg_vol(void);

#endif
