/**
 * @file bsp_dac.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-28
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdio.h>
#include "BAT32A239.h"
#include "board_config.h"
#include "bsp_dac.h"
#include "dac.h"

#define DAC_VALUE_SET 12
#define DAC_VALUE_MAX 255

#define DAC_VDD 3300


int bsp_dac_init(void)
{
    DAC_Init(DAC_CHANNEL_0);
    DAC_Start(DAC_CHANNEL_0);
    bsp_dac_set_value(DAC_DEFAULT_VOL);
    return 0;
}

int bsp_dac_set_value(uint16_t val)
{
    uint32_t temp = val;
    temp = (val * (DAC_VALUE_MAX + 1)) / DAC_VDD;

    if(temp >= DAC_VALUE_MAX)
        temp = DAC_VALUE_MAX;

    DAC_Set_Value(DAC_CHANNEL_0, temp);
    return 0;
}

void bsp_dac_test(void)
{
    static char test_index = 0;
    const uint16_t val_tab[4] = {1000, 1300, 1600, 2450};
    printf("\n%s ++++\n", __FUNCTION__);
    printf("dac value set to %u\n", val_tab[test_index]);
    bsp_dac_set_value(val_tab[test_index]);
    test_index = (++test_index) % (sizeof(val_tab) / sizeof(val_tab[0]));
    printf("\n%s ----\n", __FUNCTION__);
}
