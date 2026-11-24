/**
 * @file board_config.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__
#include <stdint.h>
#include <stddef.h> //for NULL
#include "userdefine.h"

//从高到低中断优先级: RX Req > PVDD关断定时器> spi 通讯
//A239有4个中断优先级(0~3) ,   数字越低，中断优先级越高
#define IRQ_PRIOR_RX_REQ  1
#define IRQ_PRIOR_PVDD_SHUTDOWN  0
#define IRQ_PRIOR_TELM  2
#define IRQ_PRIOR_PVDD_SWITCH  0

#define TXSWITCH_USE_SHARK
//#define TXSWITCH_USE_7726A

#define BOARD_DEMO4
//#define BOARD_ZUMA // DEMO4 Variant
//#define BOARD_MOVA // DEMO4 Variant
#define BOARD_DEMO4_V3 // DEMO4 Variant

#ifdef BOARD_DEMO4
// #define TX_REQ_USING_SSYNC
#endif

#define MCU_MODEL_STM32_F407     0xF407
#define MCU_MODEL_CMS_A239     0xA239 

#define BOARD_MCU_MODEL   MCU_MODEL_CMS_A239

#ifdef BOARD_DEMO4
#define DEMO_BOARD_VERSION   4
#define DEMO_BOARD_NAME "Demo4-A239"

#define HW_HAVE_SPI_FLASH

#define HW_REMOVE_DAC
#define HW_REMOVE_NST112
#define HW_REMOVE_SV11_ADC
#define HW_REMOVE_DV11_ADC
#define HW_REMOVE_VREF_ADC
#define HW_REMOVE_RX_TEMP

#ifdef BOARD_ZUMA
#define DEMO_BOARD_ALIAS "Zuma"
#define HW_VERSION_MINOR 2 // Customer ID
#define HW_VERSION_REVISION 0
#define HW_ONLY_ONE_ADS7311
#define HW_HAVE_PVDDL
#define HW_HAVE_PVDD_ICT
#define HW_HAVE_BMI_EN
#define HW_HAVE_ADS6311_EN
#define HW_HAVE_ADS6311_I2C_EN
#define HW_FIX_VARIANT_VER1
#define HW_REMOVE_HROLL
#define HW_REMOVE_VCC_3V3_EN
#elif defined(BOARD_MOVA)
#define DEMO_BOARD_ALIAS "Mova"
#define HW_VERSION_MINOR 1 // Customer ID
#define HW_VERSION_REVISION 0
#define HW_TELM_USING_IIC
#define HW_ONLY_ONE_ADS7311
#define HW_HAVE_PVDDL
#define HW_HAVE_PVDD_ICT
#define HW_HAVE_BMI_EN
#define HW_FIX_VARIANT_VER1
#define HW_REMOVE_HROLL
#define HW_REMOVE_VCC_3V3_EN
#define HW_REMOVE_RUN_LED
#elif defined(BOARD_DEMO4_V3)
#define DEMO_BOARD_ALIAS "Demo4v3"
#define HW_VERSION_MINOR 0 // Customer ID
#define HW_VERSION_REVISION 4
#define HW_ONLY_ONE_ADS7311
#define HW_HAVE_PVDDL
#define HW_HAVE_PVDD_ICT
#define HW_HAVE_BMI_EN
#define HW_HOST_BMI_CHIP
#define HW_FIX_VARIANT_VER1
#define HW_REMOVE_HROLL
#define HW_REMOVE_VCC_3V3_EN
#define HW_REMOVE_RUN_LED
#else
#define DEMO_BOARD_ALIAS "Demo4"
#define HW_VERSION_MINOR 0 // Customer ID
#define HW_VERSION_REVISION 0
#define BOARD_TAG_USE_VARIANT
#define HW_HAVE_TRIG_EN
#define HW_HAVE_CLKOUT
#endif

#else
#define DEMO_BOARD_VERSION   3
#define HW_VERSION_MINOR 0 // Customer ID
#define HW_VERSION_REVISION 0
#define DEMO_BOARD_NAME "Demo3-A239"
#define DEMO_BOARD_ALIAS "Demo3"
#endif

#define HW_VERSION_MAJOR DEMO_BOARD_VERSION
#define DEMO_BOARD_TAG  ((HW_VERSION_MINOR << 8) | (HW_VERSION_REVISION))

//如下两个宏的m_vol为10mv单位
#define PVDD_PWM_DUTY_CALC(m_vol)  (((1 / 2.2 - ((m_vol / 100.0) - 1) / 100) * (3.3 + 2.2) + 1) / 3.3 * 1000)
#ifdef BOARD_ZUMA
//PVDD_L=15.69-50*PWM,(PWM=0~1), 单位V ==> PWM_1000duty = ((15.69 - (PVDD_L_mv / 1000)) / 50.0f) * 1000
#define PVDDL_VOL_MAX  1569 //单位10mv 
#define PVDDL_PWM_DUTY_CALC(m_vol)  ((PVDDL_VOL_MAX - m_vol) / 5.0f)
#else
//PVDD_L=17.6-59.4*PWM,(PWM=0~1), 单位V ==> PWM_1000duty = ((17.6 - (PVDD_L_mv / 1000)) / 59.4f) * 1000
#define PVDDL_VOL_MAX  1760 //单位10mv 
#define PVDDL_PWM_DUTY_CALC(m_vol)  ((PVDDL_VOL_MAX - m_vol) / 5.94f)
#endif
//如下两个宏的m_vol为10mv单位的负值VOP电压
#define VOP_PWM_DUTY_CALC_V0(m_vol)        ((((0.8 - (m_vol / 100.0)) / 1000 - 0.8 / 34) * (156 + 100) + 0.8) / 3.3 * 1000)
#if defined(BOARD_ZUMA) || defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define VOP_PWM_DUTY_CALC_V1(m_vol)        (((m_vol + 1621.8f) / -929.94f) * 1000)
#else
#define VOP_PWM_DUTY_CALC_V1(m_vol)        ((((0.8 - (m_vol / 100.0)) / 1000 - 0.8 / 39) * (139 + 100) + 0.8) / 3.3 * 1000)
#endif

#define PVDD_VOL_CALC(m_adc_val)    ((m_adc_val)  / 4095.0f * 2.5f * 102.61f / 2.61f)
#define PVDDL_VOL_CALC(m_adc_val)    ((m_adc_val)  / 4095.0f * 2.5f / 0.0254f)
//计算结果为以V为单位的浮点数，绝对值
#if defined(BOARD_ZUMA) || defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
//ADC=2.2727+0.09091*VOP
#define VOP_VOL_CALC(m_adc_val)     ((2.2727f - ((float)(m_adc_val) / 4095.0f * 2.5f)) / 0.09091)
#else
#define VOP_VOL_CALC(m_adc_val)     ((160 - ((m_adc_val)  / 4095.0f * 2.5f * 104.7f)) / 4.7f)
#endif

#define DAC_DEFAULT_VOL 2400
#if defined(BOARD_ZUMA) || defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define PVDD_VOL_DEFAULT    0 //mv
#define VOP_VOL_DEFAULT      0 //mv
#else
#define PVDD_VOL_DEFAULT    20000 //mv
#define VOP_VOL_DEFAULT      -20000 //mv
#endif
#define VOP_VOL_DEFAULT_V1   -17000 //mv
#define VOP_CHECK_HW_VER_THRESHOLD      17500 //mv，绝对值

//根据target电压转换为期望的adc检测值（由硬件部门实测提供）
#define PVDD_ADC_VAL_CALC(m_vol)    (m_vol / (100.0) * 2.61 / 102.61 / 2.5 * 4095)

//根据target电压转换为期望的adc检测值（由硬件部门实测提供）
//m_vol单位为10mv的负电压
#if defined(BOARD_ZUMA) || defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
//ADC=2.2727+0.09091*VOP
#define VOP_ADC_VAL_CALC(m_vol) ((uint32_t)(((2.2727f + ((float)(m_vol) / 100 * 0.09091f)) / 2.5f) * 4095))
#else
//加0.5是为了四舍五入
#define VOP_ADC_VAL_CALC(m_vol) (((uint32_t)(((((m_vol) / 100.0 - 1.6) * 4.7 / 104.7 + 1.6) / 2.5 * 4095) + 0.5)) - 9)
#endif

//VOP ADC最小值
#define VOP_ADC_VAL_MIN     10

#define PWM_VOP_ADJ_INIT_DUTY   0
#define PWM_PVDD_ADJ_INIT_DUTY   1000
#define PWM_PVDDL_ADJ_INIT_DUTY   0

//通过寄存器能配的PVDD最低电压
#define PVDD_MIN_VAL    900 //unit 10mv

//常用PVDD电压最小值
#define PVDD_USUALLY_VOL_MIN    20000 //unit: mv

//常用PVDD最小电压值的PWM占空比上限
#define PVDD_USUALLY_PWM_MAX    780

//
#define INA_RESET_KEEP_TIME  3//unit:ms

#define PVDD_PID_DELAY_TIME 5//unit:ms

#define PVDD_PID_CTRL_THRESHOLD 4000 //unit:mv

#define PVDD_PID_PERIOD 10 //unit:ms
#define VOP_PID_PERIOD 10 //unit:ms

#define VOP_ADC_ERR     120 //unit:mv, VOP采样误差

#define PVDD_CALI_PARAM_P       (0.015f)
#define PVDD_CALI_PRECISION_DEFAULT     (100) //unit:mv
#define PVDD_CALI_PRECISION_SEQ1     (100) //unit:mv
#define PVDD_CALI_PRECISION_SEQ2     (150) //unit:mv
#define PVDD_CALI_PRECISION_SEQ3     (200) //unit:mv
#define PVDD_CALI_TIMEOUT       (12000) //unit:ms
#define PVDD_CALI_PID_INTERVAL  (100) //unit:ms
#define PVDD_CALI_STABLE_TIME   (1000)//unit:ms
#define PVDD_CALI_CTRL_THRESHOLD    (3000)//unit:mv
#define PVDD_CALI_STABLE_THRESHOLD  (400)//unit:mv
#define PVDD_CALI_STABLE_TIMEOUT    (4000) //unit:ms

#define PVDD_CALI_VOL_MIN   (PVDD_MIN_VAL * 10) //PVDD_USUALLY_VOL_MIN//unit:mv
#define PVDD_CALI_VOL_MAX   (63000)//unit:mv

#define PVDD_POWERUP_TIME_MAX   2000 //unit:ms
#define VOP_POWERUP_TIME_MAX   2000 //unit:ms


//gpio_port_t、GPIO_PORT_INVALID的定义因MCU平台而已，以便于移植
typedef unsigned char gpio_port_t;
#define GPIO_PORT_INVALID   0xFF
#define GPIO_PIN_INVALID   0xFF

/***************************GPIO definition*************************************/
#define ARRAY_VROLL_PORT_0                      6
#define ARRAY_VROLL_0                           0

