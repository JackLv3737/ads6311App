/**
 * @file bsp_pwm.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_PWM_H__
#include <stdint.h>

#define PWM_MAX_VALUE                   1000
#define PWM_MIN_VALUE                   0

int bsp_pwm_init(void);
int bsp_pwm_set_duty(uint32_t channel, uint32_t duty);
uint32_t bsp_pwm_get_duty(uint32_t channel);

#define pwm_set_duty bsp_pwm_set_duty



#endif /*__BSP_PWM_H__*/
