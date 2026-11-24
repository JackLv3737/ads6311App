/**
 * @file bsp_pwm.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdio.h>
#include "BAT32A239.h"
#include "userdefine.h"
#include "board_config.h"
#include "timm.h"
#include "bsp_pwm.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(m_array) (sizeof(m_array) / sizeof(m_array[0]))
#endif

#define VOP_ADJ_PORT    1
#define VOP_ADJ_PIN     0    
#define PVDD_ADJ_PORT   1
#define PVDD_ADJ_PIN    1
#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define PVDDL_ADJ_PORT   1
#define PVDDL_ADJ_PIN    6
#else
#define PVDDL_ADJ_PORT   1
#define PVDDL_ADJ_PIN    4
#endif

#define PWM_TIMER_CLOCK TMM_CLOCK_FCLK4
#define PWM_PERIOD        (1000)


static void bsp_pwm_port_init(void)
{
    PORT->P1   &= ~(1 << VOP_ADJ_PIN);    // P10 output low level 
    PORT->PM1  &= ~(1 << VOP_ADJ_PIN);    // P10 is used as TMIOB1 output
    PORT->POM1  &= ~(1 << VOP_ADJ_PIN);   // P10 is push-pull output mode
    PORT->PMC1  &= ~(1 << VOP_ADJ_PIN);   // P10 is digital function

    PORT->P1   &= ~(1 << PVDD_ADJ_PIN);    // P11 output low level 
    PORT->PM1  &= ~(1 << PVDD_ADJ_PIN);    // P11 is used as TMIOD1 output
    PORT->POM1  &= ~(1 << PVDD_ADJ_PIN);   // P11 is push-pull output mode
    PORT->PMC1  &= ~(1 << PVDD_ADJ_PIN);   // P11 is digital function

#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)

    PORT->P1   &= ~(1 << PVDDL_ADJ_PIN);    // P16 output low level 
    PORT->PM1  &= ~(1 << PVDDL_ADJ_PIN);    // P16 is used as TMIOB1 output
    PORT->POM1  &= ~(1 << PVDDL_ADJ_PIN);   // P16 is push-pull output mode
    PORT->PMC1  &= ~(1 << PVDDL_ADJ_PIN);   // P16 is digital function

#elif defined(BOARD_ZUMA)

// 默认PIORx是这样，可以不用配
// PIOR36=0
// PIOR37=0
// PIOR23=0
// PIOR25=0
    PORT->P1   &= ~(1 << PVDDL_ADJ_PIN);    // P14 output low level 
    PORT->PM1  &= ~(1 << PVDDL_ADJ_PIN);    // P14 is used as TMIOB0 output
    PORT->POM1  &= ~(1 << PVDDL_ADJ_PIN);   // P14 is push-pull output mode
    PORT->PMC1  &= ~(1 << PVDDL_ADJ_PIN);   // P14 is digital function
#endif   


}

static void bsp_pwm_timer_init(void)
{

#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)

    CGC->PER1 |= CGC_PER1_TMMEN_Msk;     /* enables input clock supply */
    TMM->TMSTR |= _08_TMM_TM1_COUNT_CONTINUES | _04_TMM_TM0_COUNT_CONTINUES;
    TMM->TMSTR &= (uint8_t)~(_02_TMM_TM1_COUNT_START | _01_TMM_TM0_COUNT_START);
    /* the TMGRC and TMGRD are used as general register */
    TMM->TMMR |= _00_TMM_TMGRC1_GENERAL | _00_TMM_TMGRD1_GENERAL | _00_TMM_TMGRC0_GENERAL | _00_TMM_TMGRD0_GENERAL;
    /* the counter is cleared when it matches the TMGRA register */
    TMM->TMCR0 = _20_TMM_COUNTER_CLEAR_TMGRA | PWM_TIMER_CLOCK;
    TMM->TMCR1 = _20_TMM_COUNTER_CLEAR_TMGRA | PWM_TIMER_CLOCK;
    /* TMIOB1/TMIOC1/TMIOB0 is used as PWM output */
    TMM->TMPMR |= _10_TMM_TMIOB1_PWM_MODE | _40_TMM_TMIOD1_PWM_MODE | _02_TMM_TMIOC0_PWM_MODE;
    /* Set the active level of the output pins */
    TMM->TMPOCR0 = _00_TMIOC_OUTPUT_ACTIVE_L;
    TMM->TMPOCR1 = _00_TMIOD_OUTPUT_ACTIVE_L | _00_TMIOB_OUTPUT_ACTIVE_L;
    /* Set the initial level of the output pins */
    TMM->TMOCR |= _00_TMM_TMIOB1_INITIAL_OUTPUT_L | _00_TMM_TMIOD1_INITIAL_OUTPUT_L | _00_TMM_TMIOC0_INITIAL_OUTPUT_L;
    /* The output is enabled */
    TMM->TMOER1 = 0x00U;
    TMM->TMOER1 |= _10_TMM_TMIOA1_OUTPUT_DISABLE | _00_TMM_TMIOB1_OUTPUT_ENABLE | _40_TMM_TMIOC1_OUTPUT_DISABLE | _00_TMM_TMIOD1_OUTPUT_ENABLE | _00_TMM_TMIOC0_OUTPUT_ENABLE;
    /* Set the interrupt enable register */
    TMM->TMIER0 = _00_TMM_OVIE_DISABLE | _00_TMM_IMID_DISABLE | _00_TMM_IMIC_DISABLE | _00_TMM_IMIB_DISABLE | _00_TMM_IMIA_DISABLE;
    TMM->TMIER1 = _00_TMM_OVIE_DISABLE | _00_TMM_IMID_DISABLE | _00_TMM_IMIC_DISABLE | _00_TMM_IMIB_DISABLE | _00_TMM_IMIA_DISABLE;
    /* Set PWMOP register */
    TMM->OPCTL0 = _00_TMM_PWMOP_NO_CUTOFF;

    //Set period
    TMM->TMGRA0 = PWM_PERIOD - 1;
    TMM->TMGRA1 = PWM_PERIOD - 1;

    TMM->TMGRB1 = PWM_VOP_ADJ_INIT_DUTY;
    TMM->TMGRD1 = PWM_PVDD_ADJ_INIT_DUTY;
    TMM->TMGRC0 = PWM_PVDDL_ADJ_INIT_DUTY;

    //TMM1_Start
    TMM->TMSTR |= _08_TMM_TM1_COUNT_CONTINUES | _04_TMM_TM0_COUNT_CONTINUES;
    TMM->TMSTR |= _02_TMM_TM1_COUNT_START | _01_TMM_TM0_COUNT_START;

