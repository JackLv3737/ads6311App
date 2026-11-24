/**
 * @file bsp_spi.c
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
#include "bsp_basic.h"
#include "bsp_spi.h"
#include "common.h"
#include "sci.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(m_array) (sizeof(m_array) / sizeof(m_array[0]))
#endif

#define SPI_SPEED_FACTOR    (7 << 9)
#define SPI20_SPEED_CONFIG()    do{SCI1->SDR10 = SPI_SPEED_FACTOR;}while(0)
#define SPI21_SPEED_CONFIG()    do{SCI1->SDR11 = SPI_SPEED_FACTOR;}while(0)


#define SPI20_START_CHN SPI_CHN0
#define SPI20_END_CHN   SPI_CHN3
#define SPI21_START_CHN SPI_CHN4
#define SPI21_END_CHN   SPI_CHN7

#define SPI_DEV_CHN_NUM 4

#define IS_SPI20_CHN(m_chn) ((m_chn >= SPI20_START_CHN) && (m_chn <= SPI20_END_CHN))
#define IS_SPI21_CHN(m_chn) ((m_chn >= SPI21_START_CHN) && (m_chn <= SPI21_END_CHN))

#define CS_PORT_SETTING(m_chn) do{ \
    *((m_chn)->port_reg) |=  ((m_chn)->pin_mask);             /* xx output high level */ \
    *((m_chn)->port_mode_reg)  &= ~((m_chn)->pin_mask);     /* xx is used as output */ \
}while(0)

#define CS_ACTIVE(m_chn) do{\
    *((m_chn)->port_reg) &=  ~((m_chn)->pin_mask);\
}while(0)

#define CS_INACTIVE(m_chn) do{\
    *((m_chn)->port_reg) |=  ((m_chn)->pin_mask);\
}while(0)


#define SPI_CHN_DEFINE(m_port, m_pin) \
    {(uint8_t *)(&(PORT->PM0) + m_port), (uint8_t *)(&(PORT->P0) + m_port), (1 << m_pin)}

#define SPI_CHN_NOUSED()    {NULL, NULL, 0}

#define SPI_CHN_IS_NOUSED(m_chn)    ((m_chn)->port_mode_reg == NULL)

typedef struct spi_channel_s
{
    volatile uint8_t *port_mode_reg; 
    volatile uint8_t *port_reg;
    uint8_t pin_mask;
}spi_channel_t;


static spi_channel_t s_bsp_spi_chn_list[SPI_CHN_COUNT] = {
/**
 * SPI20: BSP_SPI_CHN0 ~ BSP_SPI_CHN3, support 4 cs
 */
#if SPI_CHN0_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN0_CS_PORT, SPI_CHN0_CS_PIN),
#endif
#if SPI_CHN1_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN1_CS_PORT, SPI_CHN1_CS_PIN),
#endif
#if SPI_CHN2_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN2_CS_PORT, SPI_CHN2_CS_PIN),
#endif
#if SPI_CHN3_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN3_CS_PORT, SPI_CHN3_CS_PIN),
#endif
/**
 * SPI21: BSP_SPI_CHN4 ~ BSP_SPI_CHN7, support 4 cs
 */
#if SPI_CHN4_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN4_CS_PORT, SPI_CHN4_CS_PIN),
#endif
#if SPI_CHN5_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN5_CS_PORT, SPI_CHN5_CS_PIN),
#endif
#if SPI_CHN6_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN6_CS_PORT, SPI_CHN6_CS_PIN),
#endif
#if SPI_CHN7_CS_PORT == GPIO_PORT_INVALID
    SPI_CHN_NOUSED(),
#else
    SPI_CHN_DEFINE(SPI_CHN7_CS_PORT, SPI_CHN7_CS_PIN),
#endif
};

static uint8_t s_spi20_lock_flag = 0;
static uint8_t s_spi20_keep_active_flag = 0;
static uint8_t s_spi21_lock_flag = 0;
static uint8_t s_spi21_keep_active_flag = 0;

