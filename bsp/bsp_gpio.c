/**
 * @file bsp_gpio.c
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
#include "board_config.h"
#include "bsp_basic.h"
#include "bsp_gpio.h"
#include "gpio.h"
#include "intp.h"

#define BSP_GPIO_INTP_NUM   12

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(m_array) (sizeof(m_array) / sizeof(m_array[0]))
#endif

#define TO_BIT(m_offset)  (1 << m_offset)

#define INA_RST_NAME "InaRst"
#define INA_CMP_NAME "InaCmp"
#define PVDD_SW1_NAME "PvddSw1"
#define PVDD_SW2_NAME "PvddSw2"

typedef struct bspgpio_in_cfg_s
{
    gpio_port_t port;
    uint16_t pin;
    char *name;
}bspgpio_in_cfg_t;

typedef struct bspgpio_out_cfg_s
{
    gpio_port_t port;
    uint16_t pin;
    uint8_t default_level; //0=low, 1=high
    char *name;
}bspgpio_out_cfg_t;

typedef struct bspgpio_interrupt_cfg_s
{
    gpio_port_t port;
    uint16_t pin;
    uint16_t intp_index;
    char *name;
}bspgpio_interrupt_cfg_t;

#define GPIO_IN_CFG(m_port, m_pin, m_name)  {m_port, m_pin, m_name}
#define GPIO_OUT_CFG(m_port, m_pin, m_default_level, m_name)  {m_port, m_pin, m_default_level, m_name}
#define GPIO_INTERRUPT_CFG(m_port, m_pin, m_intp_x, m_name) {m_port, m_pin, m_intp_x, m_name}

static bsp_gpio_evt_cb_t s_intp_evt_cb_tab[BSP_GPIO_INTP_NUM] = {0};
static int s_intp_cnt[BSP_GPIO_INTP_NUM] = {0};


static const bspgpio_in_cfg_t s_in_cfg_tab[] = {
    GPIO_IN_CFG(ARRAY_VROLL_PORT_0, ARRAY_VROLL_0, "vroll0"),
    GPIO_IN_CFG(ARRAY_VROLL_PORT_1, ARRAY_VROLL_1, "vroll1"),
    GPIO_IN_CFG(ARRAY_VROLL_PORT_2, ARRAY_VROLL_2, "vroll2"),
    GPIO_IN_CFG(ARRAY_VROLL_PORT_3, ARRAY_VROLL_3, "vroll3"),
    GPIO_IN_CFG(ARRAY_VROLL_PORT_4, ARRAY_VROLL_4, "vroll4"),
    GPIO_IN_CFG(ARRAY_HROLL_PORT_0, ARRAY_HROLL_0, "hroll0"),
    GPIO_IN_CFG(ARRAY_HROLL_PORT_1, ARRAY_HROLL_1, "hroll1"),
    GPIO_IN_CFG(ARRAY_HROLL_PORT_2, ARRAY_HROLL_2, "hroll2"),
    GPIO_IN_CFG(ARRAY_HROLL_PORT_3, ARRAY_HROLL_3, "hroll3")
};

static const bspgpio_out_cfg_t s_out_cfg_tab[] = {
    GPIO_OUT_CFG(AV33_SV33_EN_PORT, AV33_SV33_EN_PIN, 0, "AvSv33En"),
    GPIO_OUT_CFG(VCC_3V3_EN_PORT, VCC_3V3_EN_PIN, 0, "Vcc33En"),
    GPIO_OUT_CFG(VCC_DRV_EN_PORT, VCC_DRV_EN_PIN, 0, "VccDrvEn"),
    GPIO_OUT_CFG(IOV18_EN_PORT, IOV18_EN_PIN, 0, "IOV18En"),
    GPIO_OUT_CFG(SV11_DV11_EN_PORT, SV11_DV11_EN_PIN, 0, "SvDv33En"),
    GPIO_OUT_CFG(AV11_EN_PORT, AV11_EN_PIN, 0, "AV11En"),
    GPIO_OUT_CFG(VOP_EN_PORT, VOP_EN_PIN, 0, "VopEn"),
    GPIO_OUT_CFG(PVDD_EN_PORT, PVDD_EN_PIN, 0, "PvddEn"),
    GPIO_OUT_CFG(PVDDL_EN_PORT, PVDDL_EN_PIN, 0, "PvddLowEn"),
    GPIO_OUT_CFG(PVDD_SW1_PORT, PVDD_SW1_PIN, 0, PVDD_SW1_NAME),
    GPIO_OUT_CFG(PVDD_SW2_PORT, PVDD_SW2_PIN, 0, PVDD_SW2_NAME),
    GPIO_OUT_CFG(INA_RST_PORT, INA_RST_PIN, 0, INA_RST_NAME),
    GPIO_OUT_CFG(RK_ERR_PORT, RK_ERR_PIN, 0, "RkErr"),
    GPIO_OUT_CFG(RX_SWTACK_PORT, RX_SWTACK_PIN, 0, "RxAck"),
    GPIO_OUT_CFG(RX_DRV_ERR_PORT, RX_DRV_ERR_PIN, 1, "TxDrvErr"),
    GPIO_OUT_CFG(S1_S2_EN_PORT, S1_S2_EN_PIN, 0, "S1S2En"),
    GPIO_OUT_CFG(TX_TRIG_EN_PORT, TX_TRIG_EN_PIN, 0, "TrigEn"),
    GPIO_OUT_CFG(BMI_EN_PORT, BMI_EN_PIN, 1, "BmiEn"),
    GPIO_OUT_CFG(ADS6311_I2C_EN_PORT, ADS6311_I2C_EN_PIN, 1, "Ads6311I2cEn"),
    GPIO_OUT_CFG(ADS6311_EN_PORT, ADS6311_EN_PIN, 1, "Ads6311En"),
    GPIO_OUT_CFG(LED2_RUN_PORT, LED2_RUN_PIN, 0, "Led2")
};

static const bspgpio_interrupt_cfg_t s_interrupt_cfg_tab[] = {
    GPIO_INTERRUPT_CFG(INA_CMP_PORT, INA_CMP_PIN, INA_CMP_INTPX, INA_CMP_NAME),
#ifdef TX_REQ_USING_SSYNC
    GPIO_INTERRUPT_CFG(RX_SSYNC_PORT, RX_SSYNC_PIN, RX_SSYNC_INTPX, "SSync"),
#else
    GPIO_INTERRUPT_CFG(RX_SWTREQ_PORT, RX_SWTREQ_PIN, RX_SWTREQ_INTPX, "SwReq"),
#endif
    GPIO_INTERRUPT_CFG(RX_FSYNC_PORT, RX_FSYNC_PIN, RX_FSYNC_INTPX, "FSync"),
    GPIO_INTERRUPT_CFG(S1_ERR_PORT, S1_ERR_PIN, S1_ERR_INTPX, "Shark1Err"),
    GPIO_INTERRUPT_CFG(S2_ERR_PORT, S2_ERR_PIN, S2_ERR_INTPX, "Shark2Err"),
    GPIO_INTERRUPT_CFG(BMI_INT1_PORT, BMI_INT1_PIN, BMI_INT1_INTPX, "BmiInt1"),
    GPIO_INTERRUPT_CFG(BMI_INT2_PORT, BMI_INT2_PIN, BMI_INT2_INTPX, "BmiInt2")
};

#ifdef INTP0_ENABLE
void intp0_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP0_IRQn);
    if(s_intp_evt_cb_tab[0]) s_intp_evt_cb_tab[0]();
    s_intp_cnt[0]++;
}
#endif

#ifdef INTP1_ENABLE
void intp1_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP1_IRQn);
    if(s_intp_evt_cb_tab[1]) s_intp_evt_cb_tab[1]();
    s_intp_cnt[1]++;
}
#endif

#ifdef INTP2_ENABLE
void intp2_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP2_IRQn);
    if(s_intp_evt_cb_tab[2]) s_intp_evt_cb_tab[2]();
    s_intp_cnt[2]++;
}
#endif

#ifdef INTP3_ENABLE
void intp3_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP3_IRQn);
    if(s_intp_evt_cb_tab[3]) s_intp_evt_cb_tab[3]();
    s_intp_cnt[3]++;
}
#endif

#ifdef INTP4_ENABLE
void intp4_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP4_IRQn);
    if(s_intp_evt_cb_tab[4]) s_intp_evt_cb_tab[4]();
    s_intp_cnt[4]++;
}
#endif

#ifdef INTP5_ENABLE
void intp5_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP5_IRQn);
    if(s_intp_evt_cb_tab[5]) s_intp_evt_cb_tab[5]();
    s_intp_cnt[5]++;
}
#endif

#ifdef INTP6_ENABLE
void intp6_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP6_IRQn);
    if(s_intp_evt_cb_tab[6]) s_intp_evt_cb_tab[6]();
    s_intp_cnt[6]++;
}
#endif

#ifdef INTP7_ENABLE
void intp7_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP7_IRQn);
    if(s_intp_evt_cb_tab[7]) s_intp_evt_cb_tab[7]();
    s_intp_cnt[7]++;
}
#endif

#ifdef INTP8_ENABLE
void intp8_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP8_IRQn);
    if(s_intp_evt_cb_tab[8]) s_intp_evt_cb_tab[8]();
    s_intp_cnt[8]++;
}
#endif

#ifdef INTP9_ENABLE
void intp9_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP9_IRQn);
    if(s_intp_evt_cb_tab[9]) s_intp_evt_cb_tab[9]();
    s_intp_cnt[9]++;
}
#endif

#ifdef INTP10_ENABLE
void intp10_interrupt(void)
{
	INTC_ClearPendingIRQ(INTP10_IRQn);
    if(s_intp_evt_cb_tab[10]) s_intp_evt_cb_tab[10]();
    s_intp_cnt[10]++;
}
#endif


/**
 * @brief bsp gpio init
 * 
 * @return int 
 */
