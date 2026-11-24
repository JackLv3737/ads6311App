/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_pid.c
* Version      : 1.0.0
* Description  : This file is for pid control
* Creation Date: 2023/05/26
* Author       : wang.yi
***********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "board_config.h"
#include "system_config.h"
#include "bsp_pwm.h"
#include "bsp_basic.h"
#include "adaps_pid.h"
#include "adaps_soc_protocol.h"

typedef struct pid_context_s
{
    pid_ajust_state_t adjust_state;
    int32_t target_val;
    float p_out;
    float i_out;
    float d_out;
    int32_t last_delta;
    uint32_t feedback;
    int32_t result;
    uint32_t adjust_start_time;
    uint32_t stable_start_time;
}pid_context_t;


static const char *s_adjust_state_str[] = {
    "unknown",
    "changed",
    "adjusting",
    "done",
    "timeout"
};

static pid_context_t s_vop_pid_ctx = {
    .adjust_state = 0,
    .target_val = 0,
    .p_out = 0.0f,
    .i_out = 0.0f,
    .d_out = 0.0f,
    .last_delta = 0,
    .feedback = 0,
    .result = 0,
    .adjust_start_time = 0,
    .stable_start_time = 0
};

static pid_context_t s_pvdd_pid_ctx = {
    .adjust_state = 0,
    .target_val = 0,
    .p_out = 0.0f,
    .i_out = 0.0f,
    .d_out = 0.0f,
    .last_delta = 0,
    .feedback = 0,
    .result = 0,
    .adjust_start_time = 0,
    .stable_start_time = 0
};

int32_t pid_control_vop(int32_t cur_pwm_val, uint32_t feedback)
{
    int32_t ret_val;
    int32_t delta;
    int32_t target_pwm_val;

    if(s_vop_pid_ctx.target_val == 0){
        return 0;
    }

    delta = feedback - s_vop_pid_ctx.target_val;
    s_vop_pid_ctx.p_out = PID_P_VOP * delta;
    s_vop_pid_ctx.i_out += PID_I_VOP * delta;
    s_vop_pid_ctx.d_out = PID_D_VOP * (delta - s_vop_pid_ctx.last_delta);
    s_vop_pid_ctx.last_delta = delta;
    s_vop_pid_ctx.feedback = feedback;

    switch(s_vop_pid_ctx.adjust_state)
    {
        case PID_STATE_TARGET_CHANGED:
            s_vop_pid_ctx.adjust_start_time = bsp_tick_get();
            s_vop_pid_ctx.adjust_state = PID_STATE_ADJUSTING;
            s_vop_pid_ctx.stable_start_time = 0;
        break;
        case PID_STATE_ADJUSTING:
            if(ABS(delta, 0) < VOP_ERR_WARN_LIMIT){
                if(s_vop_pid_ctx.stable_start_time == 0){
                    s_vop_pid_ctx.stable_start_time = bsp_tick_get();
                }else if((bsp_tick_get() - s_vop_pid_ctx.stable_start_time) > 500){
                    s_vop_pid_ctx.adjust_state = PID_STATE_ADJUST_DONE;
                }
            }else{
                s_vop_pid_ctx.stable_start_time = 0;
                if((bsp_tick_get() - s_vop_pid_ctx.adjust_start_time) > 1000){
                    s_vop_pid_ctx.adjust_state = PID_STATE_ADJUST_TIMEOUT;
                    printf("vop adjust timeout\n");
                }
            }

        break;
        //case PID_STATE_ADJUST_DONE:
        //case PID_STATE_ADJUST_TIMEOUT:
        //case PID_STATE_UNKNOWN:
        default:
        break;
    }

    ret_val = (int32_t)(s_vop_pid_ctx.p_out + s_vop_pid_ctx.i_out + s_vop_pid_ctx.d_out);

    //if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_CTRL)
    //{
    //    printf("s_vop_pid_ctx.target_val:= %d\r\n", s_vop_pid_ctx.target_val);
    //    printf("VOP delta = %d, pout = %f, iout = %f, dout = %f, ret_val = %d, target = %d\r\n", delta, p_out, i_out, d_out, ret_val, s_vop_pid_ctx.target_val);
    //}

    target_pwm_val = cur_pwm_val + ret_val;

    if ((PWM_MIN_VALUE > target_pwm_val) || (PWM_MAX_VALUE < target_pwm_val)){
        s_vop_pid_ctx.i_out = 0;
        s_vop_pid_ctx.last_delta = 0;
    }

    if (ret_val > CONTROL_MAX)
    {
        s_vop_pid_ctx.i_out = 0;
        s_vop_pid_ctx.last_delta = 0;
        ret_val = CONTROL_MAX;
    }

    if (ret_val < CONTROL_MIN){
        s_vop_pid_ctx.i_out = 0;
        s_vop_pid_ctx.last_delta = 0;
        ret_val = CONTROL_MIN;
    }

    s_vop_pid_ctx.result = ret_val;

    return ret_val;
}

void set_target_VOP(int32_t val)
{
    uint32_t target_val;
    if (val > 0){
        val = val * (-1);
    }
    target_val = (uint32_t)VOP_ADC_VAL_CALC(val);
    if(s_vop_pid_ctx.target_val != target_val){
        s_vop_pid_ctx.target_val = target_val;
        s_vop_pid_ctx.adjust_state = PID_STATE_TARGET_CHANGED;
    }else if(s_vop_pid_ctx.adjust_state != PID_STATE_ADJUST_DONE){
        s_vop_pid_ctx.adjust_state = PID_STATE_TARGET_CHANGED;
    }

}