static void bsp_spi20_init()
{
    int i;
    spi_channel_t *chn_info;
    CGC->PER0 |= CGC_PER0_SCI1EN_Msk;
    SCI1->ST1 |= _0001_SCI_CH0_STOP_TRG_ON;
    //SPS0 = (0 << SCI0_SPS0_PRS00_Pos) | (3 << SCI0_SPS0_PRS00_Pos);
    SCI1->SIR10 = _0004_SCI_SIRMN_FECTMN | _0002_SCI_SIRMN_PECTMN | _0001_SCI_SIRMN_OVCTMN;
    SCI1->SMR10 = _0020_SMRMN_DEFAULT_VALUE | _8000_SCI_CLOCK_SELECT_CK01 | _0000_SCI_CLOCK_MODE_CKS |
                  _0000_SCI_TRIGGER_SOFTWARE | _0000_SCI_MODE_SPI | _0000_SCI_TRANSFER_END;
    SCI1->SCR10 = _0004_SCRMN_DEFAULT_VALUE | _C000_SCI_RECEPTION_TRANSMISSION | SPI_MODE_0 | _0000_SCI_INTSRE_MASK |
                  _0000_SCI_PARITY_NONE | _0000_SCI_MSB | _0000_SCI_STOP_NONE | _0007_SCI_LENGTH_8;
	//SCI1->SDR10 = 0 << 9; 	// fMCK/(1+n)x2 = fMCK/2
    //SCI1->SDR10 = 1 << 9; 	// fMCK/(1+n)x2 = fMCK/4
    //SCI1->SDR10 = 2 << 9;		// fMCK/(1+n)x2 = fMCK/6
    //SCI1->SDR10 = 3 << 9;		// fMCK/(1+n)x2 = fMCK/8
	//SCI1->SDR10 = 7 << 9;		// fMCK/(1+n)x2 = fMCK/16

    // SCI1->SDR10 = _CE00_SCI_BAUDRATE_DIVISOR;

    //SCI1->SDR10 = 7 << 9;
    SPI20_SPEED_CONFIG();
    /* Set output enable */

    SCI1->SO1 &= ~_0100_SCI_CH0_CLOCK_OUTPUT_1;


    SCI1->SOE1 |= _0001_SCI_CH0_OUTPUT_ENABLE;
    /* Set SSn pin */
    for(i = SPI20_START_CHN; i <= SPI20_END_CHN; i++){
        chn_info = &s_bsp_spi_chn_list[i];
        if(!SPI_CHN_IS_NOUSED(chn_info)){
            CS_PORT_SETTING(chn_info);
        }
    }
    /* Set SCLK20 pin */
    SCLKO20_PORT_SETTING();
    /* Set SDI20 pin */
    SDI20_PORT_SETTING();
    /* Set SDO20 pin */
    SDO20_PORT_SETTING();
}

void bsp_spi20_start(void)
{
    /* SSn Active */
    SCI1->SOE1 |= _0001_SCI_CH0_OUTPUT_ENABLE;
    SCI1->SS1 |= _0001_SCI_CH0_START_TRG_ON;
    /* clear INTSPI20 interrupt flag */
    INTC_ClearPendingIRQ(SPI20_IRQn);
    NVIC_ClearPendingIRQ(SPI20_IRQn);
    /* enable INTSPI20 interrupt */
    INTC_EnableIRQ(SPI20_IRQn);
}

void bsp_spi20_stop(void)
{
    /* disable INTSPI20 interrupt */
    INTC_DisableIRQ(SPI20_IRQn);
    SCI1->ST1 |= _0001_SCI_CH0_STOP_TRG_ON;
    SCI1->SOE1 &= (uint16_t)~_0001_SCI_CH0_OUTPUT_ENABLE;
    /* clear INTSPI20 interrupt flag */
    INTC_ClearPendingIRQ(SPI20_IRQn);
}

static void bsp_spi21_init()
{
    int i;
    spi_channel_t *chn_info;
    CGC->PER0 |= CGC_PER0_SCI1EN_Msk;
    SCI1->ST1 |= _0002_SCI_CH1_STOP_TRG_ON;
    //SPS0 = (0 << SCI0_SPS0_PRS00_Pos) | (3 << SCI0_SPS0_PRS00_Pos);
    SCI1->SIR11 = _0004_SCI_SIRMN_FECTMN | _0002_SCI_SIRMN_PECTMN | _0001_SCI_SIRMN_OVCTMN;
    SCI1->SMR11 = _0020_SMRMN_DEFAULT_VALUE | _8000_SCI_CLOCK_SELECT_CK01 | _0000_SCI_CLOCK_MODE_CKS |
                  _0000_SCI_TRIGGER_SOFTWARE | _0000_SCI_MODE_SPI | _0000_SCI_TRANSFER_END;
    SCI1->SCR11 = _0004_SCRMN_DEFAULT_VALUE | _C000_SCI_RECEPTION_TRANSMISSION | SPI_MODE_0 | _0000_SCI_INTSRE_MASK |
                  _0000_SCI_PARITY_NONE | _0000_SCI_MSB | _0000_SCI_STOP_NONE | _0007_SCI_LENGTH_8;
    // SCI1->SDR11 = _CE00_SCI_BAUDRATE_DIVISOR;
    //SCI1->SDR11 = 7 << 9;
    SPI21_SPEED_CONFIG();
    /* Set output enable */
    SCI1->SO1 &= ~_0200_SCI_CH1_CLOCK_OUTPUT_1;

    SCI1->SOE1 |= _0002_SCI_CH1_OUTPUT_ENABLE;
    /* Set SSn pin */
    for(i = SPI21_START_CHN; i <= SPI21_END_CHN; i++){
        chn_info = &s_bsp_spi_chn_list[i];
        if(!SPI_CHN_IS_NOUSED(chn_info)){
            CS_PORT_SETTING(chn_info);
        }
    }
    /* Set SCLK21 pin */
    SCLKO21_PORT_SETTING();
    /* Set SDI21 pin */
    SDI21_PORT_SETTING();
    /* Set SDO21 pin */
    SDO21_PORT_SETTING();
}

