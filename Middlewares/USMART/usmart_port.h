/***********************************************************************************************************************
* File Name    : usmart.h
* Version      : 1.0.0
* Description  : head file for usmart port 
* Creation Date: 2023/05/18
* Author       : wang.yi
***********************************************************************************************************************/
 
#ifndef __USMART_PORT_H
#define __USMART_PORT_H

#define MAX_FNAME_LEN           30      /* func name max length */
#define MAX_PARM                10      /* max params num, should be compatile with usmart_exe*/
#define PARM_LEN                200     /* all params total length should less than PARAM_LEN */


#define USMART_ENTIMX_SCAN      0       /*scan in timer irq, if config as 0, shold be called periodic.  \
                                          if enable stat function, USMART_ENTIMX_SCAN has set to be 1.  \
                                        */

#define USMART_USE_HELP         1       /* help info display */
#define USMART_USE_WRFUNS       1       /* w/r function */

#define USMART_PRINTF           printf 

# if USMART_ENTIMX_SCAN == 1    

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"

/* TIMX irq definition 
 * usmart.scan in timer to get uart data and call relating func.
 * blow 4 macros  mapping to TIM1~TIM16
 */
#define USMART_TIMX                     TIM4
#define USMART_TIMX_IRQn                TIM4_IRQn
#define USMART_TIMX_IRQHandler          TIM4_IRQHandler
#define USMART_TIMX_CLK_ENABLE()        do{ __HAL_RCC_TIM4_CLK_ENABLE(); }while(0)  /* TIMX enable */

#endif

/******************************************************************************************/

#ifndef uint32_t
typedef unsigned           char uint8_t;
typedef unsigned short     int  uint16_t;
typedef unsigned           int  uint32_t;
#endif



char * usmart_get_input_string(void);        
void usmart_timx_reset_time(void);             
uint32_t usmart_timx_get_time(void);            
void usmart_timx_init(uint16_t arr, uint16_t psc);   

#endif



