int bsp_gpio_init(void)
{
    int i;
    bspgpio_interrupt_cfg_t *p_interrupt;
    bspgpio_out_cfg_t *p_out_cfg;

    //init input port
    for(i = 0; i < ARRAY_SIZE(s_in_cfg_tab); i++){
        if((s_in_cfg_tab[i].port != GPIO_PORT_INVALID) && (s_in_cfg_tab[i].pin != GPIO_PIN_INVALID)){
            PORT_Init((PORT_TypeDef)s_in_cfg_tab[i].port, s_in_cfg_tab[i].pin, INPUT);
        }
    }

    //init output port
    for(i = 0; i < ARRAY_SIZE(s_out_cfg_tab); i++){
        p_out_cfg = &s_out_cfg_tab[i];
        if((p_out_cfg->port != GPIO_PORT_INVALID) && (p_out_cfg->pin != GPIO_PIN_INVALID)){
#ifdef BOARD_DEMO4
            switch(bsp_hw_variant_ver()){
                //demo4 1V0版本没有PVDD切换功能
                case HW_VARIANT_VER0:
                    if((strcmp(p_out_cfg->name, PVDD_SW1_NAME) == 0)
                    || (strcmp(p_out_cfg->name, PVDD_SW2_NAME) == 0)){
                        continue;
                    }
                break;
                //demo4 1V1版本去掉了PVDD电流检测功能
                case HW_VARIANT_VER1:
                default:
                    if((strcmp(p_out_cfg->name, INA_RST_NAME) == 0)){
                        continue;
                    }
                break;
            }
#endif
            if(p_out_cfg->default_level){
                PORT_SetBit((PORT_TypeDef)p_out_cfg->port, p_out_cfg->pin);
            }else{
                PORT_ClrBit((PORT_TypeDef)p_out_cfg->port, p_out_cfg->pin);
            }
            PORT_Init((PORT_TypeDef)p_out_cfg->port, p_out_cfg->pin, OUTPUT);
        }
    }

    if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
        gpio_write(INA_RST_PORT, INA_RST_PIN, GPIO_PIN_SET);
    }


    //init interrupt port
    for(i = 0; i < ARRAY_SIZE(s_interrupt_cfg_tab); i++){
        p_interrupt = &s_interrupt_cfg_tab[i];
        if((p_interrupt->port == GPIO_PORT_INVALID) 
        || (p_interrupt->pin == GPIO_PIN_INVALID)){
            continue;
        }
        if((p_interrupt->port == INA_CMP_PORT) && (p_interrupt->pin == INA_CMP_PIN)){
            //demo4 1V1版本去掉了PVDD电流检测功能
            if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
                INA_CMP_INTP_PORT_SETTING();
                INTP_Init(TO_BIT(INA_CMP_INTPX), INTP_RISING);
                INTP_Start(TO_BIT(INA_CMP_INTPX));
            }
        }else if((p_interrupt->port == RX_SSYNC_PORT) && (p_interrupt->pin == RX_SSYNC_PIN)){
            RX_SSYNC_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(RX_SSYNC_INTPX), INTP_RISING);
            INTP_Start(TO_BIT(RX_SSYNC_INTPX));
            NVIC_SetPriority(RX_SSYNC_IRQ, IRQ_PRIOR_RX_REQ);
        }else if((p_interrupt->port == RX_FSYNC_PORT) && (p_interrupt->pin == RX_FSYNC_PIN)){
            RX_FSYNC_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(RX_FSYNC_INTPX), INTP_FALLING);
            INTP_Start(TO_BIT(RX_FSYNC_INTPX));
            NVIC_SetPriority(RX_SSYNC_IRQ, IRQ_PRIOR_PVDD_SWITCH);
        }else if((p_interrupt->port == RX_SWTREQ_PORT) && (p_interrupt->pin == RX_SWTREQ_PIN)){
            RX_SWTREQ_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(RX_SWTREQ_INTPX), INTP_RISING);
            INTP_Start(TO_BIT(RX_SWTREQ_INTPX));
            //printf("NVIC_GetPriority(INTP1_IRQn) %d\n", NVIC_GetPriority(INTP1_IRQn));
            NVIC_SetPriority(RX_SWTREQ_IRQ, IRQ_PRIOR_RX_REQ);
        }else if((p_interrupt->port == S1_ERR_PORT) && (p_interrupt->pin == S1_ERR_PIN)){
            S1_ERR_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(S1_ERR_INTPX), INTP_RISING);
            INTP_Start(TO_BIT(S1_ERR_INTPX));
        }else if((p_interrupt->port == S2_ERR_PORT) && (p_interrupt->pin == S2_ERR_PIN)){
            S2_ERR_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(S2_ERR_INTPX), INTP_RISING);
            INTP_Start(TO_BIT(S2_ERR_INTPX));
        }else if((p_interrupt->port == BMI_INT1_PORT) && (p_interrupt->pin == BMI_INT1_PIN)){
            BMI_INT1_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(BMI_INT1_INTPX), INTP_RISING);
            INTP_Start(TO_BIT(BMI_INT1_INTPX));
        }else if((p_interrupt->port == BMI_INT2_PORT) && (p_interrupt->pin == BMI_INT2_PIN)){
            BMI_INT2_INTP_PORT_SETTING();
            INTP_Init(TO_BIT(BMI_INT2_INTPX), INTP_RISING);
            INTP_Start(TO_BIT(BMI_INT2_INTPX));
        }

    }

    return 0;
}

