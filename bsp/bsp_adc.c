/**
 * @file bsp_adc.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
 
#include "bsp_adc.h"
#include "bsp_basic.h"
#include "BAT32A239.h"
#include "userdefine.h"
#include "adc.h"
#include "board_config.h"
#include "common.h"


#define VOP_TEST_PORT   14
#define VOP_TEST_PIN    6

#define PVDD_TEST_PORT   14
#define PVDD_TEST_PIN    7

#define INA_VOUT_PORT   0
#define INA_VOUT_PIN    4

#ifdef BOARD_DEMO4
#define ADC_NTC_PORT   2
#define ADC_NTC_PIN    2

#ifdef HW_HAVE_PVDDL
#define PVDDL_PORT   2
#define PVDDL_PIN    7
#endif

#ifdef HW_HAVE_PVDD_ICT
#define PVDD_ICT_PORT   2
#define PVDD_ICT_PIN    4
#endif

#else
#define DV11_FB_PORT   2
#define DV11_FB_PIN    7

#define SV11_FB_PORT   2
#define SV11_FB_PIN    7

#define ADC_NTC_PORT   2
#define ADC_NTC_PIN    4

#define ADC_VREF_PORT   2
#define ADC_VREF_PIN    3
#endif

void bsp_adc_test(uint32_t times);

static void bsp_adc_port_init(uint32_t init_chn_mask)
{
    //管脚配为模拟输入，不用配pinmux
    //VOP_TEST, P146 ANI15
    if(BIT(ADC_CHANNEL_VOP) & init_chn_mask){
        PORT->PMC14 |= (1 << VOP_TEST_PIN);
    }


    //PVDD_TEST, P147 ANI12
    if(BIT(ADC_CHANNEL_PVDD) & init_chn_mask){
        PORT->PMC14 |= (1 << PVDD_TEST_PIN);
    }

    //demo4 1V1版本去掉了PVDD电流检测功能
    if((BIT(ADC_CHANNEL_CURRENT_PVDD) & init_chn_mask)
    &&bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)
    ){
        //INA_VOUT, P04 ANI13
        PORT->PMC0 |= (1 << INA_VOUT_PIN);
    }

    //demo3:ADC_NTC for rx, P24 ANI4
    //demo4:ADC_NTC for tx, P22 ANI2
    if(BIT(ADC_CHANNEL_RX_TEMPERATURE) & init_chn_mask){
        PORT->PMC2 |= (1 << ADC_NTC_PIN);
    }

#ifndef HW_REMOVE_DV11_ADC
    //DV11_FB, P27 ANI7
    if(BIT(ADC_CHANNEL_DV11) & init_chn_mask){
        PORT->PMC2 |= (1 << DV11_FB_PIN);
    }
#endif

#ifndef HW_REMOVE_SV11_ADC
    //SV11_FB, P26 ANI6
    if(BIT(ADC_CHANNEL_SV11) & init_chn_mask){
        PORT->PMC2 |= (1 << SV11_FB_PIN);
    }
#endif

#ifndef HW_REMOVE_VREF_ADC
    //ADC_VREF, P23 ANI3
    if(BIT(ADC_CHANNEL_ADC_VREF) & init_chn_mask){
        PORT->PMC2 |= (1 << ADC_VREF_PIN);
    }
#endif

#ifdef HW_HAVE_PVDDL
    if(BIT(ADC_CHANNEL_PVDDL) & init_chn_mask){
        PORT->PMC2 |= (1 << PVDDL_PIN);
    }
#endif

#ifdef HW_HAVE_PVDD_ICT
    if(BIT(ADC_CHANNEL_PVDD_ICT) & init_chn_mask){
        PORT->PMC2 |= (1 << PVDD_ICT_PIN);
    }
#endif

}

int bsp_adc_init(uint32_t init_chn_mask)
{
    INTC_DisableIRQ(ADC_IRQn);          /* disable INTAD interrupt */
    CGC->PER0 |= CGC_PER0_ADCEN_Msk;    /* enables input clock supply */
    ADC->ADM0  = 0x00U;                 /* disable AD conversion and clear ADM0 register */
    INTC_ClearPendingIRQ(ADC_IRQn);     /* clear INTAD interrupt flag */
    NVIC_ClearPendingIRQ(ADC_IRQn);     /* clear INTAD interrupt flag */
    
    bsp_adc_port_init(init_chn_mask);
    
    ADC->ADM0 = _00_AD_CONVERSION_CLOCK_32 | _00_AD_COMPARATOR_DISABLE;

    ADC->ADM1 = _00_AD_HISPEED;/* AD conversion mode setting */
    ADC->ADM1 |= _00_AD_OPERMODE_SELECT;/* AD operation mode: select or scan mode  */
    ADC->ADM1 |= _08_AD_CONVMODE_ONESHOT;

    //ADC->ADM2 = _00_AD_POSITIVE_VDD | _00_AD_NEGATIVE_VSS | _00_AD_AREA_MODE_1 ;/* AD reference voltage setting */
    ADC->ADM2 = _40_AD_POSITIVE_AVREFP | _20_AD_NEGATIVE_AVREFM | _00_AD_AREA_MODE_1 ;/* AD reference voltage setting */

    ADC->ADTRG = _00_AD_TRIGGER_SOFTWARE; /* AD trigger selection */
    ADC->ADUL = _FF_AD_ADUL_VALUE;/* AD comversion result comprision upper limit setting */
    ADC->ADLL = _00_AD_ADLL_VALUE;/* AD comversion result comprision lower limit setting */

    /* adhard power up */
    ADC->ADM0 |= ADCE;  

    //bsp_adc_test(4);

    return 0;
}

uint32_t bsp_adc_get_result(uint32_t ch)
{
    uint16_t tmp_val;

    //demo4 1V1版本去掉了PVDD电流检测功能
    if((ADC_CHANNEL_CURRENT_PVDD == ch) 
    && (!bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT))){
        return 0;
    }

    return ADC_Converse(ch, 1, &tmp_val) ;
}

uint32_t bsp_adc_get_result_average(uint32_t ch, uint8_t times)
{
    uint16_t tmp_buf[ADC_AVERAGE_TIMES_MAX];

    //demo4 1V1版本去掉了PVDD电流检测功能
    if((ADC_CHANNEL_CURRENT_PVDD == ch) 
    && (!bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT))){
        return 0;
    }

    return ADC_Converse(ch, times, tmp_buf);
}

void bsp_adc_test(uint32_t times)
{
    uint32_t adc_result;
    if(times == 0){
        times = 1;
    }
    printf("\n%s %d++++\n", __FUNCTION__, times);
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_VOP, 4);
    printf("vop adc %d\n", adc_result);
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_PVDD, 4);
    printf("pvdd adc %d\n", adc_result);
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_CURRENT_PVDD, 4);
    printf("current pvdd adc %d\n", adc_result);
#ifdef BOARD_DEMO4
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_TX_TEMP, 4);
    printf("tx temp adc %d\n", adc_result);
#else
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_DV11, 4);
    printf("dv11 adc %d\n", adc_result);
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_SV11, 4);
    printf("sv11 adc %d\n", adc_result);
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_RX_TEMP, 4);
    printf("rx temp adc %d\n", adc_result);
    adc_result = bsp_adc_get_result_average(ADC_CHANNEL_ADC_VREF, 4);
    printf("vref adc %d\n", adc_result);
#endif
    printf("\n%s ----\n", __FUNCTION__);
}

