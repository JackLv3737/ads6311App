/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_7726a_driver.c
* Version      : 1.0.0
* Description  : This file is application for hb 7726a dirver
* Creation Date: 2023/04/23
* Author       : wang.yi
***********************************************************************************************************************/
#include <stdio.h>
#include "string.h"
#include "adaps_7726a_driver.h"
#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "bsp_basic.h"

#define HB_SEL_CHN1_TO_CHN6   0
#define HB_SEL_CHN7_TO_CHN12  1

#define NCV7726A_CHN_COUNT  12
#define NCV7726A_CHN_GROUP_SIZE   (NCV7726A_CHN_COUNT / 2)
#define CHAIN_COUNT         2
#define CHAIN_CHIP_COUNT    3
#define CHAIN_CHN_TOTAL    (NCV7726A_CHN_COUNT * CHAIN_CHIP_COUNT)
#define CHAIN_CHN_OFFSET       (CHAIN_CHN_USE_COUNT / 2)

#define CHAIN_CHN_DISABLE_ALL    (CHAIN_CHN_TOTAL + 1)


#define SPI_CHANNEL_CHAIN1               BSP_SPI_CHN0
#define SPI_CHANNEL_CHAIN2               BSP_SPI_CHN1

#define SPI_DATA_SIZE       (sizeof(uint16_t) * CHAIN_CHIP_COUNT)

typedef struct channel_config_ {
    uint16_t OVLO : 1;
    uint16_t HBCNF : 6;
    uint16_t HBEN : 6;
    uint16_t ULDSC : 1;
    uint16_t HBSEL : 1;
    uint16_t SRR : 1;
} channel_config_t;

typedef union 
{
    channel_config_t config;
    uint16_t value;
}channel_config_u;


typedef struct channel_state_ {
    uint16_t TW : 1;
    //当HBSTx = 0时， 由于POR、 Spi或故障， HBCRx被强制为0
    uint16_t HBCR : 6;//Half-Bridge xx Config status,0=LS On,HS Off, 1=LS Off, HS On
    uint16_t HBST : 6;//Half-Bridge xx Output status,0=Hi-Z, 1=Enable
    uint16_t ULD : 1;//Underload Dedect
    uint16_t PSF : 1;//VS1 and/or VS2 Undervoltage or Overvoltage
    uint16_t OCS : 1;//Latched Overcurrent Shutdown
} channel_state_t;

typedef union 
{
    channel_state_t state;
    uint16_t value;
}channel_state_u;

static channel_state_u s_hbsel0_state[CHAIN_COUNT][CHAIN_CHIP_COUNT];
static channel_state_u s_hbsel1_state[CHAIN_COUNT][CHAIN_CHIP_COUNT];
static uint8_t s_cur_setting_chn = 0;
static uint32_t s_chn_disable_err_cnt = 0;
static uint8_t s_chain_enable_flag = NCV7726A_CHAIN_ALL_ENABLE;
static uint8_t s_state_clean = 0;

static void u16array_reverse(uint16_t *data, uint16_t size)
{
    uint8_t *u8p;
    uint8_t u8tmp;
    if(data){
        u8p = (uint8_t *)data;
        while(size--){
            u8tmp = u8p[0];
            u8p[0] = u8p[1];
            u8p[1] = u8tmp;
            u8p += 2;
        }
    }
}

static void u16array_set_bigendian(uint16_t value, uint16_t *data, uint16_t size)
{
    uint16_t u16tmp;
    if(data){
        u16tmp = ((value << 8) & 0xFF00) | ((value >> 8) & 0xFF);
        while(size--){
            *data++ = u16tmp;
        }
    }
}

void ncv7726a_chain_enable(uint32_t flag)
{
    switch(flag)
    {
        case NCV7726A_CHAIN_ALL_DISABLE:
            gpio_write(HBD1_EN_PORT, HBD1_EN_PIN, GPIO_PIN_RESET);
            gpio_write(HBD2_EN_PORT, HBD2_EN_PIN, GPIO_PIN_RESET);
            s_chain_enable_flag = NCV7726A_CHAIN_ALL_DISABLE;
        break;
        case NCV7726A_CHAIN_1EN_2DIS:
            gpio_write(HBD1_EN_PORT, HBD1_EN_PIN, GPIO_PIN_SET);
            gpio_write(HBD2_EN_PORT, HBD2_EN_PIN, GPIO_PIN_RESET);
            s_chain_enable_flag = NCV7726A_CHAIN_1EN_2DIS;
        break;
        case NCV7726A_CHAIN_1DIS_2EN:
            gpio_write(HBD1_EN_PORT, HBD1_EN_PIN, GPIO_PIN_RESET);
            gpio_write(HBD2_EN_PORT, HBD2_EN_PIN, GPIO_PIN_SET);
            s_chain_enable_flag = NCV7726A_CHAIN_1DIS_2EN;
        break;
        default: //all enable
            gpio_write(HBD1_EN_PORT, HBD1_EN_PIN, GPIO_PIN_SET);
            gpio_write(HBD2_EN_PORT, HBD2_EN_PIN, GPIO_PIN_SET);
            s_chain_enable_flag = NCV7726A_CHAIN_ALL_ENABLE;
        break;
    }
}