//void SPI21_Start(void) removing cs ctrl
void bsp_spi21_start(void)
{
    SCI1->SOE1 |= _0002_SCI_CH1_OUTPUT_ENABLE;
    SCI1->SS1 |= _0002_SCI_CH1_START_TRG_ON;
    /* clear INTSPI21 interrupt flag */
    INTC_ClearPendingIRQ(SPI21_IRQn);
    NVIC_ClearPendingIRQ(SPI21_IRQn);
    /* enable INTSPI21 interrupt */
    INTC_EnableIRQ(SPI21_IRQn);
}

//void SPI21_Stop(void) removing cs ctrl
void bsp_spi21_stop(void)
{
    /* disable INTSPI21 interrupt */
    INTC_DisableIRQ(SPI21_IRQn);
    SCI1->ST1 |= _0002_SCI_CH1_STOP_TRG_ON;
    SCI1->SOE1 &= (uint16_t)~_0002_SCI_CH1_OUTPUT_ENABLE;
    /* clear INTSPI20 interrupt flag */
    INTC_ClearPendingIRQ(SPI21_IRQn);
}

static void bsp_spi20_send(uint8_t *tx_data, uint16_t size)
{
    int i;
    volatile uint8_t dummy;
    bsp_spi20_start();
    for(i = 0; i < size; i++){
        SCI1->SIO20 = tx_data[i];
        bsp_delay_us(1);
        dummy = SCI1->SIO20;
    }
    bsp_spi20_stop();
}

static void bsp_spi20_recv(uint8_t *rx_data, uint16_t size)
{
    int i;
    bsp_spi20_start();
    for(i = 0; i < size; i++){
        SCI1->SIO20 = 0xFF;
        bsp_delay_us(1);
        rx_data[i] = SCI1->SIO20;
    }
    bsp_spi20_stop();
}

static void bsp_spi20_send_and_recv(uint8_t *tx_data, uint8_t *rx_data, uint16_t size)
{
    int i;
    bsp_spi20_start();
    for(i = 0; i < size; i++){
        SCI1->SIO20 = tx_data[i];
        bsp_delay_us(1);
        rx_data[i] = SCI1->SIO20;
    }
    bsp_spi20_stop();
}

static void bsp_spi21_send(uint8_t *tx_data, uint16_t size)
{
    int i;
    volatile uint8_t dummy;
    bsp_spi21_start();
    for(i = 0; i < size; i++){
        SCI1->SIO21 = tx_data[i];
        bsp_delay_us(1);
        dummy = SCI1->SIO21;
    }
    bsp_spi21_stop();
}

static void bsp_spi21_recv(uint8_t *rx_data, uint16_t size)
{
    int i;
    bsp_spi21_start();
    for(i = 0; i < size; i++){
        SCI1->SIO21 = 0xFF;
        bsp_delay_us(1);
        rx_data[i] = SCI1->SIO21;
    }
    bsp_spi21_stop();
}

static void bsp_spi21_send_and_recv(uint8_t *tx_data, uint8_t *rx_data, uint16_t size)
{
    int i;
    bsp_spi21_start();
    for(i = 0; i < size; i++){
        SCI1->SIO21 = tx_data[i];
        bsp_delay_us(1);
        rx_data[i] = SCI1->SIO21;
    }
    bsp_spi21_stop();
}


/**
 * @brief bsp spi init
 * 
 * @return int 
 */
