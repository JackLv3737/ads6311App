/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_pmic_driver.c
* Version      : 1.0.0
* Description  : This file is application for pmic dirver
* Creation Date: 2023/04/24
* Author       : wang.yi
***********************************************************************************************************************/

/*
    V1.0.0  initial version
*/
#include "board_config.h"
#include "system_config.h"
#include "adaps_pmic_driver.h"
#include "bsp_i2c.h"
#include "bsp_gpio.h"
#include "bsp_basic.h"
#include "bsp_adc.h"
#include "bsp_pwm.h"
#include "ring_buf.h"
#include "adaps_soc_protocol.h"


#define VOL_REC_NUM_MAX 8
#define VOL_REC_NUM_MIN 5

#define IGNORE_FAR_VOL_CNT  1


typedef struct vol_rec_unit_s
{
    uint32_t timestamp;
    uint32_t val;
}vol_rec_unit_t;


typedef struct power_wave_info_s
{
    wave_state_t state;
    ring_buf_t ring_ctrl;
    vol_rec_unit_t vol_rec_list[VOL_REC_NUM_MAX];
    uint32_t vol_abs_avrg;
    uint32_t adjust_start_time;
    uint32_t adjust_timeout; //unit:ms
    uint32_t adjust_flag;
}power_wave_info_t;

typedef struct power_err_info_s
{
    uint32_t err_cnt;
    uint32_t last_err_time;

}power_err_info_t;


static uint8_t s_pmic_state = ADAPS_PMIC_STATE_UNKNOWN;

static uint32_t s_pmic_powerup_done_time = 0;

static uint32_t s_pmic_dv11_vol = 0;
static uint32_t s_pmic_sv11_vol = 0;
static uint32_t s_pmic_pvdd_vol = 0;
static uint32_t s_pmic_pvdd2_vol = 0;
static uint32_t s_pmic_vop_vol = 0;

static power_err_info_t s_dv11_err = {0};
static power_err_info_t s_sv11_err = {0};

static power_wave_info_t s_pvdd_wave = {
    .state = WAVE_STATE_UNKNOWN,
    .ring_ctrl = {0, 0, VOL_REC_NUM_MAX, 0},
};
static power_wave_info_t s_pvdd2_wave = {
    .state = WAVE_STATE_UNKNOWN,
    .ring_ctrl = {0, 0, VOL_REC_NUM_MAX, 0},
};
static power_wave_info_t s_vop_wave= {
    .state = WAVE_STATE_UNKNOWN,
    .ring_ctrl = {0, 0, VOL_REC_NUM_MAX, 0},
};

char *pmic_wave_state_str(uint32_t state)
{
    switch(state & WAVE_STATE_RELATE_MASK)
    {
        case WAVE_STATE_UNKNOWN:
            return "unknown";
        break;
        case WAVE_STATE_STABLE:
            switch(state & (~WAVE_STATE_RELATE_MASK)){
                case WAVE_STATE_NEAR:
                    return "stable-near";
                break;
                case WAVE_STATE_BELOW:
                    return "stable-below";
                break;
                case WAVE_STATE_ABOVE:
                    return "stable-above";
                break;
                case WAVE_STATE_CROSS:
                    return "stable-cross";
                break;
            }
        break;
        case WAVE_STATE_UNSTABLE:
            switch(state & (~WAVE_STATE_RELATE_MASK)){
                case WAVE_STATE_NEAR:
                    return "unstable-near";
                break;
                case WAVE_STATE_BELOW:
                    return "unstable-below";
                break;
                case WAVE_STATE_ABOVE:
                    return "unstable-above";
                break;
                case WAVE_STATE_CROSS:
                    return "unstable-cross";
                break;
            }
        break;
        case WAVE_STATE_RISING:
            switch(state & (~WAVE_STATE_RELATE_MASK)){
                case WAVE_STATE_NEAR:
                    return "rising-near";
                break;
                case WAVE_STATE_BELOW:
                    return "rising-below";
                break;
                case WAVE_STATE_ABOVE:
                    return "rising-above";
                break;
                case WAVE_STATE_CROSS:
                    return "rising-cross";
                break;
            }
        break;
        case WAVE_STATE_FALLING:
            switch(state & (~WAVE_STATE_RELATE_MASK)){
                case WAVE_STATE_NEAR:
                    return "falling-near";
                break;
                case WAVE_STATE_BELOW:
                    return "falling-below";
                break;
                case WAVE_STATE_ABOVE:
                    return "falling-above";
                break;
                case WAVE_STATE_CROSS:
                    return "falling-cross";
                break;
            }
        break;
    }
    return "undefined";
}