void ncv7726a_reset(void)
{
    uint16_t value[CHAIN_CHIP_COUNT];
    uint16_t value_rd[CHAIN_CHIP_COUNT];

    gpio_write(HBD1_EN_PORT, HBD1_EN_PIN, GPIO_PIN_RESET);
    gpio_write(HBD2_EN_PORT, HBD2_EN_PIN, GPIO_PIN_RESET);
    delay_us(20);
    gpio_write(HBD1_EN_PORT, HBD1_EN_PIN, GPIO_PIN_SET);
    gpio_write(HBD2_EN_PORT, HBD2_EN_PIN, GPIO_PIN_SET);
    delay_us(20);
    memset(&s_hbsel0_state, 0, sizeof(s_hbsel0_state));
    memset(&s_hbsel1_state, 0, sizeof(s_hbsel1_state));
    s_cur_setting_chn = 0;
    memset((uint8_t *)&value, 0, sizeof(value));
    bsp_spi_write_and_read_data(SPI_CHANNEL_CHAIN1, (uint8_t *)&value, (uint8_t *)&value_rd, sizeof(value));
    bsp_spi_write_and_read_data(SPI_CHANNEL_CHAIN2, (uint8_t *)&value, (uint8_t *)&value_rd, sizeof(value));
}


void ncv7726a_init(void)
{
    ncv7726a_reset();

    return;
}


/**
 * @brief Set the channel
 * 
 * @param channel_number range[1,37]
 * @param action_after_done do action after setting done
 */
