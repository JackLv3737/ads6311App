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
#include <stdint.h>

enum bsp_hw_variant_ver_e
{
    HW_VARIANT_VER_UNKNOWN = 0,
    HW_VARIANT_VER0, //demo4v0, 原始版
    HW_VARIANT_VER1, //demo4v1
    HW_VARIANT_VER2,
    HW_VARIANT_VER3,
    HW_VARIANT_COUNT
};

enum bsp_hw_feature_e
{
    HW_FEATURE_PVDD_CURRENT = (1 << 0),
    HW_FEATURE_PVDD_SWIO = (1 << 1)
};

#define delay_us bsp_delay_us
#define delay_ms bsp_delay_ms

int bsp_basic_init(const char *hardware_ver, const char *software_ver);
void bsp_delay_us(uint32_t us);
void bsp_delay_ms(uint32_t ms);
uint32_t bsp_tick_get(void);
int bsp_stdio_getchar_unblock(void);
int bsp_stdio_getchar_timeout(uint32_t timeout_ms);
void bsp_reboot(void);

void bsp_enable_irq(void);
void bsp_disable_irq(void);

uint32_t bsp_hw_variant_ver(void);
uint8_t *bsp_hw_variant_name(void);
uint32_t bsp_hw_check_feature(uint32_t feature);

#endif /*__BSP_BASIC_H__*/