int bsp_spi_init(void)
{
    int i;
    for(i = SPI20_START_CHN; i <= SPI20_END_CHN; i++){
        if(!SPI_CHN_IS_NOUSED(&s_bsp_spi_chn_list[i])){
            bsp_spi20_init();
            break;
        }
    }

    for(i = SPI21_START_CHN; i <= SPI21_END_CHN; i++){
        if(!SPI_CHN_IS_NOUSED(&s_bsp_spi_chn_list[i])){
            bsp_spi21_init();
            break;
        }
    }
    //bsp_spi_test();
    return 0;
}
    
int bsp_spi_write_data(uint8_t channel, uint8_t *pdata, uint16_t size)
{
    uint8_t chn_mask;
    spi_channel_t *chn_info;
    if(IS_SPI20_CHN(channel)){
        chn_mask = 1 << (channel - SPI20_START_CHN);
        if(s_spi20_lock_flag & (~chn_mask)){
            return -1;
        }
        chn_info = &s_bsp_spi_chn_list[channel];
        if(SPI_CHN_IS_NOUSED(chn_info)){
            return -1;
        }
        CS_ACTIVE(chn_info);
        bsp_spi20_send(pdata, size);
        if(!(s_spi20_keep_active_flag & chn_mask)){
            CS_INACTIVE(chn_info);
        }
    }else if(IS_SPI21_CHN(channel)){
        chn_mask = 1 << (channel - SPI21_START_CHN);
        if(s_spi21_lock_flag & (~chn_mask)){
            return -1;
        }
        chn_info = &s_bsp_spi_chn_list[channel];
        if(SPI_CHN_IS_NOUSED(chn_info)){
            return -1;
        }
        CS_ACTIVE(chn_info);
        bsp_spi21_send(pdata, size);
        if(!(s_spi21_keep_active_flag & chn_mask)){
            CS_INACTIVE(chn_info);
        }
    }else{
        return -1;
    }
    return 0;
}

int bsp_spi_read_data(uint8_t channel, uint8_t *pdata, uint16_t size)
{
    uint8_t chn_mask;
    spi_channel_t *chn_info;
    if(IS_SPI20_CHN(channel)){
        chn_mask = 1 << (channel - SPI20_START_CHN);
        if(s_spi20_lock_flag & (~chn_mask)){
            return -1;
        }
        chn_info = &s_bsp_spi_chn_list[channel];
        if(SPI_CHN_IS_NOUSED(chn_info)){
            return -1;
        }
        CS_ACTIVE(chn_info);
        bsp_spi20_recv(pdata, size);
        if(!(s_spi20_keep_active_flag & chn_mask)){
            CS_INACTIVE(chn_info);
        }
    }else if(IS_SPI21_CHN(channel)){
        chn_mask = 1 << (channel - SPI21_START_CHN);
        if(s_spi21_lock_flag & (~chn_mask)){
            return -1;
        }
        chn_info = &s_bsp_spi_chn_list[channel];
        if(SPI_CHN_IS_NOUSED(chn_info)){
            return -1;
        }
        CS_ACTIVE(chn_info);
        bsp_spi21_recv(pdata, size);
        if(!(s_spi21_keep_active_flag & chn_mask)){
            CS_INACTIVE(chn_info);
        }
    }else{
        return -1;
    }
    return 0;
}

int bsp_spi_write_and_read_data(uint8_t channel, uint8_t *pdata_w, uint8_t *pdata_r, uint16_t size)
{
    uint8_t chn_mask;
    spi_channel_t *chn_info;
    if(IS_SPI20_CHN(channel)){
        chn_mask = 1 << (channel - SPI20_START_CHN);
        if(s_spi20_lock_flag & (~chn_mask)){
            return -1;
        }
        chn_info = &s_bsp_spi_chn_list[channel];
        if(SPI_CHN_IS_NOUSED(chn_info)){
            return -1;
        }
        CS_ACTIVE(chn_info);
        bsp_spi20_send_and_recv(pdata_w, pdata_r, size);
        if(!(s_spi20_keep_active_flag & chn_mask)){
            CS_INACTIVE(chn_info);
        }
    }else if(IS_SPI21_CHN(channel)){
        chn_mask = 1 << (channel - SPI21_START_CHN);
        if(s_spi21_lock_flag & (~chn_mask)){
            return -1;
        }
        chn_info = &s_bsp_spi_chn_list[channel];
        if(SPI_CHN_IS_NOUSED(chn_info)){
            return -1;
        }
        CS_ACTIVE(chn_info);
        bsp_spi21_send_and_recv(pdata_w, pdata_r, size);
        if(!(s_spi21_keep_active_flag & chn_mask)){
            CS_INACTIVE(chn_info);
        }
    }else{
        return -1;
    }
    return 0;
}