uint32_t ncv7726a_set_channel(uint8_t channel_number, ncv7726a_action_t action_after_done)
{
    channel_config_u chain1_config;
    channel_config_u chain2_config;
    uint16_t tx_data[CHAIN_COUNT][CHAIN_CHIP_COUNT];
    uint16_t rx_data[CHAIN_COUNT][CHAIN_CHIP_COUNT];
    uint16_t chip_index;

    uint8_t chain1_chn = 0;
    uint8_t chain2_chn = 0;

    //printf("ncv7726a_set_channelx_en %d\n", channel_number);

    if (channel_number < 1 || channel_number > CHAIN_CHN_DISABLE_ALL) {
        return 1;
    }
    s_cur_setting_chn = channel_number;
    if(channel_number <= CHAIN_CHN_USE_COUNT){
        chain1_chn = channel_number - 1;
        //chain2_chn = (chain1_chn + CHAIN_CHN_OFFSET) % CHAIN_CHN_USE_COUNT;
        // 10/19,应系统需求，改为两列相同通道同时发光，不错开
        chain2_chn = chain1_chn;
    }else{
        channel_number = CHAIN_CHN_DISABLE_ALL;
    }

    //1. 关闭上次通道
    //chain1
    chain1_config.value = 0;
    chain1_config.config.SRR = 0;
    chain1_config.config.ULDSC = 0;
    chain1_config.config.OVLO = 0;
    chain1_config.config.HBEN = 0x3F;//all channel enable
    if (chain1_chn <= 5 || (chain1_chn > 11 && chain1_chn <= 17) \
        || (chain1_chn > 23 && chain1_chn <= 29)) {
        chain1_config.config.HBSEL = HB_SEL_CHN1_TO_CHN6;
    } else {
        chain1_config.config.HBSEL = HB_SEL_CHN7_TO_CHN12;
    }
    if (chain1_config.config.HBSEL == HB_SEL_CHN7_TO_CHN12) {
        u16array_set_bigendian(chain1_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        delay_us(6);
        chain1_config.config.HBSEL = HB_SEL_CHN1_TO_CHN6;
        u16array_set_bigendian(chain1_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        chain1_config.config.HBSEL = HB_SEL_CHN7_TO_CHN12;
    } else {
        u16array_set_bigendian(chain1_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        delay_us(6);
        // {
        //     channel_state_u hbsel0_state[CHAIN_CHIP_COUNT];
        //     int i;
            
        //     spi_write_and_read_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], (uint8_t *)rx_data[0], SPI_DATA_SIZE);
        //     u16array_reverse(rx_data[0], CHAIN_CHIP_COUNT);
        //     memcpy(hbsel0_state, &rx_data[0][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        //     for(i = 0; i < CHAIN_CHIP_COUNT; i++)
        //     {
        //         if(hbsel0_state[0].state.HBCR){
        //             s_chn_disable_err_cnt++;
        //         }
        //     }

        // }

        chain1_config.config.HBSEL = HB_SEL_CHN7_TO_CHN12;
        u16array_set_bigendian(chain1_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        chain1_config.config.HBSEL = HB_SEL_CHN1_TO_CHN6;
    }
    //chain2
    chain2_config.value = 0;
    chain2_config.config.SRR = 0;
    chain2_config.config.ULDSC = 0;
    chain2_config.config.OVLO = 0;
    chain2_config.config.HBEN = 0x3F;//all channel enable
    if (chain2_chn <= 5 || (chain2_chn > 11 && chain2_chn <= 17) \
        || (chain2_chn > 23 && chain2_chn <= 29)) {
        chain2_config.config.HBSEL = HB_SEL_CHN1_TO_CHN6;
    } else {
        chain2_config.config.HBSEL = HB_SEL_CHN7_TO_CHN12;
    }
    if (chain2_config.config.HBSEL == HB_SEL_CHN7_TO_CHN12) {
        u16array_set_bigendian(chain2_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN2, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        delay_us(6);
        chain2_config.config.HBSEL = HB_SEL_CHN1_TO_CHN6;
        u16array_set_bigendian(chain2_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN2, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        chain2_config.config.HBSEL = HB_SEL_CHN7_TO_CHN12;
    } else {
        u16array_set_bigendian(chain2_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN2, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        delay_us(6);
        chain2_config.config.HBSEL = HB_SEL_CHN7_TO_CHN12;
        u16array_set_bigendian(chain2_config.value, tx_data[0], CHAIN_CHIP_COUNT);
        spi_write_data(SPI_CHANNEL_CHAIN2, (uint8_t *)tx_data[0], SPI_DATA_SIZE);
        chain2_config.config.HBSEL = HB_SEL_CHN1_TO_CHN6;
    }
    delay_us(30);

    //2. 选通当前通道
    u16array_set_bigendian(chain1_config.value, &tx_data[0][0], CHAIN_CHIP_COUNT);
    u16array_set_bigendian(chain2_config.value, &tx_data[1][0], CHAIN_CHIP_COUNT);

    if (channel_number != CHAIN_CHN_DISABLE_ALL) {//37
        //chain1 config
        chain1_config.config.HBCNF = 0x01 << (chain1_chn % NCV7726A_CHN_GROUP_SIZE);
        chip_index = (CHAIN_CHIP_COUNT - 1) - (chain1_chn / NCV7726A_CHN_COUNT);
        u16array_set_bigendian(chain1_config.value, &tx_data[0][chip_index], 1);
        //chain2 config
        chain2_config.config.HBCNF = 0x01 << (chain2_chn % NCV7726A_CHN_GROUP_SIZE);
        chip_index = (CHAIN_CHIP_COUNT - 1) - (chain2_chn / NCV7726A_CHN_COUNT);
        u16array_set_bigendian(chain2_config.value, &tx_data[1][chip_index], 1);
    }

    //config hbsel and the last hbsel state 
    spi_write_and_read_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], (uint8_t *)rx_data[0], SPI_DATA_SIZE);
    spi_write_and_read_data(SPI_CHANNEL_CHAIN2, (uint8_t *)tx_data[1], (uint8_t *)rx_data[1], SPI_DATA_SIZE);
    u16array_reverse(rx_data[0], CHAIN_CHIP_COUNT * CHAIN_COUNT);

    if(s_chain_enable_flag & NCV7726A_CHAIN1_FLAG){
        if(chain1_config.config.HBSEL == HB_SEL_CHN7_TO_CHN12){
            memcpy(&s_hbsel0_state[0][0].value, &rx_data[0][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memcpy(&s_hbsel1_state[0][0].value, &rx_data[0][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }else{
        if(chain1_config.config.HBSEL == HB_SEL_CHN7_TO_CHN12){
            memset(&s_hbsel0_state[0][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memset(&s_hbsel1_state[0][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }

    if(s_chain_enable_flag & NCV7726A_CHAIN2_FLAG){
        if(chain2_config.config.HBSEL == HB_SEL_CHN7_TO_CHN12){
            memcpy(&s_hbsel0_state[1][0].value, &rx_data[1][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memcpy(&s_hbsel1_state[1][0].value, &rx_data[1][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }else{
        if(chain2_config.config.HBSEL == HB_SEL_CHN7_TO_CHN12){
            memset(&s_hbsel0_state[1][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memset(&s_hbsel1_state[1][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }


    delay_us(20);

    if(action_after_done){
        action_after_done();
    }

    //for get another hbsel state
    spi_write_and_read_data(SPI_CHANNEL_CHAIN1, (uint8_t *)tx_data[0], (uint8_t *)rx_data[0], SPI_DATA_SIZE);
    spi_write_and_read_data(SPI_CHANNEL_CHAIN2, (uint8_t *)tx_data[1], (uint8_t *)rx_data[1], SPI_DATA_SIZE);
    u16array_reverse(rx_data[0], CHAIN_CHIP_COUNT * CHAIN_COUNT);

    if(s_chain_enable_flag & NCV7726A_CHAIN1_FLAG){
        if(chain1_config.config.HBSEL == HB_SEL_CHN1_TO_CHN6){
            memcpy(&s_hbsel0_state[0][0].value, &rx_data[0][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memcpy(&s_hbsel1_state[0][0].value, &rx_data[0][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }else{
        if(chain1_config.config.HBSEL == HB_SEL_CHN1_TO_CHN6){
            memset(&s_hbsel0_state[0][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memset(&s_hbsel1_state[0][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }

    if(s_chain_enable_flag & NCV7726A_CHAIN2_FLAG){
        if(chain2_config.config.HBSEL == HB_SEL_CHN1_TO_CHN6){
            memcpy(&s_hbsel0_state[1][0].value, &rx_data[1][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memcpy(&s_hbsel1_state[1][0].value, &rx_data[1][0], sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }else{
        if(chain2_config.config.HBSEL == HB_SEL_CHN1_TO_CHN6){
            memset(&s_hbsel0_state[1][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }else{
            memset(&s_hbsel1_state[1][0].value, 0, sizeof(channel_state_u) * CHAIN_CHIP_COUNT);
        }
    }

    s_state_clean = 0;

    return 0;
}

uint32_t ncv7726a_set_channelx_en(uint8_t channel_number)
{
    uint32_t ret;
    ret = ncv7726a_set_channel(channel_number, NULL);
    ncv7726a_print_state();
    return ret;
}


int ncv7726a_all_channels_disable(void) 
{
    return ncv7726a_set_channel(CHAIN_CHN_DISABLE_ALL, NULL);
}

void ncv7726a_print_state(void)
{
    int i;
    int j;
    channel_state_u state;
    printf("\ncurrent setting channel %d\n", s_cur_setting_chn);
    printf("chn disable err cnt %d\n", s_chn_disable_err_cnt);
    printf("chain enable flag %d\n", s_chain_enable_flag);
    printf("state is clean %d\n", s_state_clean);        
    
    for(i = 0; i < CHAIN_COUNT; i++){
        for(j = 0; j < CHAIN_CHIP_COUNT; j++){
            state.value = s_hbsel0_state[i][CHAIN_CHIP_COUNT - 1 - j].value;
            printf("[chain%d]chip%d hbsel0 state: OCS %d, PSF %d, ULD %d, HBST 0x%02x, HBCR 0x%02x, TW %d\n", i, j, \
                state.state.OCS, state.state.PSF, state.state.ULD, state.state.HBST, state.state.HBCR, state.state.TW);
            state.value = s_hbsel1_state[i][CHAIN_CHIP_COUNT - 1 - j].value;
            printf("[chain%d]chip%d hbsel1 state: OCS %d, PSF %d, ULD %d, HBST 0x%02x, HBCR 0x%02x, TW %d\n", i, j, \
                state.state.OCS, state.state.PSF, state.state.ULD, state.state.HBST, state.state.HBCR, state.state.TW);
        }
    }
}

int ncv7726a_state_check(void)
{
    int i;
    int j;
    channel_state_u state;
    if(s_state_clean){
        return 0;
    }
    for(i = 0; i < CHAIN_COUNT; i++){
        for(j = 0; j < CHAIN_CHIP_COUNT; j++){
            state.value = s_hbsel0_state[i][CHAIN_CHIP_COUNT - 1 - j].value;
            if(state.state.OCS || state.state.PSF){
                return 1;
            }
            state.value = s_hbsel1_state[i][CHAIN_CHIP_COUNT - 1 - j].value;
            if(state.state.OCS || state.state.PSF){
                return 1;
            }
        }
    }
    return 0;
}

int ncv7726a_state_clean(void)
{
    s_state_clean = 1;
    memset(s_hbsel0_state, 0, sizeof(s_hbsel0_state));
    memset(s_hbsel1_state, 0, sizeof(s_hbsel1_state));
    return 0;
}