int32_t pid_control_pvdd(uint32_t feedback)
{
    int32_t ret_val;
    int32_t delta;

    if(s_pvdd_pid_ctx.target_val == 0){
        return 0;
    }
    
    delta = feedback - s_pvdd_pid_ctx.target_val;
    s_pvdd_pid_ctx.p_out = PID_P_PVDD * delta;
    s_pvdd_pid_ctx.i_out += PID_I_PVDD * delta;
    s_pvdd_pid_ctx.d_out = PID_D_PVDD * (delta - s_pvdd_pid_ctx.last_delta);
    s_pvdd_pid_ctx.last_delta = delta;
    s_pvdd_pid_ctx.feedback = feedback;

    switch(s_pvdd_pid_ctx.adjust_state)
    {
        case PID_STATE_TARGET_CHANGED:
            s_pvdd_pid_ctx.adjust_start_time = bsp_tick_get();
            s_pvdd_pid_ctx.adjust_state = PID_STATE_ADJUSTING;
            s_pvdd_pid_ctx.stable_start_time = 0;
        break;
        case PID_STATE_ADJUSTING:
            if(ABS(delta, 0) < PVDD_ERR_WARN_LIMIT){
                if(s_pvdd_pid_ctx.stable_start_time == 0){
                    s_pvdd_pid_ctx.stable_start_time = bsp_tick_get();
                }else if((bsp_tick_get() - s_pvdd_pid_ctx.stable_start_time) > 500){
                    s_pvdd_pid_ctx.adjust_state = PID_STATE_ADJUST_DONE;
                }
            }else{
                s_pvdd_pid_ctx.stable_start_time = 0;
                if((bsp_tick_get() - s_pvdd_pid_ctx.adjust_start_time) > 1000){
                    s_pvdd_pid_ctx.adjust_state = PID_STATE_ADJUST_TIMEOUT;
                    printf("pvdd adjust timeout\n");
                }
            }

        break;
        //case PID_STATE_ADJUST_DONE:
        //case PID_STATE_ADJUST_TIMEOUT:
        //case PID_STATE_UNKNOWN:
        default:
        break;
    }
	
    ret_val = (int32_t)(s_pvdd_pid_ctx.p_out + s_pvdd_pid_ctx.i_out + s_pvdd_pid_ctx.d_out);
	
    //printf("PVDD PID: feedback %d, delta = %d, pout = %f, iout = %f, ret_val = %d, target = %d\n", 
    //    feedback, s_pvdd_pid_ctx.last_delta, s_pvdd_pid_ctx.p_out, s_pvdd_pid_ctx.i_out, ret_val, s_pvdd_pid_ctx.target_val);
        
    if (ret_val > CONTROL_MAX){
       ret_val = CONTROL_MAX; 
    }

    if (ret_val < -1000){
        ret_val = -1000;
    }

    s_pvdd_pid_ctx.result = ret_val;

    return ret_val;
}

void set_target_pvdd(uint32_t val)
{
    uint32_t target_val;
    target_val = (uint32_t)PVDD_ADC_VAL_CALC(val);
    // printf("set_target_pvdd: pvdd_code:= %d", val);
    if(s_pvdd_pid_ctx.target_val != target_val){
        s_pvdd_pid_ctx.target_val = target_val;
        s_pvdd_pid_ctx.adjust_state = PID_STATE_TARGET_CHANGED;
    }else if(s_pvdd_pid_ctx.adjust_state != PID_STATE_ADJUST_DONE){
        s_pvdd_pid_ctx.adjust_state = PID_STATE_TARGET_CHANGED;
    }
}

void vop_pid_context_reset(void)
{
    uint32_t target_val = s_vop_pid_ctx.target_val;
    memset(&s_vop_pid_ctx, 0, sizeof(s_vop_pid_ctx));
    s_vop_pid_ctx.target_val = target_val;
}

void pvdd_pid_context_reset(void)
{
    uint32_t target_val = s_pvdd_pid_ctx.target_val;
    memset(&s_pvdd_pid_ctx, 0, sizeof(s_pvdd_pid_ctx));
    s_pvdd_pid_ctx.target_val = target_val;
}

void vop_pid_state_print(void)
{
    printf("vop pid state:\n");
    printf("adjust state:%s\n", s_adjust_state_str[s_vop_pid_ctx.adjust_state]);
    printf("target adc value: %d\n", s_vop_pid_ctx.target_val);
    printf("feedback adc value: %d\n", s_vop_pid_ctx.feedback);
    printf("last delta: %d\n", s_vop_pid_ctx.last_delta);
    printf("p.i.d out: %f %f %f \n", s_vop_pid_ctx.p_out, s_vop_pid_ctx.i_out, s_vop_pid_ctx.d_out);
    printf("result:%d\n", s_vop_pid_ctx.result);
    
}

void pvdd_pid_state_print(void)
{
    printf("pvdd pid state:\n");
    printf("adjust state:%s\n", s_adjust_state_str[s_pvdd_pid_ctx.adjust_state]);
    printf("target adc value: %d\n", s_pvdd_pid_ctx.target_val);
    printf("feedback adc value: %d\n", s_pvdd_pid_ctx.feedback);
    printf("last delta: %d\n", s_pvdd_pid_ctx.last_delta);
    printf("p.i.d out: %f %f %f \n", s_pvdd_pid_ctx.p_out, s_pvdd_pid_ctx.i_out, s_pvdd_pid_ctx.d_out);
    printf("result:%d\n", s_pvdd_pid_ctx.result);
    
}

pid_ajust_state_t vop_pid_adjust_state_get(void)
{
    return s_vop_pid_ctx.adjust_state;
}

pid_ajust_state_t pvdd_pid_adjust_state_get(void)
{
    return s_vop_pid_ctx.adjust_state;
}


