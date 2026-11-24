/**
 * @file bsp_basic.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_BASIC_H__

#define __BSP_BASIC_H__
#include <stdint.h>

#include <stdio.h>
#include "userdefine.h"
#include "BAT32A239.h"
#include "sci.h"
#include "clk.h"
#include "protocol.h"

#define PROJECT_NAME                       "HAWK"
#define HARDWARE_NAME                      "DEVKIT_3_0"
#define MCU_BRAND_NAME                     "CMSEMICON"
#define MCU_MODEL_NAME                     "BAT32A239KK64FB"

#define USART_REC_LEN                      12 * 1024 /* 定义最大接收字节数 120 KB */
#define RXBUFFERSIZE                       1 /* 缓存大小 */

extern uint8_t u8Uart1RxBuf[USART_REC_LEN];
extern uint32_t u32UartRxCnt;
extern uint8_t u8RxBuf[RXBUFFERSIZE];

#define delay_us bsp_delay_us

int bsp_basic_init(const char *hardware_ver, const char *software_ver);
void bsp_delay_us(uint32_t us);
void bsp_delay_ms(uint32_t ms);
uint32_t bsp_tick_get(void);
int bsp_stdio_getchar_unblock(void);
int bsp_stdio_getchar_timeout(uint32_t timeout_ms);

int bsp_init(void);

void bsp_enable_irq(void);
void bsp_disable_irq(void);


#endif /*__BSP_BASIC_H__*/