static int pmic_wave_judge(power_wave_info_t *wave, uint32_t target_val, int32_t near_threshold, uint32_t time_before)
{
    int i;
    int32_t rec_cnt = 0;
    uint32_t vol_val_list[VOL_REC_NUM_MAX];
    int32_t delta_list[VOL_REC_NUM_MAX];
    int32_t delta;
    int32_t near_cnt;
    int32_t below_cnt;
    int32_t above_cnt;
    int32_t flat_cnt;
    int32_t inc_cnt;
    int32_t dec_cnt;
    ring_buf_t ring_ctrl = wave->ring_ctrl;
    uint32_t vol_abs_sum;
    int32_t i32tmp;
    wave_state_t state = WAVE_STATE_UNKNOWN;
    while(!ring_buf_is_empty(&ring_ctrl)){
        if((bsp_tick_get() - wave->vol_rec_list[ring_ctrl.out].timestamp) < time_before){
            vol_val_list[rec_cnt++] = wave->vol_rec_list[ring_ctrl.out].val;
        }
        ring_buf_remove(&ring_ctrl);
    }

    if(rec_cnt < VOL_REC_NUM_MIN){
        return WAVE_STATE_UNKNOWN;
    }

    near_cnt = 0;
    below_cnt = 0;
    above_cnt = 0;
    vol_abs_sum = 0;
    for(i = 0; i < rec_cnt; i++){
        vol_abs_sum += vol_val_list[i];
        delta_list[i] = vol_val_list[i] - target_val;
        if(delta_list[i] > near_threshold){
            above_cnt++;
        }else if(delta_list[i] < -near_threshold){
            below_cnt++;
        }else{
            near_cnt++;
        }
    }
    wave->vol_abs_avrg = vol_abs_sum / rec_cnt;

    //
    if((near_cnt + IGNORE_FAR_VOL_CNT) >= rec_cnt){
        state = WAVE_STATE_STABLE_NEAR;
    }else{
        if(below_cnt == rec_cnt){
            //全below
            state = WAVE_STATE_BELOW;
        }else if(above_cnt == rec_cnt){
            //全above
            state = WAVE_STATE_ABOVE;
        }else{
            //有below、above，即有相交的
            state = WAVE_STATE_CROSS;
        }

        flat_cnt = 0;
        inc_cnt = 0;
        dec_cnt = 0;
        for(i = 1; i < rec_cnt; i++){
            //delta的前后比较
            i32tmp = delta_list[i] - delta_list[i - 1];
            if(i32tmp > near_threshold){ 
                inc_cnt++;
            }else if(i32tmp < -near_threshold){
                dec_cnt++;
            }else{
                flat_cnt++;
            }
        }

        i32tmp = rec_cnt - 1; //delta count
        if(flat_cnt == i32tmp){
            //全平的
            if(state == WAVE_STATE_CROSS){
                state = WAVE_STATE_UNSTABLE_NEAR;
            }else{ //BELOW or ABOVE
                state |= WAVE_STATE_STABLE;
            }
        }else if((flat_cnt + inc_cnt) == i32tmp){
            //全递增或平的
            state |= WAVE_STATE_RISING;
        }else if((flat_cnt + dec_cnt) == i32tmp){
            //全递减或平的
            state |= WAVE_STATE_FALLING;
        }else{
            //有递增、递减
            if(state == WAVE_STATE_CROSS){
                state = WAVE_STATE_UNSTABLE_NEAR;
            }else{//BELOW or ABOVE
                state |= WAVE_STATE_UNSTABLE;
            }
        }

    }
    return state;
}

void pmic_write_byte(uint8_t reg, uint8_t value)
{
    i2c_write(PMIC_CHIP_ADDR, reg, &value, 1);
}

void pmic_read_byte(uint8_t reg, uint8_t* value)
{
    i2c_read(PMIC_CHIP_ADDR, reg, value, 1);
}