#define ARRAY_VROLL_PORT_1                      6
#define ARRAY_VROLL_1                           1

#define ARRAY_VROLL_PORT_2                      6
#define ARRAY_VROLL_2                           3

#define ARRAY_VROLL_PORT_3                      7
#define ARRAY_VROLL_3                           7

#define ARRAY_VROLL_PORT_4                      7
#define ARRAY_VROLL_4                           6

#ifdef BOARD_DEMO4

#ifdef HW_REMOVE_HROLL
#define ARRAY_HROLL_PORT_0                      GPIO_PORT_INVALID
#define ARRAY_HROLL_0                           GPIO_PIN_INVALID

#define ARRAY_HROLL_PORT_1                      GPIO_PORT_INVALID
#define ARRAY_HROLL_1                           GPIO_PIN_INVALID

#define ARRAY_HROLL_PORT_2                      GPIO_PORT_INVALID
#define ARRAY_HROLL_2                           GPIO_PIN_INVALID

#define ARRAY_HROLL_PORT_3                      GPIO_PORT_INVALID
#define ARRAY_HROLL_3                           GPIO_PIN_INVALID

#else

#define ARRAY_HROLL_PORT_0                      2
#define ARRAY_HROLL_0                           4

#define ARRAY_HROLL_PORT_1                      2
#define ARRAY_HROLL_1                           5