int bsp_spi_lock(uint8_t channel, uint8_t cs_keep_active_flag)
{
    uint8_t mask;
    if(IS_SPI20_CHN(channel)){
        mask = 1 << (channel - SPI20_START_CHN);
        s_spi20_lock_flag |= mask;
        if(cs_keep_active_flag){
            s_spi20_keep_active_flag |= mask;
        }else{
            s_spi20_keep_active_flag &= ~mask;
            if(!SPI_CHN_IS_NOUSED(&s_bsp_spi_chn_list[channel])){
                CS_INACTIVE(&s_bsp_spi_chn_list[channel]);
            }
        }

    }else if(IS_SPI21_CHN(channel)){
        mask = 1 << (channel - SPI21_START_CHN);
        s_spi21_lock_flag |= mask;
        if(cs_keep_active_flag){
            s_spi21_keep_active_flag |= mask;
        }else{
            s_spi21_keep_active_flag &= ~mask;
            if(!SPI_CHN_IS_NOUSED(&s_bsp_spi_chn_list[channel])){
                CS_INACTIVE(&s_bsp_spi_chn_list[channel]);
            }
        }
    }
    return 0;
}

int bsp_spi_unlock(uint8_t channel)
{
    uint8_t mask;
    if(IS_SPI20_CHN(channel)){
        mask = 1 << (channel - SPI20_START_CHN);
        s_spi20_lock_flag &= ~mask;
        if(s_spi20_keep_active_flag & mask){
            if(!SPI_CHN_IS_NOUSED(&s_bsp_spi_chn_list[channel])){
                CS_INACTIVE(&s_bsp_spi_chn_list[channel]);
            }
            s_spi20_keep_active_flag &= ~mask;
        }
    }else if(IS_SPI21_CHN(channel)){
        mask = 1 << (channel - SPI21_START_CHN);
        s_spi21_lock_flag &= ~mask;
        if(s_spi21_keep_active_flag & mask){
            if(!SPI_CHN_IS_NOUSED(&s_bsp_spi_chn_list[channel])){
                CS_INACTIVE(&s_bsp_spi_chn_list[channel]);
            }
            s_spi21_keep_active_flag &= ~mask;
        }
    }
    return 0;
}

void bsp_spi_test(void)
{
    #define TEST_TELM_READ  
    #define TX_CHECKSUM(m_buf, m_offset) ((m_buf[m_offset] ^ m_buf[m_offset + 1] ^ m_buf[m_offset + 2] ^ m_buf[m_offset + 3] ^ 0x0F))
    #define RX_CHECKSUM(m_buf, m_offset) ((uint8_t)(m_buf[m_offset] ^ m_buf[m_offset + 1] ^ 0x0F))
    #define RX_SIZE 4
    int i;
    uint8_t tx_data[] = {0x80, 0x01, 0x56, 0x78, 0x90};
    static uint8_t rx_data_20[ARRAY_SIZE(tx_data)];
    static uint8_t rx_data_21[ARRAY_SIZE(tx_data)];
    printf("\n%s ++++\n", __FUNCTION__);
    tx_data[4] = TX_CHECKSUM(tx_data, 0);
    bsp_spi20_send_and_recv(tx_data, rx_data_20, ARRAY_SIZE(rx_data_20));
#ifdef TEST_TELM_READ
    bsp_spi20_send_and_recv(tx_data, rx_data_20, RX_SIZE);
    printf("spi20 rx data:");
    for(i = 0; i < RX_SIZE; i++){
        printf("0x%x ", rx_data_20[i]);
    }
    printf("\n");
    if(rx_data_20[3] == RX_CHECKSUM(rx_data_20, 1)){
        printf("spi20 rx checksum ok\n");
    }else{
        printf("spi20 rx checksum fail\n");
    }
#endif
    tx_data[4] = TX_CHECKSUM(tx_data, 0);
    bsp_spi21_send_and_recv(tx_data, rx_data_21, ARRAY_SIZE(rx_data_21));
#ifdef TEST_TELM_READ
    bsp_spi21_send_and_recv(tx_data, rx_data_21, RX_SIZE);
    printf("spi21 rx data:");
    for(i = 0; i < RX_SIZE; i++){
        printf("0x%x ", rx_data_21[i]);
    }
    printf("\n");
    if(rx_data_21[3] == RX_CHECKSUM(rx_data_21, 1)){
        printf("spi21 rx checksum ok\n");
    }else{
        printf("spi21 rx checksum fail\n");
    }
    printf("\n");
#endif
    printf("\n%s ----\n", __FUNCTION__);
    return 0;
}