void pmic_powerup_init()
{
    reg_control_set(OFFSET_DV11, POWER_DISABLE);
    reg_control_set(OFFSET_SV11, POWER_DISABLE);
    reg_control_set(OFFSET_AV11, POWER_DISABLE);
    reg_control_set(OFFSET_MP_VCC11A, POWER_DISABLE);
    delay_us(200);

    reg_control_set(OFFSET_IOV18, POWER_DISABLE);
    reg_control_set(OFFSET_MP_VCC18A, POWER_DISABLE);
    delay_us(200);

    reg_control_set(OFFSET_IOV33_DRV, POWER_DISABLE);
    reg_control_set(OFFSET_IOV33_TRIG, POWER_DISABLE);
    delay_us(200);

    reg_control_set(OFFSET_AV33, POWER_DISABLE);
    reg_control_set(OFFSET_SV33, POWER_DISABLE);
    delay_us(200);

    reg_control_set(OFFSET_VCC_3V3, POWER_DISABLE);
    delay_us(200);

    reg_control_set(OFFSET_VOP, POWER_DISABLE);
    delay_us(500);

    reg_control_set(OFFSET_PVDD, POWER_DISABLE);
    delay_us(500);

    s_pmic_state = ADAPS_PMIC_POWER_DOWN_DONE;
    reg_control_set(OFFSET_PMIC_STATE, s_pmic_state);
    
}

void pmic_powerup_all(void (*power_ctrl_cb)(void))
{
    s_pmic_state = ADAPS_PMIC_POWER_UP_ING;
    reg_control_set(OFFSET_PMIC_STATE, s_pmic_state);

    bsp_pwm_set_duty(PWM_PVDD_CHANNEL, PWM_PVDD_ADJ_INIT_DUTY);
#ifdef HW_HAVE_PVDDL
    bsp_pwm_set_duty(PWM_PVDDL_CHANNEL, PWM_PVDDL_ADJ_INIT_DUTY);
#endif
    //上电时把VOP配成绝对值最低电压
    bsp_pwm_set_duty(PWM_VOP_CHANNEL, PWM_VOP_ADJ_INIT_DUTY);
    
    reg_control_set(OFFSET_DV11, POWER_ENABLE);
    reg_control_set(OFFSET_SV11, POWER_ENABLE);
    delay_us(400);

    reg_control_set(OFFSET_AV11, POWER_ENABLE);
    reg_control_set(OFFSET_MP_VCC11A, POWER_ENABLE);
    delay_us(900);

    reg_control_set(OFFSET_IOV18, POWER_ENABLE);
    reg_control_set(OFFSET_MP_VCC18A, POWER_ENABLE);
    delay_us(500);

    reg_control_set(OFFSET_AV33, POWER_ENABLE);
    reg_control_set(OFFSET_SV33, POWER_ENABLE);
    delay_us(500);

    reg_control_set(OFFSET_VCC_3V3, POWER_ENABLE);
    //VCC_3V3_EN high --> INA_RST low --> PVDD_EN high  -->3ms--> INA_RST high
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
        gpio_write(INA_RST_PORT, INA_RST_PIN, GPIO_PIN_RESET);
    }
    delay_us(500);

    reg_control_set(OFFSET_VCC_5V_DRV, POWER_ENABLE);
    delay_us(500);

    reg_control_set(OFFSET_VOP, POWER_ENABLE);
    delay_us(4000);

    reg_control_set(OFFSET_PVDD, POWER_ENABLE);
#ifdef HW_HAVE_PVDDL
    reg_control_set(OFFSET_PVDDL_EN, POWER_ENABLE);