#elif defined(BOARD_ZUMA)

    CGC->PER1 |= CGC_PER1_TMMEN_Msk;     /* enables input clock supply */
    TMM->TMSTR |= _08_TMM_TM1_COUNT_CONTINUES | _04_TMM_TM0_COUNT_CONTINUES;
    TMM->TMSTR &= (uint8_t)~(_02_TMM_TM1_COUNT_START | _01_TMM_TM0_COUNT_START);
    /* the TMGRC and TMGRD are used as general register */
    TMM->TMMR |= _00_TMM_TMGRC1_GENERAL | _00_TMM_TMGRD1_GENERAL | _00_TMM_TMGRC0_GENERAL | _00_TMM_TMGRD0_GENERAL;
    /* the counter is cleared when it matches the TMGRA register */
    TMM->TMCR0 = _20_TMM_COUNTER_CLEAR_TMGRA | PWM_TIMER_CLOCK;
    TMM->TMCR1 = _20_TMM_COUNTER_CLEAR_TMGRA | PWM_TIMER_CLOCK;
    /* TMIOB1/TMIOC1/TMIOB0 is used as PWM output */
    TMM->TMPMR |= _10_TMM_TMIOB1_PWM_MODE | _40_TMM_TMIOD1_PWM_MODE | _01_TMM_TMIOB0_PWM_MODE;
    /* Set the active level of the output pins */
    TMM->TMPOCR0 = _00_TMIOB_OUTPUT_ACTIVE_L;
    TMM->TMPOCR1 = _00_TMIOD_OUTPUT_ACTIVE_L | _00_TMIOB_OUTPUT_ACTIVE_L;
    /* Set the initial level of the output pins */
    TMM->TMOCR |= _00_TMM_TMIOB1_INITIAL_OUTPUT_L | _00_TMM_TMIOD1_INITIAL_OUTPUT_L | _00_TMM_TMIOB0_INITIAL_OUTPUT_L;
    /* The output is enabled */
    TMM->TMOER1 = 0x00U;
    TMM->TMOER1 |= _10_TMM_TMIOA1_OUTPUT_DISABLE | _00_TMM_TMIOB1_OUTPUT_ENABLE | _40_TMM_TMIOC1_OUTPUT_DISABLE | _00_TMM_TMIOD1_OUTPUT_ENABLE | _00_TMM_TMIOB0_OUTPUT_ENABLE;
    /* Set the interrupt enable register */
    TMM->TMIER0 = _00_TMM_OVIE_DISABLE | _00_TMM_IMID_DISABLE | _00_TMM_IMIC_DISABLE | _00_TMM_IMIB_DISABLE | _00_TMM_IMIA_DISABLE;
    TMM->TMIER1 = _00_TMM_OVIE_DISABLE | _00_TMM_IMID_DISABLE | _00_TMM_IMIC_DISABLE | _00_TMM_IMIB_DISABLE | _00_TMM_IMIA_DISABLE;
    /* Set PWMOP register */
    TMM->OPCTL0 = _00_TMM_PWMOP_NO_CUTOFF;

    //Set period
    TMM->TMGRA0 = PWM_PERIOD - 1;
    TMM->TMGRA1 = PWM_PERIOD - 1;

    TMM->TMGRB1 = PWM_VOP_ADJ_INIT_DUTY;
    TMM->TMGRD1 = PWM_PVDD_ADJ_INIT_DUTY;
    TMM->TMGRB0 = PWM_PVDDL_ADJ_INIT_DUTY;

    //TMM1_Start
    TMM->TMSTR |= _08_TMM_TM1_COUNT_CONTINUES | _04_TMM_TM0_COUNT_CONTINUES;
    TMM->TMSTR |= _02_TMM_TM1_COUNT_START | _01_TMM_TM0_COUNT_START;

