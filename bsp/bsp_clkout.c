/**
 * @file bsp_clkout.c
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "BAT32A239.h"
#include "userdefine.h"
#include "clk.h"
#include "pcbz.h"
#include "bsp_clkout.h"

#define BSP_CLKOUT_PORT_SETTING() do{ \
        /*BSP_CLKOUT_CHN0*/ \
        PORT->PIOR0 |=  (1 << 4);   /* allocate CLKBUZ1 to P55 */ \
        PORT->P5    &= ~(1 << 5);   /* P55 output low level */ \
        PORT->PM5   &= ~(1 << 5);   /* P55 is used as CLKBUZ1 output */ \
}while(0)


int bsp_clkout_init(void)
{
    BSP_CLKOUT_PORT_SETTING();
    return 0;
}

int bsp_clkout_enable(uint8_t channel, uint8_t enable)
{
    #define CHN0_CLK_CONFIG PCBZ_FMAIN_3
    switch(channel)
    {
        case BSP_CLKOUT_CHN0://CLKBUZ1_P55
            if(enable){
                PCBZ->CKS1  = 0x80U | CHN0_CLK_CONFIG;
            }else{
                PCBZ->CKS1  &= ~0x80U; 
            }
        break;
    }


    return 0;
}