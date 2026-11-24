/**
 * @file bsp_timer.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "stdlib.h"
#include "board_config.h"
#include "bsp_timer.h"
#include "BAT32A239.h"
#include "userdefine.h"
#include "tim4.h"
#include "bsp_basic.h"


#define BSP_TIMER_CB_NUM_MAX    9

static void bsp_timer_cb_do_nothing(void);


static bsp_timer_callback_t handle[BSP_TIMER_CB_NUM_MAX];

static volatile bsp_timer_callback_t s_tm40ch0timer_callback = NULL;
static volatile unsigned int s_tm40ch0_interrupt_count = 0;

static inline void tm4_chn0_timer_start(unsigned int timeout_us);

static void bsp_timer_cb_do_nothing(void)
{
}

void tm40_channel0_interrupt(void)
{
    bsp_timer_callback_t timer_callback;
    INTC_ClearPendingIRQ(TM00_IRQn);    /* clear INTTM00 interrupt flag */
    TM40_Channel_Stop(TM4_CHANNEL_0);
    s_tm40ch0_interrupt_count++;
    if(s_tm40ch0timer_callback){
        timer_callback = s_tm40ch0timer_callback; 
        s_tm40ch0timer_callback = bsp_timer_cb_do_nothing;
        timer_callback();
    }
}



//refer to TM40_IntervalTimer()
//timeout_us must less than 65535 as tm4 is 16bit timer
static inline void tm4_chn0_timer_start(unsigned int timeout_us)
{
    //CGC->PER0 |= CGC_PER0_TM40EN_Msk;    /* enables input clock supply */
    //TM40->TPS0 = _0000_TM4_CKM3_fCLK_8 | _0000_TM4_CKM2_fCLK_1 | _0060_TM4_CKM1_fCLK_6 | _0000_TM4_CKM0_fCLK_0;

    /* stop specify channel */
    TM40_Channel_Stop(TM4_CHANNEL_0);

    /* clear interrupt flag */
    INTC_ClearPendingIRQ(TM00_IRQn);
    NVIC_ClearPendingIRQ(TM00_IRQn);
    /* Channel 0 is used as interval timer */
    //TM40->TMR00 = _8000_TM4_CLOCK_SELECT_CKM1 | _0000_TM4_CLOCK_MODE_CKS | _0000_TM4_TRIGGER_SOFTWARE |
    //              _0000_TM4_MODE_INTERVAL_TIMER | _0000_TM4_START_INT_UNUSED;
    TM40->TDR00 = timeout_us - 1;
    TM40->TOE0 &= ~_0001_TM4_CH0_OUTPUT_ENABLE;
    /* enable interrupt */
    INTC_EnableIRQ(TM00_IRQn);
    /* Start specify channel */
    TM40_Channel_Start(TM4_CHANNEL_0);
    
}

static int tm4_chn0_timer_init(void)
{

    CGC->PER0 |= CGC_PER0_TM40EN_Msk;    /* enables input clock supply */
    TM40->TPS0 = _0000_TM4_CKM3_fCLK_8 | _0000_TM4_CKM2_fCLK_1 | _0060_TM4_CKM1_fCLK_6 | _0000_TM4_CKM0_fCLK_0;
    TM40->TMR00 = _8000_TM4_CLOCK_SELECT_CKM1 | _0000_TM4_CLOCK_MODE_CKS | _0000_TM4_TRIGGER_SOFTWARE |
                  _0000_TM4_MODE_INTERVAL_TIMER | _0000_TM4_START_INT_UNUSED;

    NVIC_SetPriority(TM00_IRQn, IRQ_PRIOR_PVDD_SHUTDOWN);
    //printf("NVIC_GetPriority(TM00_IRQn) %d\n", NVIC_GetPriority(TM00_IRQn));
    return 0;
}



int bsp_timer_init(void)
{
    tm4_chn0_timer_init();

    return 0;
}

int bsp_timer_register_callback(bsp_timer_callback_t cb)
{
    if (cb)
    {
        for(uint8_t i = 0; i < BSP_TIMER_CB_NUM_MAX; i++)
        {
            if (!handle[i])
            {
                handle[i] = cb;
                return 0;
            }
        }
    }
    return -1;
}

void bsp_timer_period_proc(void)
{
    static uint32_t last_tick = 0;
    if(bsp_tick_get() - last_tick < BSP_TIMER_PERIOD_MS){
        return;
    }
    last_tick = bsp_tick_get();
    for (uint8_t i = 0; i < BSP_TIMER_CB_NUM_MAX; i++)
    {
        if (handle[i])
        {
            handle[i]();
        }
    }
}

int bsp_timer_oneshot_start(unsigned int chn, unsigned int timeout_us, bsp_timer_callback_t callback)
{
    if(timeout_us == 0){
        return -1;
    }

    switch(chn){
        case BSP_TIMER_CHN0:
            s_tm40ch0timer_callback = callback;
            tm4_chn0_timer_start(timeout_us);
        break;
        case BSP_TIMER_CHN1:

        break;
        default:
            return -1;
        break;
    }
    
    return 0;
}

int bsp_timer_oneshot_stop(unsigned int chn)
{

    switch(chn){
        case BSP_TIMER_CHN0:
            s_tm40ch0timer_callback = bsp_timer_cb_do_nothing;
            TM40_Channel_Stop(TM4_CHANNEL_0);
            INTC_ClearPendingIRQ(TM00_IRQn);
            NVIC_ClearPendingIRQ(TM00_IRQn);
        break;
        case BSP_TIMER_CHN1:

        break;
        default:
            return -1;
        break;
    }

    return 0;
}


void bsp_timer_oneshot_test(unsigned int timeout_us)
{
    uint32_t start_time = 0;
    uint32_t count_last = 0;

    //TM40_IntervalTimer(TM4_CHANNEL_0, timeout_us);
    bsp_timer_oneshot_start(BSP_TIMER_CHN0, timeout_us, bsp_timer_cb_do_nothing);
    count_last = s_tm40ch0_interrupt_count;
    start_time = bsp_tick_get();
    while(s_tm40ch0_interrupt_count == count_last){
        if(bsp_tick_get() - start_time > 1000){
            break;
        }
    }

    printf("\ns_tm40ch0_interrupt_count = %u, cost %d ms\n", s_tm40ch0_interrupt_count, bsp_tick_get() - start_time);

}