#define ARRAY_HROLL_PORT_2                      2
#define ARRAY_HROLL_2                           6

#define ARRAY_HROLL_PORT_3                      2
#define ARRAY_HROLL_3                           7
#endif


#else

#define ARRAY_HROLL_PORT_0                      2
#define ARRAY_HROLL_0                           5

#define ARRAY_HROLL_PORT_1                      GPIO_PORT_INVALID
#define ARRAY_HROLL_1                           GPIO_PIN_INVALID

#define ARRAY_HROLL_PORT_2                      GPIO_PORT_INVALID
#define ARRAY_HROLL_2                           GPIO_PIN_INVALID

#define ARRAY_HROLL_PORT_3                      GPIO_PORT_INVALID
#define ARRAY_HROLL_3                           GPIO_PIN_INVALID

#endif


#define AV33_SV33_EN_PORT   12
#define AV33_SV33_EN_PIN    0


#ifdef BOARD_DEMO4
#ifdef HW_REMOVE_VCC_3V3_EN
#define VCC_3V3_EN_PORT     GPIO_PORT_INVALID
#define VCC_3V3_EN_PIN      GPIO_PIN_INVALID
#else
#define VCC_3V3_EN_PORT     1
#define VCC_3V3_EN_PIN      5
#endif
#else
#define VCC_3V3_EN_PORT     4
#define VCC_3V3_EN_PIN      2
#endif


