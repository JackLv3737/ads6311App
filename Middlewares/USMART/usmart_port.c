/***********************************************************************************************************************
* File Name    : usmart_port.c
* Version      : 1.0.0
* Description  : This file is for usmart port
* Creation Date: 2023/05/18
* Author       : wang.yi
***********************************************************************************************************************/

#include "usmart.h"
#include "usmart_port.h"

#define INPUT_BUF_LEN   (MAX_FNAME_LEN + PARM_LEN + 8)

#if USMART_ENTIMX_SCAN == 1
TIM_HandleTypeDef g_timx_usmart_handle;      /* timer handler*/
#endif

/**
 * @brief       get input data flow(str)
 * @note      
 * @param       
 * @retval
 *   @arg       0, have not received data
 *   @arg       others, data ptr
 */
char *usmart_get_input_string(void)
{
    static char input_buf[INPUT_BUF_LEN + 1];
    static uint16_t input_cnt = 0;
    int result;

    while(1){
        result = bsp_stdio_getchar_unblock();
        if(result < 0){
            break;
        }else if('\r' == result){
            continue;
        }else if('\n' == result){
            input_buf[input_cnt] = '\0';
            input_cnt = 0;
            return input_buf;
        }
        if(input_cnt >= INPUT_BUF_LEN){
            input_cnt = 0;
        }
        input_buf[input_cnt++] = (char)result;
    }


    return 0;
}

/* if sacn timer is enable */
#if USMART_ENTIMX_SCAN == 1

/**
  * usmart_reset_runtime,clear run time, along with timer count&flag. config LOAD to max value to get max stat time.
 *  usmart_get_runtime,get run time. because run time is calculate by CNT,and usmart is called in interrupt, so time interrupt is not available.and the max stat time is between 
 *  2 CNT, 1 reset and another overflow. to smt32, the max value is about 13.1s
 *  USMART_TIMX_IRQHandler&Timer4_Init config according to MCU spec.ensure that count freq is 10khz. and timer reload is disable.
 */

/**
 * @brief       reset runtime
 *  @note     
 * @param       none
 * @retval      none
 */
void usmart_timx_reset_time(void)
{
    __HAL_TIM_CLEAR_FLAG(&g_timx_usmart_handle, TIM_FLAG_UPDATE); /* clear int flag */
    __HAL_TIM_SET_AUTORELOAD(&g_timx_usmart_handle, 0XFFFF);      /* set reload value to max */
    __HAL_TIM_SET_COUNTER(&g_timx_usmart_handle, 0);              /* clear CNT */
    usmart_dev.runtime = 0;
}

/**
 * @brief      get runtime
 * @note      
 * @param      none
 * @retval     excute time,unit:0.1ms
 */
uint32_t usmart_timx_get_time(void)
{
    if (__HAL_TIM_GET_FLAG(&g_timx_usmart_handle, TIM_FLAG_UPDATE) == SET)  /*overflow during running time */
    {
        usmart_dev.runtime += 0XFFFF;
    }
    usmart_dev.runtime += __HAL_TIM_GET_COUNTER(&g_timx_usmart_handle);
    return usmart_dev.runtime;                                 /* return cnt */
}

/**
 * @brief       timer init
 * @param       arr:auto load value
 *              psc:timer division factor
 * @retval      none
 */ 
void usmart_timx_init(uint16_t arr, uint16_t psc)
{
    USMART_TIMX_CLK_ENABLE();
    
    g_timx_usmart_handle.Instance = USMART_TIMX;                 
    g_timx_usmart_handle.Init.Prescaler = psc;                 
    g_timx_usmart_handle.Init.CounterMode = TIM_COUNTERMODE_UP;  
    g_timx_usmart_handle.Init.Period = arr;                      
    g_timx_usmart_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&g_timx_usmart_handle);
    HAL_TIM_Base_Start_IT(&g_timx_usmart_handle);               
    HAL_NVIC_SetPriority(USMART_TIMX_IRQn,3,3);                 
    HAL_NVIC_EnableIRQ(USMART_TIMX_IRQn);                       
}

/**
 * @brief       USMART timer irq
 * @param       none
 * @retval      none
 */
void USMART_TIMX_IRQHandler(void)
{
    if(__HAL_TIM_GET_IT_SOURCE(&g_timx_usmart_handle,TIM_IT_UPDATE)==SET)/* overflow irq */
    {
        usmart_dev.scan();                                   /* usmart scan */
        __HAL_TIM_SET_COUNTER(&g_timx_usmart_handle, 0);;    /* clear timer CNT */
        __HAL_TIM_SET_AUTORELOAD(&g_timx_usmart_handle, 100);/* restore LOAD */
    }
    
    __HAL_TIM_CLEAR_IT(&g_timx_usmart_handle, TIM_IT_UPDATE);/* clear irq flag */
}

#endif
















