/***********************************************************************************************************************
* File Name    : adaps_pid.h
* Version      : 1.0.0
* Description  : head file for pid control
* Creation Date: 2023/05/26
* Author       : wang.yi
***********************************************************************************************************************/
#ifndef ADAPS_PID_H
#define ADAPS_PID_H

#include <stdint.h>

//此状态用来表示pid目标改变后调整过程的状态，不作为电压的状态
typedef enum pid_adjust_state_e
{
    PID_STATE_UNKNOWN = 0,
    PID_STATE_TARGET_CHANGED,
    PID_STATE_ADJUSTING,
    PID_STATE_ADJUST_DONE,
    PID_STATE_ADJUST_TIMEOUT
}pid_ajust_state_t;


#define PID_P_VOP                               0.2f
#define PID_I_VOP                               0.04f
#define PID_D_VOP                               0.1f


#define PID_P_PVDD                              0.01f
#define PID_I_PVDD                              0.001f
#define PID_D_PVDD                              0.01f

#define CONTROL_MIN                             -1000
#define CONTROL_MAX                             1000

int32_t pid_control_vop(int32_t cur_pwm_val, uint32_t feedback);
void set_target_VOP(int32_t val);
int32_t pid_control_pvdd(uint32_t feedback);
void set_target_pvdd(uint32_t val);

void vop_pid_context_reset(void);
void pvdd_pid_context_reset(void);

void vop_pid_state_print(void);
void pvdd_pid_state_print(void);

pid_ajust_state_t vop_pid_adjust_state_get(void);
pid_ajust_state_t pvdd_pid_adjust_state_get(void);


#endif