#ifdef HW_HAVE_PVDDL
#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define PVDDL_EN_PORT     0
#define PVDDL_EN_PIN      0
#else
#define PVDDL_EN_PORT     1
#define PVDDL_EN_PIN      5
#endif
#else
#define PVDDL_EN_PORT     GPIO_PORT_INVALID
#define PVDDL_EN_PIN      GPIO_PIN_INVALID
#endif


#define VCC_DRV_EN_PORT     4
#define VCC_DRV_EN_PIN      1

#define IOV18_EN_PORT       7
#define IOV18_EN_PIN        3

#define SV11_DV11_EN_PORT   1
#define SV11_DV11_EN_PIN    7

#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define AV11_EN_PORT        1
#define AV11_EN_PIN         2
#else
#define AV11_EN_PORT        1
#define AV11_EN_PIN         6
#endif

#define VOP_EN_PORT         14
#define VOP_EN_PIN          0

#define PVDD_EN_PORT        14
#define PVDD_EN_PIN         1

//P130只有输出功能
#define INA_RST_PORT        13
#define INA_RST_PIN         0

#define RK_ERR_PORT         3
#define RK_ERR_PIN          1

#ifdef BOARD_DEMO4
#define RX_SWTACK_PORT      7
#define RX_SWTACK_PIN       5
#else
#define RX_SWTACK_PORT      5
#define RX_SWTACK_PIN       3
#endif

#ifdef BOARD_DEMO4
#define RX_DRV_ERR_PORT     7
#define RX_DRV_ERR_PIN      4
#else
#define RX_DRV_ERR_PORT     5
#define RX_DRV_ERR_PIN      4
#endif

#define S1_S2_EN_PORT       0
#define S1_S2_EN_PIN        5

