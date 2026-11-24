/**
 * @file common.h
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#define false                                   0

#if !defined(BIT)
#define BIT(n)                      (1 << n)
#endif

#ifndef ABS
#define ABS(m_a, m_b) ((m_a) > (m_b) ? ((m_a) - (m_b)) : ((m_b) - (m_a)))
#endif

#ifndef COMBINE_U16
#define COMBINE_U16(m_high_byte, m_low_byte)    (((m_high_byte) << 8) | (m_low_byte))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(m_array) (sizeof(m_array) / sizeof(m_array[0]))
#endif

#define ATTR_USED __attribute__((used))
#define ATTR_BUILD_AT(m_addr)   __attribute__((at(m_addr)))

#define _STR(m_s)   #m_s
#define STR(m_s)   _STR(m_s)

static uint8_t calc_str_xor(char *buf) {
    uint8_t xor = 0;
    while(*buf) xor ^= *buf++;
    return xor;
}

#endif /*__COMMON_H__*/