#endif
    pmic_adjust_pvdd_vol(reg_val_get(OFFSET_VOL_PVDD));
    //reg_reset_flag(OFFSET_VOL_PVDD, REG_FLAG_ALL);
    delay_ms(INA_RESET_KEEP_TIME);
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
        gpio_write(INA_RST_PORT, INA_RST_PIN, GPIO_PIN_SET);
    }
    printf("[%d]vol pvdd set to %d mv\n", bsp_tick_get(), reg_val_get(OFFSET_VOL_PVDD) * 10);

    if(power_ctrl_cb){
        //更新采样电压值
        power_ctrl_cb();
        delay_ms(4);
        power_ctrl_cb();
        delay_ms(4);
        power_ctrl_cb();
    }else{
        delay_ms(8);
    }
    printf("[%d]pmic state %d, pvdd %d, vop %d, dv11 %d, sv11 %d\n", bsp_tick_get(), s_pmic_state, 
        reg_val_get(OFFSET_PVDD_VOL) * 10, reg_val_get(OFFSET_VOP_VOL_ABS) * 10, reg_val_get(OFFSET_VOL_DV11), reg_val_get(OFFSET_VOL_SV11));
    s_pmic_powerup_done_time = bsp_tick_get();
    s_pmic_state = ADAPS_PMIC_POWER_UP_DONE;
    reg_control_set(OFFSET_PMIC_STATE, s_pmic_state);
    
}

void pmic_powerdown_all(void (*power_ctrl_cb)(void))
{
    s_pmic_state = ADAPS_PMIC_POWER_DOWN_ING;
    reg_control_set(OFFSET_PMIC_STATE, s_pmic_state);

    bsp_pwm_set_duty(PWM_VOP_CHANNEL, PWM_VOP_ADJ_INIT_DUTY);
    bsp_pwm_set_duty(PWM_PVDD_CHANNEL, PWM_PVDD_ADJ_INIT_DUTY);
#ifdef HW_HAVE_PVDDL
    bsp_pwm_set_duty(PWM_PVDDL_CHANNEL, PWM_PVDDL_ADJ_INIT_DUTY);
#endif
    reg_reset_flag(OFFSET_VOL_PVDD, REG_FLAG_ALL);
    
    reg_control_set(OFFSET_VOP, POWER_DISABLE);  
    delay_ms(200);
#ifdef HW_HAVE_PVDDL
    reg_control_set(OFFSET_PVDDL_EN, POWER_DISABLE);
#endif
    reg_control_set(OFFSET_PVDD, POWER_DISABLE);
    delay_ms(400);


    reg_control_set(OFFSET_VCC_5V_DRV, POWER_DISABLE);
    delay_us(500);

    reg_control_set(OFFSET_VCC_3V3, POWER_DISABLE);
    delay_us(500);

    reg_control_set(OFFSET_AV33, POWER_DISABLE);
    reg_control_set(OFFSET_SV33, POWER_DISABLE);
    delay_us(800);


    reg_control_set(OFFSET_MP_VCC18A , POWER_DISABLE);
    reg_control_set(OFFSET_IOV18, POWER_DISABLE);
    delay_us(1000);

    reg_control_set(OFFSET_AV11, POWER_DISABLE);
    reg_control_set(OFFSET_MP_VCC11A, POWER_DISABLE);
    delay_us(500);

    reg_control_set(OFFSET_SV11, POWER_DISABLE);
    reg_control_set(OFFSET_DV11, POWER_DISABLE);
    delay_us(500);
    if(power_ctrl_cb){
        power_ctrl_cb();
    }
    printf("pmic state %d, dv11 %d, sv11 %d\n", s_pmic_state, reg_val_get(OFFSET_VOL_DV11), reg_val_get(OFFSET_VOL_SV11));
    s_pmic_state = ADAPS_PMIC_POWER_DOWN_DONE;
    reg_control_set(OFFSET_PMIC_STATE, s_pmic_state);
}


uint8_t pmic_get_state(void)
{
    return s_pmic_state;
}

char* pmic_get_state_string(uint8_t state)
{
    switch(s_pmic_state){
        case ADAPS_PMIC_POWER_UP_ING: return "up-ing";
        case ADAPS_PMIC_POWER_UP_DONE: return "up-done";
        case ADAPS_PMIC_POWER_DOWN_ING: return "down-ing";
        case ADAPS_PMIC_POWER_DOWN_DONE: return "down-done";
    }

    return "unkown";

}