#ifdef HW_HAVE_TRIG_EN
#define TX_TRIG_EN_PORT     2
#define TX_TRIG_EN_PIN      3
#else
#define TX_TRIG_EN_PORT     GPIO_PORT_INVALID
#define TX_TRIG_EN_PIN      GPIO_PIN_INVALID
#endif

#ifdef HW_HAVE_BMI_EN
#define BMI_EN_PORT     2
#define BMI_EN_PIN      3
#else
#define BMI_EN_PORT     GPIO_PORT_INVALID
#define BMI_EN_PIN      GPIO_PIN_INVALID
#endif

#ifdef HW_REMOVE_RUN_LED
#define LED2_RUN_PORT   GPIO_PORT_INVALID
#define LED2_RUN_PIN    GPIO_PIN_INVALID
#else
#define LED2_RUN_PORT   13
#define LED2_RUN_PIN    6
#endif

#define PVDD_SW1_PORT     13  //
#define PVDD_SW1_PIN      0

#define PVDD_SW2_PORT     0  //
#define PVDD_SW2_PIN      4

#ifdef HW_HAVE_ADS6311_I2C_EN
#define ADS6311_I2C_EN_PORT 0
#define ADS6311_I2C_EN_PIN 0
#else
#define ADS6311_I2C_EN_PORT GPIO_PORT_INVALID
#define ADS6311_I2C_EN_PIN GPIO_PIN_INVALID
#endif

#ifdef HW_HAVE_ADS6311_EN
#define ADS6311_EN_PORT 0
#define ADS6311_EN_PIN 1
#else
#define ADS6311_EN_PORT GPIO_PORT_INVALID
#define ADS6311_EN_PIN GPIO_PIN_INVALID
#endif




/*************************** ADC definition**************************************/


#define ADC_CHANNEL_VOP                         15  //VOP_TEST, ANI15
#define ADC_CHANNEL_PVDD                        12  //PVDD_TEST, ANI12
#ifdef HW_HAVE_PVDDL
#define ADC_CHANNEL_PVDDL                       7  // ANI7 P27
#endif

#ifdef HW_HAVE_PVDD_ICT
#define ADC_CHANNEL_PVDD_ICT                    4  //  ANI4 P24
#endif

#define ADC_CHANNEL_CURRENT_PVDD                13  //INA_VOUT, ANI13
#ifdef BOARD_DEMO4
#define ADC_CHANNEL_TX_TEMP                     2  //ADC_NTC, ANI2
#else
#define ADC_CHANNEL_DV11                        7   //DV11_FB, ANI7
#define ADC_CHANNEL_SV11                        6   //SV11_FB, ANI6
#define ADC_CHANNEL_ADC_VREF                    3   //ADC_VREF, ANI3
#define ADC_CHANNEL_RX_TEMP                     4   //ADC_NTC, ANI4
#endif


/***************************PWM definition**************************************/
#define PWM_VOP_CHANNEL                         3//
#define PWM_PVDD_CHANNEL                        4//
#define PWM_PVDDL_CHANNEL                       2//

#define VREF                                    2500   //2.5V
#define ADC_MAX                                 4096



/***************************interrupt gpio definition**************************************/
//INA_CMP : P01
#define INA_CMP_PORT    0
#define INA_CMP_PIN     1
#define INA_CMP_INTPX    10
#define INA_CMP_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 7);    /* allocate INTP10 to P01 */ \
        PORT->PM0   |= (1 << 1);    /* P01 is used as INTP10 input */ \
}while(0)
#define INTP10_ENABLE

//RX_SWTREQ : P52
#define RX_SWTREQ_PORT    5
#define RX_SWTREQ_PIN     2
#define RX_SWTREQ_INTPX   1
#define RX_SWTREQ_IRQ   INTP1_IRQn
#define RX_SWTREQ_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP1 to P52 */ \
        PORT->PM5   |= (1 << RX_SWTREQ_PIN);    /* P52 is used as INTP1 input */ \
}while(0)
#define INTP1_ENABLE