#else
    CGC->PER1 |= CGC_PER1_TMMEN_Msk;     /* enables input clock supply */
    TMM->TMSTR |= _08_TMM_TM1_COUNT_CONTINUES;
    TMM->TMSTR &= (uint8_t)~_02_TMM_TM1_COUNT_START;
    /* the TMGRC and TMGRD are used as general register */
    TMM->TMMR |= _00_TMM_TMGRC1_GENERAL | _00_TMM_TMGRD1_GENERAL;
    /* the counter is cleared when it matches the TMGRA register */ 
    TMM->TMCR1 = _20_TMM_COUNTER_CLEAR_TMGRA | PWM_TIMER_CLOCK;
    /* TMIOB1/TMIOC1/TMIOD1 is used as PWM output */
    TMM->TMPMR |= _10_TMM_TMIOB1_PWM_MODE | _40_TMM_TMIOD1_PWM_MODE;
    /* Set the active level of the output pins */
    TMM->TMPOCR1 = _00_TMIOD_OUTPUT_ACTIVE_L | _00_TMIOB_OUTPUT_ACTIVE_L;
    /* Set the initial level of the output pins */
    TMM->TMOCR |= _00_TMM_TMIOB1_INITIAL_OUTPUT_L | _00_TMM_TMIOD1_INITIAL_OUTPUT_L;
    /* The output is enabled */
    TMM->TMOER1 &= 0x0FU;
    TMM->TMOER1 |= _10_TMM_TMIOA1_OUTPUT_DISABLE | _00_TMM_TMIOB1_OUTPUT_ENABLE | _40_TMM_TMIOC1_OUTPUT_DISABLE | _00_TMM_TMIOD1_OUTPUT_ENABLE;
    /* Set the interrupt enable register */
    TMM->TMIER1 = _00_TMM_OVIE_DISABLE | _00_TMM_IMID_DISABLE | _00_TMM_IMIC_DISABLE | _00_TMM_IMIB_DISABLE | _00_TMM_IMIA_DISABLE;
    /* Set PWMOP register */
    TMM->OPCTL0 = _00_TMM_PWMOP_NO_CUTOFF;

    //Set period
    TMM->TMGRA1 = PWM_PERIOD - 1;

    TMM->TMGRB1 = PWM_VOP_ADJ_INIT_DUTY;
    TMM->TMGRD1 = PWM_PVDD_ADJ_INIT_DUTY;

    //TMM1_Start
    TMM->TMSTR |= _08_TMM_TM1_COUNT_CONTINUES;
    TMM->TMSTR |= _02_TMM_TM1_COUNT_START;