uint32_t pmic_powerup_done_time(void)
{
    return s_pmic_powerup_done_time;
}
#ifndef HW_REMOVE_DV11_ADC
uint32_t pmic_sample_dv11_vol(uint32_t *err_flag_out)
{
    uint32_t vol;
    uint32_t tick_now;
    uint32_t err_flag = 0;
    vol = (adc_get_result(ADC_CHANNEL_DV11) * VREF / ADC_MAX);
    if(s_pmic_state == ADAPS_PMIC_POWER_UP_DONE){
        if((vol > DV11_VOL_LIMIT_HIGH) || (vol < DV11_VOL_LIMIT_LOW)){
            tick_now = bsp_tick_get();
            if(tick_now - s_dv11_err.last_err_time > 1000){
                s_dv11_err.err_cnt = 1;
            }else if(tick_now - s_dv11_err.last_err_time >= 100){
                s_dv11_err.err_cnt++;
            }
            s_dv11_err.last_err_time = tick_now;
            if(s_dv11_err.err_cnt > 2){
                err_flag = 1;
            }
        }
    }

    if(err_flag_out){
        *err_flag_out = err_flag;
    }

    s_pmic_dv11_vol = vol;
    return vol;
}

#endif

#ifndef HW_REMOVE_SV11_ADC

uint32_t pmic_sample_sv11_vol(uint32_t *err_flag_out)
{
    uint32_t vol;
    uint32_t tick_now;
    uint32_t err_flag = 0;
    vol = (adc_get_result(ADC_CHANNEL_SV11) * VREF / ADC_MAX);
    if(s_pmic_state == ADAPS_PMIC_POWER_UP_DONE){
        if((vol > SV11_VOL_LIMIT_HIGH) || (vol < SV11_VOL_LIMIT_LOW)){
            tick_now = bsp_tick_get();
            if(tick_now - s_sv11_err.last_err_time > 1000){
                s_sv11_err.err_cnt = 1;
            }else if(tick_now - s_sv11_err.last_err_time >= 100){
                s_sv11_err.err_cnt++;
            }
            s_sv11_err.last_err_time = tick_now;
            if(s_sv11_err.err_cnt > 2){
                err_flag = 1;
            }
        }
    }

    if(err_flag_out){
        *err_flag_out = err_flag;
    }

    s_pmic_sv11_vol = vol;
    return vol;
}

#endif


uint32_t pmic_sample_pvdd_vol(uint32_t *adc_val, uint8_t is_record)
{
    int cur_state;

    uint32_t adc_pvdd;
    uint32_t target_vol;
    adc_pvdd = adc_get_result_average(ADC_CHANNEL_PVDD, 50);
    s_pmic_pvdd_vol =  (uint32_t)(PVDD_VOL_CALC(adc_pvdd) * 1000);
    if(adc_val){
        *adc_val = adc_pvdd;
    }

    if(is_record){

        if(ring_buf_is_full(&s_pvdd_wave.ring_ctrl)){
            ring_buf_remove(&s_pvdd_wave.ring_ctrl);
        }
        s_pvdd_wave.vol_rec_list[s_pvdd_wave.ring_ctrl.in].val = s_pmic_pvdd_vol;
        s_pvdd_wave.vol_rec_list[s_pvdd_wave.ring_ctrl.in].timestamp = bsp_tick_get();
        ring_buf_add(&s_pvdd_wave.ring_ctrl);

        target_vol = reg_val_get(OFFSET_VOL_PVDD) * 10;
        cur_state = pmic_wave_judge(&s_pvdd_wave, target_vol, PVDD_ERR_WARN_LIMIT, 1500);
        if(cur_state != WAVE_STATE_UNKNOWN){
            if(s_pvdd_wave.state != cur_state){
                printf("[%u]pvdd wave state changed: %s(0x%04x) -> %s(0x%04x), cur pvdd %d mv\n", bsp_tick_get(), 
                    pmic_wave_state_str(s_pvdd_wave.state), s_pvdd_wave.state, pmic_wave_state_str(cur_state), cur_state, s_pmic_pvdd_vol);
                if(WAVE_STATE_STABLE_NEAR == cur_state){
                    printf("pvdd is stable at target vol %d mv\n", s_pmic_pvdd_vol);
                }
                s_pvdd_wave.state = cur_state;
            }
        }

    }

    return s_pmic_pvdd_vol;
}