#ifdef BOARD_DEMO4
//RX_SSYNC : P53  
#define RX_SSYNC_PORT    5
#define RX_SSYNC_PIN     3
#define RX_SSYNC_INTPX   2
#define RX_SSYNC_IRQ   INTP2_IRQn
#define RX_SSYNC_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP2 to P53 */ \
        PORT->PM5   |= (1 << RX_SSYNC_PIN);    /* P53 is used as INTP2 input */ \
}while(0)
#define INTP2_ENABLE

//RX_FSYNC : P54
#define RX_FSYNC_PORT    5
#define RX_FSYNC_PIN     4
#define RX_FSYNC_INTPX   3
#define RX_FSYNC_IRQ   INTP3_IRQn
#define RX_FSYNC_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP3 to P54 */ \
        PORT->PM5   |= (1 << RX_FSYNC_PIN);    /* P54 is used as INTP2 input */ \
}while(0)
#define INTP3_ENABLE

#else
//RX_SSYNC : P00
#define RX_SSYNC_PORT    0
#define RX_SSYNC_PIN     0
#define RX_SSYNC_INTPX   8
#define RX_SSYNC_IRQ   INTP8_IRQn
#define RX_SSYNC_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 7);    /* allocate INTP8 to P00 */ \
        PORT->PM0   |= (1 << 0);    /* P00 is used as INTP8 input */ \
}while(0)
#define INTP8_ENABLE

//RX_FSYNC : P54
#define RX_FSYNC_PORT    GPIO_PORT_INVALID
#define RX_FSYNC_PIN     GPIO_PIN_INVALID
#define RX_FSYNC_INTPX   0
#define RX_FSYNC_INTP_PORT_SETTING()

#endif

#ifdef BOARD_DEMO4
//S1_ERR : P42
#define S1_ERR_PORT    4
#define S1_ERR_PIN     2
#define S1_ERR_INTPX   8
#define S1_ERR_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP8 to P42 */ \
        PORT->PM4   |= (1 << 2);    /* P42 is used as INTP8 input */ \
}while(0)
#define INTP8_ENABLE
#else
//S1_ERR : P55
#define S1_ERR_PORT    5
#define S1_ERR_PIN     5
#define S1_ERR_INTPX   4
#define S1_ERR_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP4 to P55 */ \
        PORT->PM5   |= (1 << 5);    /* P55 is used as INTP4 input */ \
}while(0)
#define INTP4_ENABLE
#endif

#ifdef HW_ONLY_ONE_ADS7311

#define S2_ERR_PORT    GPIO_PORT_INVALID
#define S2_ERR_PIN     GPIO_PIN_INVALID
#define S2_ERR_INTPX   0
#define S2_ERR_INTP_PORT_SETTING()

#else
//S2_ERR : P43
#define S2_ERR_PORT    4
#define S2_ERR_PIN     3
#define S2_ERR_INTPX   9
#define S2_ERR_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP9 to P43 */ \
        PORT->PM4   |= (1 << 3);    /* P43 is used as INTP9 input */ \
}while(0)
#define INTP9_ENABLE
#endif

#ifdef HW_HOST_BMI_CHIP

//BMI_INT1 : P55
#define BMI_INT1_PORT    5
#define BMI_INT1_PIN     5
#define BMI_INT1_INTPX   4
#define BMI_INT1_INTP_PORT_SETTING() do{ \
        PORT->PIOR0 |= (1 << 0);    /* allocate INTP4 to P55 */ \
        PORT->PM5   |= (1 << 5);    /* P55 is used as INTP4 input */ \
}while(0)
#define INTP4_ENABLE

//BMI_INT2 : P136
#define BMI_INT2_PORT    13
#define BMI_INT2_PIN     6
#define BMI_INT2_INTPX   0
#define BMI_INT2_INTP_PORT_SETTING() do{ \
        PORT->PM13   |= (1 << 6);    /* P136 is used as INTP0 input */ \
}while(0)
#define INTP0_ENABLE

