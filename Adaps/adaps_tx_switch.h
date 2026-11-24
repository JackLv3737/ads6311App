/**
 * @file adaps_tx_switch.h
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2023-09-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef ADAPS_TX_SWITCH_H
#define ADAPS_TX_SWITCH_H

#include <stdint.h>

#define SEL_EXPO_TIME1  0
#define SEL_EXPO_TIME2  1

typedef void (*txswt_action_t)(void);

enum txswt_callback_type_e
{
    TXSWT_FIRST_SUBFRAME_START = 0,
    TXSWT_LAST_SUBFRAME_END,
};

int txswt_init(void);
void txswt_open(void);
void txswt_close(void);
int txswt_is_open(void);
void txswt_pause(void);
void txswt_resume(void);
int txswt_enable(void);
int txswt_disable(void);
int txswt_is_enable(void);
int txswt_state_check(void);
int txswt_state_clean(void);

//for roi cali
void txswt_set_fixed_chn(uint32_t fixed_chn);
void txswt_set_fixed_chn2(uint32_t fixed_chn);
void txswt_set_fixed_chn3(uint32_t fixed_chn);
void txswt_set_fixed_chn4(uint32_t fixed_chn);
void txswt_config_fixed_chn(uint32_t index, uint32_t fixed_chn);
void txswt_set_fixed_zone(uint32_t fixed_zone);
void txswt_set_hit_chn(uint32_t hit_chn);
void txswt_set_hit_zone(uint32_t hit_zone);

void txswt_set_hit_two_chn(uint32_t chn1, uint32_t chn2);
void txswt_set_hit_multi_chn(uint32_t chn_mask);

void txswt_notify_pvdd_changed(uint32_t pvdd_vol);

int txswt_set_rx_expo_time1(uint32_t time_us);
int txswt_set_rx_expo_time2(uint32_t time_us);
//flag: 0=using time1, others=using time2
int txswt_switch_rx_expo_time(uint32_t flag);
void txswt_timer_enable(uint32_t enable);

void txswt_chn_short_protect_enable(uint32_t enable);

void txswt_chn_reconfig_enable(uint32_t enable);

void txswt_chip_config(void);

void txswt_print_state(void);

uint32_t txswt_ctrl_last_time_get(void);

void txswt_set_subframe_count(uint32_t count);
uint32_t txswt_get_cur_subframe_index(void);
void txswt_reset_cur_subframe_index(void);

void txswt_set_vroll_offset(uint32_t offset);

void txswt_set_callback(uint8_t type, txswt_action_t cb);

#endif //ADAPS_TX_SWITCH_H