// for PVDD L
#ifdef HW_HAVE_PVDDL
uint32_t pmic_sample_pvdd2_vol(uint32_t *adc_val, uint8_t is_record)
{
    int cur_state;

    uint32_t adc_pvdd;
    uint32_t target_vol;
    adc_pvdd = adc_get_result_average(ADC_CHANNEL_PVDDL, 50);
    s_pmic_pvdd2_vol = (uint32_t)(PVDDL_VOL_CALC(adc_pvdd) * 1000);
    if(adc_val){
        *adc_val = adc_pvdd;
    }

    if(is_record){

        if(ring_buf_is_full(&s_pvdd2_wave.ring_ctrl)){
            ring_buf_remove(&s_pvdd2_wave.ring_ctrl);
        }
        s_pvdd2_wave.vol_rec_list[s_pvdd2_wave.ring_ctrl.in].val = s_pmic_pvdd2_vol;
        s_pvdd2_wave.vol_rec_list[s_pvdd2_wave.ring_ctrl.in].timestamp = bsp_tick_get();
        ring_buf_add(&s_pvdd2_wave.ring_ctrl);

        target_vol = reg_val_get(OFFSET_PVDD_4_WEAK_LASER) * 10;
        cur_state = pmic_wave_judge(&s_pvdd2_wave, target_vol, PVDD_ERR_WARN_LIMIT, 1500);
        if(cur_state != WAVE_STATE_UNKNOWN){
            if(s_pvdd2_wave.state != cur_state){
                printf("[%u]pvdd2 wave state changed: %s(0x%04x) -> %s(0x%04x), cur pvdd2 %d mv\n", bsp_tick_get(), 
                    pmic_wave_state_str(s_pvdd2_wave.state), s_pvdd2_wave.state, pmic_wave_state_str(cur_state), cur_state, s_pmic_pvdd2_vol);
                if(WAVE_STATE_STABLE_NEAR == cur_state){
                    printf("pvdd2 is stable at target vol %d mv\n", s_pmic_pvdd2_vol);
                }
                s_pvdd2_wave.state = cur_state;
            }
        }

    }

    return s_pmic_pvdd2_vol;
}

void pmic_set_pvdd2_pwm(int32_t duty)
{
    uint32_t last_duty = bsp_pwm_get_duty(PWM_PVDDL_CHANNEL);
    uint32_t delta_abs;
    uint32_t timeout_ms;
    uint32_t time_now;
    if (duty > PWM_MAX_VALUE){
        duty = PWM_MAX_VALUE;
    }

    if (duty < PWM_MIN_VALUE){
        duty = PWM_MIN_VALUE;
    }

    if(last_duty != duty){
        delta_abs = ABS(last_duty, duty);
        if(delta_abs < 5){
            timeout_ms = 200;
        }else if(delta_abs < 100){
            timeout_ms = 1000;
        }else{
            timeout_ms = VOP_POWERUP_TIME_MAX;
        }

        time_now = bsp_tick_get();
        //之前的已超时, 或者新的超时时间点比之前的迟，才更新adjust相关时间
        if((time_now - s_pvdd2_wave.adjust_start_time > s_pvdd2_wave.adjust_timeout)
        || ((time_now + timeout_ms) - (s_pvdd2_wave.adjust_start_time + s_pvdd2_wave.adjust_timeout) > 0)){
            s_pvdd2_wave.adjust_timeout = timeout_ms;
            s_pvdd2_wave.adjust_start_time = time_now;
        }
        s_pvdd2_wave.adjust_flag++;
    }

    pwm_set_duty(PWM_PVDDL_CHANNEL, (uint32_t)duty);
}

void pmic_adjust_pvdd2_vol(int32_t vol)
{
    int32_t pwm_val;
    int16_t cali_vol;

    if(vol > PVDDL_VOL_MAX){
        vol = PVDDL_VOL_MAX;
    }

    if(vol != 0){
        cali_vol = reg_val_get(OFFSET_CALI_PVDDL_SAVED_VOL);
        if(cali_vol == vol){
            pwm_val = reg_val_get(OFFSET_CALI_PVDDL_SAVED_DUTY);
        }else{
            pwm_val = (int32_t)PVDDL_PWM_DUTY_CALC(vol);
        }
    }else{
        pwm_val = PWM_PVDDL_ADJ_INIT_DUTY;
    }

    pmic_set_pvdd2_pwm(pwm_val);
}

uint32_t pmic_get_pvdd2_duty(void)
{
    return bsp_pwm_get_duty(PWM_PVDDL_CHANNEL);
}

#endif