#else

#define BMI_INT1_PORT    GPIO_PORT_INVALID
#define BMI_INT1_PIN     GPIO_PIN_INVALID
#define BMI_INT1_INTPX   0
#define BMI_INT1_INTP_PORT_SETTING()

#define BMI_INT2_PORT    GPIO_PORT_INVALID
#define BMI_INT2_PIN     GPIO_PIN_INVALID
#define BMI_INT2_INTPX   0
#define BMI_INT2_INTP_PORT_SETTING()

#endif


#ifdef TX_REQ_USING_SSYNC
#define TX_REQ_PORT     RX_SSYNC_PORT
#define TX_REQ_PIN      RX_SSYNC_PIN
#else
#define TX_REQ_PORT     RX_SWTREQ_PORT
#define TX_REQ_PIN      RX_SWTREQ_PIN
#endif


/***************************For Porting**************************************/
#ifdef BOARD_DEMO4
#define ADC_CHANNEL_RX_TEMPERATURE	ADC_CHANNEL_TX_TEMP
#else
#define ADC_CHANNEL_RX_TEMPERATURE	ADC_CHANNEL_RX_TEMP
#define ADC_CHANNEL_VREF                ADC_CHANNEL_ADC_VREF
#endif

#define HBD1_EN_PORT GPIO_PORT_INVALID
#define HBD2_EN_PORT GPIO_PORT_INVALID
#define HBD1_EN_PIN GPIO_PIN_INVALID
#define HBD2_EN_PIN GPIO_PIN_INVALID


#define VCC_3V3_EN                              VCC_3V3_EN_PIN

#define DV11_EN_PORT                       SV11_DV11_EN_PORT
#define DV11_EN                                 SV11_DV11_EN_PIN

#define SV11_EN_PORT                       SV11_DV11_EN_PORT
#define SV11_EN                                 SV11_DV11_EN_PIN

#define AV11_EN                                 AV11_EN_PIN

#define MP_VCC11A_EN_PORT           GPIO_PORT_INVALID
#define MP_VCC11A_EN                      GPIO_PIN_INVALID

#define IOV18_EN                                IOV18_EN_PIN

#define MP_VCC18_EN_PORT                   GPIO_PORT_INVALID
#define MP_VCC18_EN                             GPIO_PIN_INVALID

#define SV33_EN_PORT                       AV33_SV33_EN_PORT
#define SV33_EN                                 AV33_SV33_EN_PIN

#define AV33_EN_PORT                       AV33_SV33_EN_PORT
#define AV33_EN                                 AV33_SV33_EN_PIN

#define IOV33_DRV_EN_PORT            GPIO_PORT_INVALID
#define IOV33_DRV_EN            GPIO_PIN_INVALID

#define IOV33_TRIG_EN_PORT            GPIO_PORT_INVALID
#define IOV33_TRIG_EN            GPIO_PIN_INVALID

#define VCC_5V_DRV_EN_PORT                            VCC_DRV_EN_PORT
#define VCC_5V_DRV_EN                            VCC_DRV_EN_PIN


#define VOP_PORT                                VOP_EN_PORT
#define VOP_EN                                   VOP_EN_PIN

#define PVDD_PORT                                PVDD_EN_PORT
#define PVDD_EN                                 PVDD_EN_PIN


#define RX_VROLL0_PORT                          ARRAY_VROLL_PORT_0
#define RX_VROLL0_PIN                           ARRAY_VROLL_0
#define RX_VROLL1_PORT                          ARRAY_VROLL_PORT_1
#define RX_VROLL1_PIN                           ARRAY_VROLL_1
#define RX_VROLL2_PORT                          ARRAY_VROLL_PORT_2
#define RX_VROLL2_PIN                           ARRAY_VROLL_2
#define RX_VROLL3_PORT                          ARRAY_VROLL_PORT_3
#define RX_VROLL3_PIN                           ARRAY_VROLL_3
#define RX_VROLL4_PORT                          ARRAY_VROLL_PORT_4
#define RX_VROLL4_PIN                           ARRAY_VROLL_4

