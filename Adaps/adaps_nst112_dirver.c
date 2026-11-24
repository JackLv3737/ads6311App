/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_nst112_driver.c
* Version      : 1.0.0
* Description  : This file is application for nst112 dirver
* Creation Date: 2023/05/30
* Author       : wang.yi
***********************************************************************************************************************/
#include <stdio.h>
#include "bsp_basic.h"
#include "bsp_i2c.h"
#include "adaps_nst112_driver.h"
#include "adaps_soc_protocol.h"
#include "common.h"

#define NST112_ADDR(m_model)    (0x90 + (m_model << 1))
#define NST112_NAME(m_model)    ('A' + m_model)

#define NST112_MODEL_NUM    4

static uint8_t s_chip_inited_ok_flag = 0;

static uint8_t s_chip_remap[NST112_MODEL_NUM] = {CHIPA, CHIPB, CHIPC, CHIPD};

static int nst112_config(uint8_t  chip_addr)
{
    uint8_t point_reg;
    uint8_t config[2];

    point_reg = 0x01;
    config[0] = 0x60;
    config[1] = 0xA0;

    if(i2c_write(chip_addr, point_reg, config, 2) != 0){
        return -1;
    }

    return 0;
    
}

void nst112_init(void)
{
    int i;
    int first_model = -1;
    //有些中微板子需要加2毫秒以上延时才使CHIPA初始化通过，原因未明;
    bsp_delay_ms(5);

    s_chip_inited_ok_flag = 0;
    for(i = 0; i < NST112_MODEL_NUM; i++){
        s_chip_remap[i] = i;
        if(nst112_config(NST112_ADDR(i)) == 0){
            if(first_model < 0){
                first_model = i;
            }
            s_chip_inited_ok_flag |= BIT(i);
            printf("NST112 chip%c init ok\n", NST112_NAME(i));
        }
    }

    if(!s_chip_inited_ok_flag){
        printf("nst112 config fail, no chip probed!\n");
        return;
    }

    //旧TX板贴有NST112A与NST112B，而新TX板只贴一片NST112，型号不固定
    //SpadisApp显示的OFFSET_TEMP_B的值(即CHIPB，指VCSEL附近温度)
    //如果新板贴的不是CHIPB，则需要remap
    if(!(s_chip_inited_ok_flag & BIT(CHIPB))){
        s_chip_remap[CHIPB] = first_model;
    }

    printf("nst112 init ok, ok flag 0x%x, CHIPB index %d\n", s_chip_inited_ok_flag, s_chip_remap[CHIPB]);

}

float get_temperature_nst112(NST112_CHIP chip)
{

    uint8_t point_reg;
    uint8_t temp_nst112[2];
    int16_t temp_int16;
    float t;
    uint8_t chip_remap;

	if(NST112_MODEL_NUM <= chip){
        return 0.0f;
    }

    chip_remap = s_chip_remap[chip];
    point_reg = 0;
    if(i2c_read(NST112_ADDR(chip_remap), point_reg, temp_nst112, 2) != 0){
        return 0.0f;
    }
    temp_int16 = ((temp_nst112[0] << 8) | (temp_nst112[1]));

    t = (temp_int16 >> 4) * 0.0625;
    if (get_dbgflag() & ADAPS_DBG_TRACE_TEMPERATURE_READ){
        printf("i2c address:0x%x, t[0]=0x%02x, t[1]=0x%02x,temp_int16=%d(0x%x), return: %e(%f)\r\n",
        NST112_ADDR(chip_remap), temp_nst112[0], temp_nst112[1], temp_int16, temp_int16, t, t);
    }
    return t;

}