#ifdef HW_HAVE_PVDD_ICT
uint32_t pmic_sample_pvdd_ict_vol(void)
{
    return (adc_get_result(ADC_CHANNEL_PVDD_ICT) * VREF / ADC_MAX) / 0.0245;
}
#endif

void pmic_set_pvdd_pwm(int32_t duty)
{
    uint32_t last_duty = bsp_pwm_get_duty(PWM_PVDD_CHANNEL);
    uint32_t delta_abs;
    uint32_t timeout_ms;
    uint32_t time_now;
    if (duty > PWM_MAX_VALUE){
        duty = PWM_MAX_VALUE;
    }

    if (duty < PWM_MIN_VALUE){
        duty = PWM_MIN_VALUE;
    }

    if(last_duty != duty){
        delta_abs = ABS(last_duty, duty);
        if(delta_abs < 5){
            timeout_ms = 200;
        }else if(delta_abs < 100){
            timeout_ms = 1000;
        }else{
            timeout_ms = VOP_POWERUP_TIME_MAX;
        }

        time_now = bsp_tick_get();
        //之前的已超时, 或者新的超时时间点比之前的迟，才更新adjust相关时间
        if((time_now - s_pvdd_wave.adjust_start_time > s_pvdd_wave.adjust_timeout)
        || ((time_now + timeout_ms) - (s_pvdd_wave.adjust_start_time + s_pvdd_wave.adjust_timeout) > 0)){
            s_pvdd_wave.adjust_timeout = timeout_ms;
            s_pvdd_wave.adjust_start_time = time_now;
        }
        s_pvdd_wave.adjust_flag++;
    }

    pwm_set_duty(PWM_PVDD_CHANNEL, (uint32_t)duty);
}

uint32_t pmic_get_pvdd_duty(void)
{
    return bsp_pwm_get_duty(PWM_PVDD_CHANNEL);
}

void pmic_adjust_pvdd_vol(int32_t vol)
{
    int32_t pwm_val;
    int16_t cali_vol;

    if(vol != 0){
        cali_vol = reg_val_get(OFFSET_CALI_PVDD_SAVED_VOL);
        if(cali_vol == vol){
            pwm_val = reg_val_get(OFFSET_CALI_PVDD_SAVED_DUTY);
        }else{
            pwm_val = (int32_t)PVDD_PWM_DUTY_CALC(vol);
        }
    }else{
        pwm_val = PWM_PVDD_ADJ_INIT_DUTY;
    }

    pmic_set_pvdd_pwm(pwm_val);
}


uint32_t pmic_sample_vop_vol(uint32_t *adc_val, uint8_t is_record)
{
    int cur_state;

    uint32_t adc_vop;
    uint32_t target_vol;

    adc_vop = adc_get_result_average(ADC_CHANNEL_VOP, 5);
    if(VOP_ADC_VAL_MIN < adc_vop){
        s_pmic_vop_vol = (uint32_t)(VOP_VOL_CALC(adc_vop) * 1000);
    }else{
        //这种情况是VOP电压芯片的使能脚被失能
        s_pmic_vop_vol = 0;
    }
    if(adc_val){
        *adc_val = adc_vop;
    }

    if(is_record){

        if(ring_buf_is_full(&s_vop_wave.ring_ctrl)){
            ring_buf_remove(&s_vop_wave.ring_ctrl);
        }
        s_vop_wave.vol_rec_list[s_vop_wave.ring_ctrl.in].val = s_pmic_vop_vol;
        s_vop_wave.vol_rec_list[s_vop_wave.ring_ctrl.in].timestamp = bsp_tick_get();
        ring_buf_add(&s_vop_wave.ring_ctrl);

        target_vol = reg_val_get(OFFSET_VOL_VOP) * -10;
        cur_state = pmic_wave_judge(&s_vop_wave, target_vol, VOP_ERR_WARN_LIMIT, 1500);
        if(cur_state != WAVE_STATE_UNKNOWN){
            if(s_vop_wave.state != cur_state){
                printf("[%u]vop wave state changed: %s(0x%04x) -> %s(0x%04x), cur vop:%d mv\n", 
                    bsp_tick_get(), pmic_wave_state_str(s_vop_wave.state), s_vop_wave.state, 
                    pmic_wave_state_str(cur_state), cur_state, s_pmic_vop_vol);
                if(WAVE_STATE_STABLE_NEAR == cur_state){
                    printf("vop is stable at target vol %d mv\n", target_vol);
                }
                s_vop_wave.state = cur_state;
            }
        }
    }

    return s_pmic_vop_vol;
}

