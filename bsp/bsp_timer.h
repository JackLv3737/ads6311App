/**
 * @file bsp_timer.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_TIMER_H__
#include <stdint.h>

#define BSP_TIMER_PERIOD_MS 100 //100ms

typedef enum bsp_timer_chn_e{
    BSP_TIMER_CHN0 = 0,
    BSP_TIMER_CHN1,
    BSP_TIMER_CHN_SUM
}bsp_timer_chn_t;

typedef void (*bsp_timer_callback_t)(void);

int bsp_timer_init(void);
int bsp_timer_register_callback(bsp_timer_callback_t cb);
void bsp_timer_period_proc(void);

int bsp_timer_oneshot_start(unsigned int chn, unsigned int timeout_us, bsp_timer_callback_t callback);
int bsp_timer_oneshot_stop(unsigned int chn);

//-------will remove bellow
#define timer_cb_regist(m_cb) bsp_timer_register_callback(m_cb) 
#define timer_periodcallback bsp_timer_period_proc

#endif /*__BSP_TIMER_H__*/
