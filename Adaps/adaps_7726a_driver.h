/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_7726a_driver.h
* Version      : 1.0.0
* Description  : head file for adaps_7726a_driver.c
* Creation Date: 2023/04/23
* Author       : wang.yi
***********************************************************************************************************************/
#ifndef ADAPS_7726A_H
#define ADAPS_7726A_H

#include <stdint.h>

#define CHAIN_CHN_USE_COUNT    32//(28)

#define NCV7726A_CHAIN1_FLAG    1
#define NCV7726A_CHAIN2_FLAG    2

#define NCV7726A_CHAIN_ALL_DISABLE  0                       //0:all disable
#define NCV7726A_CHAIN_1EN_2DIS     NCV7726A_CHAIN1_FLAG    //1:chain1 enable, chain2 disable
#define NCV7726A_CHAIN_1DIS_2EN     NCV7726A_CHAIN2_FLAG    //2:chain1 disable, chain2 enable
#define NCV7726A_CHAIN_ALL_ENABLE   (NCV7726A_CHAIN1_FLAG | NCV7726A_CHAIN2_FLAG) //3:all enable

typedef void (*ncv7726a_action_t)(void);

void ncv7726a_init(void);
void ncv7726a_reset(void);

uint32_t ncv7726a_set_channelx_en(uint8_t channel_number);
uint32_t ncv7726a_set_channel(uint8_t channel_number, ncv7726a_action_t action_after_done);
int ncv7726a_all_channels_disable(void);

void ncv7726a_print_state(void);
int ncv7726a_state_check(void);
int ncv7726a_state_clean(void);

void ncv7726a_chain_enable(uint32_t flag);

#endif