#endif

}



 
int bsp_pwm_init(void)
{
    bsp_pwm_port_init();

    bsp_pwm_timer_init();

    return 0;
}

int bsp_pwm_set_duty(uint32_t channel, uint32_t duty)
{
    if(duty > PWM_MAX_VALUE || duty < PWM_MIN_VALUE)
        return -1;
#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
    if(channel == PWM_VOP_CHANNEL){
        TMM->TMGRB1 = duty; //vop
    }else if(channel == PWM_PVDD_CHANNEL){
        TMM->TMGRD1 = duty;//pvdd
    }else if(channel == PWM_PVDDL_CHANNEL){
        TMM->TMGRC0 = duty;//pvdd L
    }
#else
    if(channel == PWM_VOP_CHANNEL){
        TMM->TMGRB1 = duty; //vop
    }else if(channel == PWM_PVDD_CHANNEL){
        TMM->TMGRD1 = duty;//pvdd
    }else if(channel == PWM_PVDDL_CHANNEL){
        TMM->TMGRB0 = duty;//pvdd L
    }
#endif

    return 0;
}


uint32_t bsp_pwm_get_duty(uint32_t channel)
{
#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
    if(channel == PWM_VOP_CHANNEL){
        return TMM->TMGRB1; //vop
    }else if(channel == PWM_PVDD_CHANNEL){
        return TMM->TMGRD1;//pvdd
    }else if(channel == PWM_PVDDL_CHANNEL){
        return TMM->TMGRC0;//pvdd L
    }
#else
    if(channel == PWM_VOP_CHANNEL){
        return TMM->TMGRB1; //vop
    }else if(channel == PWM_PVDD_CHANNEL){
        return TMM->TMGRD1;//pvdd
    }else if(channel == PWM_PVDDL_CHANNEL){
        return TMM->TMGRB0;//pvdd L
    }
#endif
    return 0;
}


void bsp_pwm_test(void)
{
    static char test_index = 0;
    const uint16_t val_tab1[4] = {200, 400, 600, 800};
    const uint16_t val_tab2[4] = {100, 300, 700, 900};
    const uint16_t val_tab3[4] = {300, 500, 800, 950};
    printf("\n%s ++++\n", __FUNCTION__);
    bsp_pwm_set_duty(PWM_VOP_CHANNEL, val_tab1[test_index]);
    bsp_pwm_set_duty(PWM_PVDD_CHANNEL, val_tab2[test_index]);
    bsp_pwm_set_duty(PWM_PVDDL_CHANNEL, val_tab3[test_index]);
    printf("vop pwm duty set to %u\n", val_tab1[test_index]);
    printf("pvdd pwm duty set to %u\n", val_tab2[test_index]);
    printf("pvddl pwm duty set to %u\n", val_tab3[test_index]);
    test_index = (++test_index) % ARRAY_SIZE(val_tab1);
    printf("\n%s ----\n", __FUNCTION__);
}