int bsp_gpio_read(gpio_port_t port, uint8_t pin)
{
    if((port == GPIO_PORT_INVALID) || (pin == GPIO_PIN_INVALID)){
        return -1;
    }
    return PORT_GetBit((PORT_TypeDef)port, pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

int bsp_gpio_write(gpio_port_t port, uint8_t pin, uint8_t value)
{
    if((port == GPIO_PORT_INVALID) || (pin == GPIO_PIN_INVALID)){
        return -1;
    }
    if(value)
        PORT_SetBit((PORT_TypeDef)port, pin);
    else 
        PORT_ClrBit((PORT_TypeDef)port, pin);
    return 0;
}

int bsp_gpio_toggle(gpio_port_t port, uint8_t pin)
{
    if((port == GPIO_PORT_INVALID) || (pin == GPIO_PIN_INVALID)){
        return -1;
    }
    PORT_ToggleBit((PORT_TypeDef)port, pin);
    return 0;
}

int bsp_gpio_register_event_callback(gpio_port_t port, uint16_t pin, bsp_gpio_evt_cb_t cb, uint8_t event_flag)
{
    int i;
    if((port == GPIO_PORT_INVALID) || (pin == GPIO_PIN_INVALID)){
        return -1;
    }

    for(i = 0; i < ARRAY_SIZE(s_interrupt_cfg_tab); i++){
        if((port == s_interrupt_cfg_tab[i].port) 
        && (pin == s_interrupt_cfg_tab[i].pin)){
            s_intp_evt_cb_tab[s_interrupt_cfg_tab[i].intp_index] = cb;
            return 0;
        }
    }

    printf("\n%s err: unkown port 0x%x or pin %u\n", __FUNCTION__, port, pin);

    return -1;
}


void bsp_gpio_test(int test_write)
{
    static char gpio_out_flag = 0;
    const bspgpio_out_cfg_t *p_out;
    const bspgpio_in_cfg_t *p_in;
    bspgpio_interrupt_cfg_t *p_intr;
    int i;
    printf("\n%s ++++\n", __FUNCTION__);

    if(test_write){
        for(i = 0; i < ARRAY_SIZE(s_out_cfg_tab); i++){
            p_out = &s_out_cfg_tab[i];
            if((p_out->port == GPIO_PORT_INVALID) || (p_out->pin == GPIO_PIN_INVALID)){
                continue;
            }
            if((i % 2) == 0){
                bsp_gpio_write(p_out->port, p_out->pin, gpio_out_flag);
                printf("[%d]%s GPIO P%u%u output %u\n", i, p_out->name, p_out->port, p_out->pin, gpio_out_flag);
            }else{
                bsp_gpio_write(p_out->port, p_out->pin, !gpio_out_flag);
                printf("[%d]%s GPIO P%u%u output %u\n", i, p_out->name, p_out->port, p_out->pin, !gpio_out_flag);
            }
        }
        gpio_out_flag = !gpio_out_flag;
    }


    for(i = 0; i < ARRAY_SIZE(s_in_cfg_tab); i++){
        p_in = &s_in_cfg_tab[i];
        if((p_in->port == GPIO_PORT_INVALID) || (p_in->pin == GPIO_PIN_INVALID)){
            continue;
        }
        printf("[%d]%s GPIO P%u%u state %u\n", i, p_in->name, p_in->port, p_in->pin, bsp_gpio_read(p_in->port, p_in->pin));
    }

    for(i = 0; i < ARRAY_SIZE(s_interrupt_cfg_tab); i++){
        p_intr = &s_interrupt_cfg_tab[i];
        printf("%s:intp%d(P%d%d) cnt %u\n", p_intr->name, p_intr->intp_index, p_intr->port, p_intr->pin, s_intp_cnt[p_intr->intp_index]);
    }

    printf("\n%s ----\n", __FUNCTION__);
}