uint32_t pmic_get_pvdd_vol(void)
{
    return s_pmic_pvdd_vol;
}

uint32_t pmic_get_pvdd2_vol(void)
{
    return s_pmic_pvdd2_vol;
}


void pmic_set_vop_pwm(int32_t duty)
{
    uint32_t last_duty = bsp_pwm_get_duty(PWM_VOP_CHANNEL);
    uint32_t timeout_ms;
    uint32_t time_now;
    uint32_t delta_abs;
    if (duty > PWM_MAX_VALUE){
        duty = PWM_MAX_VALUE;
    }

    if (duty < PWM_MIN_VALUE){
        duty = PWM_MIN_VALUE;
    }

    pwm_set_duty(PWM_VOP_CHANNEL, (uint32_t)duty);
    if(last_duty != duty){
        delta_abs = ABS(last_duty, duty);
        if(delta_abs < 5){
            timeout_ms = 200;
        }else if(delta_abs < 100){
            timeout_ms = 1000;
        }else{
            timeout_ms = VOP_POWERUP_TIME_MAX;
        }

        time_now = bsp_tick_get();
        //之前的已超时, 或者新的超时时间点比之前的迟，才更新adjust相关时间
        if((time_now - s_vop_wave.adjust_start_time > s_vop_wave.adjust_timeout)
        || ((time_now + timeout_ms) - (s_vop_wave.adjust_start_time + s_vop_wave.adjust_timeout) > 0)){
            s_vop_wave.adjust_timeout = timeout_ms;
            s_vop_wave.adjust_start_time = time_now;
        }


        s_vop_wave.adjust_flag++;
    }

}

uint32_t pmic_get_vop_duty(void)
{
    return bsp_pwm_get_duty(PWM_VOP_CHANNEL);
}

void pmic_adjust_vop_vol(int32_t vol)
{
    int32_t vop_duty;
    if (vol > 0) {
        vol = vol * (-1);
    }

    if(vol != 0){
        if(bsp_hw_variant_ver() == HW_VARIANT_VER0){
            vop_duty = (int32_t)VOP_PWM_DUTY_CALC_V0(vol);
        }else{
            //默认情况也用V1公式
            vop_duty = (int32_t)VOP_PWM_DUTY_CALC_V1(vol);
        }
    }else{
        vop_duty = PWM_VOP_ADJ_INIT_DUTY;
    }


    pmic_set_vop_pwm(vop_duty);

}


uint32_t pmic_pvdd_adjust_is_finish(void)
{
    if(bsp_tick_get() - s_pmic_powerup_done_time < PVDD_POWERUP_TIME_MAX){
        return 0;
    }
    return (bsp_tick_get() - s_pvdd_wave.adjust_start_time > s_pvdd_wave.adjust_timeout);
}

uint32_t pmic_pvdd2_adjust_is_finish(void)
{
    if(bsp_tick_get() - s_pmic_powerup_done_time < PVDD_POWERUP_TIME_MAX){
        return 0;
    }
    return (bsp_tick_get() - s_pvdd2_wave.adjust_start_time > s_pvdd2_wave.adjust_timeout);
}

uint32_t pmic_vop_adjust_is_finish(void)
{
    if(bsp_tick_get() - s_pmic_powerup_done_time < VOP_POWERUP_TIME_MAX){
        return 0;
    }
    return (bsp_tick_get() - s_vop_wave.adjust_start_time > s_vop_wave.adjust_timeout);
}

uint32_t pmic_pvdd_wave_state(void)
{
    return s_pvdd_wave.state;
}

uint32_t pmic_pvdd2_wave_state(void)
{
    return s_pvdd2_wave.state;
}

uint32_t pmic_vop_wave_state(void)
{
    return s_vop_wave.state;
}

uint32_t pmic_pvdd_avrg_vol(void)
{
    return s_pvdd_wave.vol_abs_avrg;
}

uint32_t pmic_pvdd2_avrg_vol(void)
{
    return s_pvdd2_wave.vol_abs_avrg;
}