#define RX_HROLL0_PORT                          ARRAY_HROLL_PORT_0
#define RX_HROLL0_PIN                           ARRAY_HROLL_0
#define RX_HROLL1_PORT                          ARRAY_HROLL_PORT_1
#define RX_HROLL1_PIN                           ARRAY_HROLL_1
#define RX_HROLL2_PORT                          ARRAY_HROLL_PORT_2
#define RX_HROLL2_PIN                           ARRAY_HROLL_2
#define RX_HROLL3_PORT                          ARRAY_HROLL_PORT_3
#define RX_HROLL3_PIN                           ARRAY_HROLL_3


#define LED2_PORT   LED2_RUN_PORT
#define LED2_PIN    LED2_RUN_PIN
#define LED3_PORT   GPIO_PORT_INVALID
#define LED3_PIN    GPIO_PIN_INVALID


/***************************Spi Channel Config**************************************/
enum spi_chn_e
{
  SPI_CHN0 = 0,
  SPI_CHN1,
  SPI_CHN2,
  SPI_CHN3,
  SPI_CHN4,
  SPI_CHN5,
  SPI_CHN6,
  SPI_CHN7,
  SPI_CHN_COUNT
};

#define SPI_CHN_INVALID SPI_CHN_COUNT

#ifdef BOARD_DEMO4
#define FLASH_SPI_CHAN  SPI_CHN4
#define ADS7311_CHIP1_SPI_CHN     SPI_CHN5
#ifdef HW_ONLY_ONE_ADS7311
#define ADS7311_CHIP2_SPI_CHN     SPI_CHN_INVALID
#else
#define ADS7311_CHIP2_SPI_CHN     SPI_CHN6
#endif

#ifdef BOARD_DEMO4_V3
#define IMU_SPI_CHAN    SPI_CHN0
#define SPI_CHN0_CS_PORT 0
#define SPI_CHN0_CS_PIN 1
#else
#define IMU_SPI_CHAN SPI_CHN_INVALID
#define SPI_CHN0_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN0_CS_PIN GPIO_PORT_INVALID
#endif

#define SPI_CHN1_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN1_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN2_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN2_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN3_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN3_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN4_CS_PORT 0
#define SPI_CHN4_CS_PIN 6

#if defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define SPI_CHN5_CS_PORT 4
#define SPI_CHN5_CS_PIN 3
#else
#define SPI_CHN5_CS_PORT 1
#define SPI_CHN5_CS_PIN 3
#endif

#if defined(BOARD_ZUMA) || defined(BOARD_MOVA) || defined(BOARD_DEMO4_V3)
#define SPI_CHN6_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN6_CS_PIN GPIO_PORT_INVALID
#else
#define SPI_CHN6_CS_PORT 1
#define SPI_CHN6_CS_PIN 4
#endif

#define SPI_CHN7_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN7_CS_PIN GPIO_PORT_INVALID

#else
#define FLASH_SPI_CHAN  SPI_CHN_INVALID
#define ADS7311_CHIP1_SPI_CHN     SPI_CHN0
#define ADS7311_CHIP2_SPI_CHN     SPI_CHN4
#define IMU_SPI_CHAN SPI_CHN_INVALID

#define SPI_CHN0_CS_PORT 1
#define SPI_CHN0_CS_PIN 2

#define SPI_CHN1_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN1_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN2_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN2_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN3_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN3_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN4_CS_PORT 0
#define SPI_CHN4_CS_PIN 6

#define SPI_CHN5_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN5_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN6_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN6_CS_PIN GPIO_PORT_INVALID

#define SPI_CHN7_CS_PORT GPIO_PORT_INVALID
#define SPI_CHN7_CS_PIN GPIO_PORT_INVALID

#endif




#endif /*__BOARD_CONFIG_H__*/
