/**
 * @file bsp_telematic.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "stdio.h"
#include "stddef.h"
#include "userdefine.h"
#include "BAT32A239.h"
#include "dma.h"
#include "sci.h"
#include "iica.h"
#include "board_config.h"
#include "bsp_telematic.h"
#include "bsp_basic.h"
#include "bsp_gpio.h"

#define BSP_TELM_PRESS_TEST 0

#define TELM_TICK_INVALID  0

#define TELM_MASS_ADDR  0x93

#define TELM_CHECKSUM_INIT_VALUE    0x0F

#define TELM_RX_CHECKSUM(m_buf, m_offset) ((uint8_t)(m_buf[m_offset] ^ m_buf[m_offset + 1] ^ m_buf[m_offset + 2] ^ m_buf[m_offset + 3] ^ TELM_CHECKSUM_INIT_VALUE))
#define TELM_TX_CHECKSUM(m_buf, m_offset) ((uint8_t)(m_buf[m_offset] ^ m_buf[m_offset + 1] ^ TELM_CHECKSUM_INIT_VALUE))

enum telm_state_e
{
    TELM_STATE_IDLE = 0,
    TELM_STATE_RX,
    TELM_STATE_TX,
    TELM_STATE_MASS_RX,
    TELM_STATE_MASS_TX,
    TELM_STATE_COUNT
};

static void bsp_telm_dummy_evt_callback(bsp_telm_evt_t evt)
{
}

static int bsp_telm_dummy_msg_callback(uint8_t *rx_msg, uint16_t rx_len, uint8_t *tx_msg)
{
    return 0;
}

//用空函数避免需要判断空指针
static bsp_telm_msg_cb_t s_telm_msg_cb = bsp_telm_dummy_msg_callback;
static bsp_telm_evt_cb_t s_telm_evt_cb = bsp_telm_dummy_evt_callback;
static enum telm_state_e s_telm_state = TELM_STATE_IDLE;
static uint32_t s_telm_msg_cnt = 0;

static uint32_t s_telm_tx_start_tick = 0;
static uint32_t s_telm_rx_start_tick = 0;
static uint32_t s_telm_tx_timeout_val = 0;
static uint32_t s_telm_rx_timeout_val = 0;

static const char *s_telm_state_str[TELM_STATE_COUNT] = {
    "idle",
    "rx",
    "tx",
    "mass_rx",
    "mass_tx"
};

void IRQ16_Handler(void) __attribute__((alias("telm_iica0_interrupt")));
void IRQ10_Handler(void) __attribute__((alias("telm_spi_interrupt")));

static inline uint32_t bsp_telm_get_cur_tick(void)
{
    uint32_t now_tick = bsp_tick_get();
    if(TELM_TICK_INVALID == now_tick){
        return (TELM_TICK_INVALID + 1);
    }

    return now_tick;
}

#if BSP_TELM_PRESS_TEST

#define TELM_FAKE_REG_NUM   256
static uint16_t s_telm_fake_reg[TELM_FAKE_REG_NUM];

static int bsp_telm_fake_msg_callback(uint8_t *rx_msg, uint16_t rx_len, uint8_t *tx_msg)
{
    uint16_t value;
    if(rx_msg[0] & 0x80){ //读操作
        value = s_telm_fake_reg[rx_msg[1]];
        tx_msg[0] = rx_msg[1];
        tx_msg[1] = (value >> 8) & 0xFF;
        tx_msg[2] = value & 0xFF;
        return 3;
    }else{ //写操作
        s_telm_fake_reg[rx_msg[1]] = (rx_msg[2] << 8) | rx_msg[3];
    }
    return 0;
}

static void bsp_telm_dump_fake_reg(void)
{
    uint16_t val;
    printf("\r\n");
    printf("-----");
    for (int i = 0; i < 0x10; i++) {
        printf("----%x", i);
    }
    printf("\r\n");
    for (int i = 0; i < 0x10; i++) {
        printf("%04x:", (i << 4) );
        for (int j = 0; j < 0x10; j++) {
            val = (uint16_t)s_telm_fake_reg[i*0x10+j];
            printf(" %04X",val);
        }
        printf("\r\n");
    }
    for(int i=0;i<85;i++)
        printf("=");

    printf("\r\n");
}

#endif

#ifdef HW_TELM_USING_IIC

#define TELM_ERR_REC_NUM    4

#define TELM_IIC_CHECKSUM_SIZE  1
#define TELM_IIC_ADDR_SIZE  2
#define TELM_IIC_VALUE_SIZE  2
#define TELM_IIC_TX_SIZE    (TELM_IIC_VALUE_SIZE + TELM_IIC_CHECKSUM_SIZE)
#define TELM_IIC_RX_SIZE    (TELM_IIC_ADDR_SIZE + TELM_IIC_VALUE_SIZE + TELM_IIC_CHECKSUM_SIZE)
#define TELM_IIC_TX_OFFSET  1

#define TELM_IIC_TIMEOUT_MIN_MS 50 //must more than 0 ms
#define TELM_IIC_TIMEOUT_MAX_MS 500
#define TELM_IIC_TIMEOUT_1BYTE_MS 10

enum telm_iic_state_e
{
    TELM_IIC_STATE_LISTEN = 0,
    TELM_IIC_STATE_WAIT_ADDR1, //仅用于IIC
    TELM_IIC_STATE_WAIT_ADDR2, //仅用于IIC
    TELM_IIC_STATE_DATA_RX,
    TELM_IIC_STATE_DATA_TX,
    TELM_IIC_STATE_COUNT
};

#define TELM_IIC_ADDR   (0x48 << 1)

/* You can allocate the SCLA0 to P60 or P14 with PIOR02 register */
#define TELM_SCLA0_PORT_SETTING() do{ \
        PORT->PIOR0 |=  (1 << 2);    /* allocate SCLA0 to P14 */ \
        PORT->P1    &= ~(1 << 4);    /* P14 output low level */ \
        PORT->PM1   &= ~(1 << 4);    /* P14 is used as SCLA0 output */ \
        PORT->POM1  |=  (1 << 4);    /* P14 is N-ch open-drain output mode */ \
}while(0)

/* You can allocate the SDAA0 to P61 or P15 with PIOR02 register */
#define TELM_SDAA0_PORT_SETTING() do{ \
        PORT->PIOR0 |=  (1 << 2);    /* allocate SDAA0 to P15 */ \
        PORT->P1    &= ~(1 << 5);    /* P15 output low level */ \
        PORT->PM1   &= ~(1 << 5);    /* P15 is used as SDAA0 inout */ \
        PORT->POM1  |=  (1 << 5);    /* P15 is N-ch open-drain output mode */ \
}while(0)

#define IICA0_CLEAR_WAKEUP(void)                \
    do {                                        \
        IICA0->IICCTL01 &= ~0x80; /* WUP = 0 */ \
        __NOP();                                \
        __NOP();                                \
        __NOP();                                \
        __NOP();                                \
        __NOP();                                \
    } while (0)

static enum telm_iic_state_e s_telm_iic_state = TELM_IIC_STATE_LISTEN;

static uint8_t s_telm_tx_buf[TELM_IIC_TX_OFFSET + TELM_IIC_TX_SIZE];
static uint8_t __attribute__((aligned(4))) s_telm_rx_buf[TELM_IIC_RX_SIZE + 1];
static uint8_t __attribute__((aligned(4))) s_telm_last_rx_buf[TELM_IIC_RX_SIZE];

static uint8_t *s_telm_cur_data_buf = s_telm_rx_buf;
static uint32_t s_telm_cur_data_len = 0;
static uint32_t s_telm_cur_data_count = 0;
static uint32_t s_telm_err_rec_count = 0;
static uint32_t s_telm_err_rec_list[TELM_ERR_REC_NUM];

static const char *s_telm_iic_state_str[TELM_IIC_STATE_COUNT] = {
    "listen",
    "wait_addr1",
    "wait_addr2",
    "data_rx",
    "data_tx"
};

#define IICA0_IS_READ() (IICA0->IICS0 & IICA0_IICS0_TRC_Msk)
#define IICA0_IS_WRITE() (!(IICA0->IICS0 & IICA0_IICS0_TRC_Msk))

//解除等待(即释放SCL为高阻态)。在解除等待后自动清除WREL位。
#define IICA0_RELEASE_WAIT()                        \
    do {                                            \
        IICA0->IICCTL00 |= IICA0_IICCTL00_WREL_Msk; \
    } while (0)

#define IICA_ERR_PROC()                                              \
    do {                                                            \
        telm_iica0_restart();                                       \
        if (s_telm_err_rec_count < TELM_ERR_REC_NUM) {              \
            s_telm_err_rec_list[s_telm_err_rec_count++] = __LINE__; \
        }                                                           \
    } while (0)

static void telm_iica0_trans_start(uint8_t *buf, uint16_t size, enum telm_state_e state)
{
    s_telm_state = state;
    s_telm_cur_data_buf = buf;
    s_telm_cur_data_len = size;
    s_telm_cur_data_count = 0;
    s_telm_iic_state = TELM_IIC_STATE_LISTEN;
}

static void telm_iica0_restart(void)
{
    IICA0_RELEASE_WAIT();
    telm_iica0_trans_start(s_telm_rx_buf, TELM_IIC_ADDR_SIZE, TELM_STATE_IDLE);
}

void telm_iica0_interrupt(void)
{
    int ret;
    if(INTC_GetPendingIRQ(DIV_IRQn)){
        INTC_ClearPendingIRQ(DIV_IRQn);
    }
    
    if(INTC_GetPendingIRQ(IICA1_IRQn)){
        INTC_ClearPendingIRQ(IICA1_IRQn);
    }

    if(!INTC_GetPendingIRQ(IICA0_IRQn)){
        return;
    }
    INTC_ClearPendingIRQ(IICA0_IRQn);
    IICA0_CLEAR_WAKEUP();
    //忽略IICA0 Master中断
    if((IICA0->IICS0 & _80_IICA_MASTER_DEVICE) == 0x80U){
        return;
    }

    //IICA0 Slave处理：
    /* Control for stop condition */
    if (IICA0->IICS0 & IICA0_IICS0_SPD_Msk){
        /* Get stop condition */
        //禁止停止条件检测产生的中断请求
        IICA0->IICCTL00 &= ~IICA0_IICCTL00_SPIE_Msk; /* SPIE0 = 0: disable */
        switch(s_telm_state){
            case TELM_STATE_IDLE:
            break;
            case TELM_STATE_RX:
                if(s_telm_cur_data_count >= s_telm_cur_data_len){
                    if(s_telm_rx_buf[4] != TELM_RX_CHECKSUM(s_telm_rx_buf, 0)){
                        IICA_ERR_PROC();
                    }else{
                        s_telm_msg_cb(s_telm_rx_buf, TELM_IIC_RX_SIZE, s_telm_tx_buf);
                        s_telm_msg_cnt++;
                        s_telm_state = TELM_STATE_IDLE;
                    }
                    *((uint32_t*)s_telm_last_rx_buf) = *((uint32_t*)s_telm_rx_buf);
                }else if(s_telm_iic_state != TELM_IIC_STATE_LISTEN){
                    //异常处理
                    s_telm_state = TELM_STATE_IDLE;
                    IICA_ERR_PROC();
                }
            break;
            case TELM_STATE_MASS_RX:
                if(s_telm_cur_data_count >= s_telm_cur_data_len){
                    s_telm_evt_cb(BSP_TELM_EVT_MASS_RX_DONE);
                    s_telm_state = TELM_STATE_IDLE;
                }else if(s_telm_iic_state != TELM_IIC_STATE_LISTEN){
                    //异常处理
                    s_telm_state = TELM_STATE_IDLE;
                    IICA_ERR_PROC();
                }
            break;
            case TELM_STATE_TX:
                if(s_telm_cur_data_count >= s_telm_cur_data_len){
                    s_telm_state = TELM_STATE_IDLE;
                }else if(s_telm_iic_state != TELM_IIC_STATE_LISTEN){
                    s_telm_state = TELM_STATE_IDLE;
                    IICA_ERR_PROC();
                }
            break;
            case TELM_STATE_MASS_TX:
                if(s_telm_cur_data_count >= s_telm_cur_data_len){
                    s_telm_evt_cb(BSP_TELM_EVT_MASS_TX_DONE);
                    s_telm_state = TELM_STATE_IDLE;
                }else if(s_telm_iic_state != TELM_IIC_STATE_LISTEN){
                    //异常处理
                    s_telm_state = TELM_STATE_IDLE;
                    IICA_ERR_PROC();
                }
            break;
            default:
                //异常处理
                s_telm_state = TELM_STATE_IDLE;
                IICA_ERR_PROC();
            break;
        }
        s_telm_iic_state = TELM_IIC_STATE_LISTEN;
        return;
    }

    switch(s_telm_iic_state){
        case TELM_IIC_STATE_LISTEN: //监听状态
            if(!(IICA0->IICS0 & IICA0_IICS0_COI_Msk)){ /* 地址未匹配 */
                break;
            }

            switch(s_telm_state){
                case TELM_STATE_IDLE:
                    if(IICA0_IS_READ()){ /* 读写位为读 */
                        // 异常处理
                        IICA_ERR_PROC();
                        break;
                    }
                    //允许停止条件检测产生的中断请求
                    //允许应答。在第9个时钟期间将SDAAn线置为低电平。
                    //解除等待。在解除等待后自动清除。
                    IICA0->IICCTL00 |= (IICA0_IICCTL00_SPIE_Msk | IICA0_IICCTL00_ACKE_Msk);
                    //在第8个时钟的下降沿产生中断请求信号。
                    //从属设备：在输入8个时钟后，将时钟置为低电平，然后等待主控设备。  
                    IICA0->IICCTL00 &= ~IICA0_IICCTL00_WTIM_Msk;
                    IICA0_RELEASE_WAIT(); 
                    s_telm_iic_state = TELM_IIC_STATE_WAIT_ADDR1;
                break;
                case TELM_STATE_RX:
                case TELM_STATE_MASS_RX:
                    if(IICA0_IS_READ()){ /* 读写位为读 */
                        // 异常处理
                        IICA_ERR_PROC();
                        break;
                    }
                    //允许停止条件检测产生的中断请求
                    //允许应答。在第9个时钟期间将SDAAn线置为低电平。
                    IICA0->IICCTL00 |= (IICA0_IICCTL00_SPIE_Msk | IICA0_IICCTL00_ACKE_Msk);
                    //在第8个时钟的下降沿产生中断请求信号。
                    //从属设备：在输入8个时钟后，将时钟置为低电平，然后等待主控设备。  
                    IICA0->IICCTL00 &= ~IICA0_IICCTL00_WTIM_Msk;
                    IICA0_RELEASE_WAIT();
                    s_telm_iic_state = TELM_IIC_STATE_DATA_RX;
                break;
                case TELM_STATE_TX:
                    IICA0->IICA00 = s_telm_cur_data_buf[s_telm_cur_data_count++];
                    //在输入9个时钟后，将时钟置为低电平，然后等待主控设备
                    IICA0->IICCTL00 |= (IICA0_IICCTL00_SPIE_Msk | IICA0_IICCTL00_WTIM_Msk);
                    s_telm_iic_state = TELM_IIC_STATE_DATA_TX;
                break;
                case TELM_STATE_MASS_TX:
                    if(IICA0_IS_WRITE()){ /* 读写位为写 */
                        // 异常处理
                        IICA_ERR_PROC();
                        break;
                    }
                    IICA0->IICA00 = s_telm_cur_data_buf[s_telm_cur_data_count++];
                    //在输入9个时钟后，将时钟置为低电平，然后等待主控设备
                    IICA0->IICCTL00 |= (IICA0_IICCTL00_SPIE_Msk | IICA0_IICCTL00_WTIM_Msk);
                    s_telm_iic_state = TELM_IIC_STATE_DATA_TX;
                break;
                default:
                    // 异常处理
                    IICA_ERR_PROC();
                break;
            }
        break;
        case TELM_IIC_STATE_WAIT_ADDR1: //已收到IIC地址状态
            if(IICA0_IS_READ()){ /* 读写位为读 */
                // 异常处理
                IICA_ERR_PROC();
                break;
            }
            s_telm_rx_buf[0] = IICA0->IICA00;
            s_telm_iic_state = TELM_IIC_STATE_WAIT_ADDR2;
            IICA0_RELEASE_WAIT();
        break;
        case TELM_IIC_STATE_WAIT_ADDR2: //已收到IIC地址状态
            if(IICA0_IS_READ()){ /* 读写位为读 */
                // 异常处理
                IICA_ERR_PROC();
                break;
            }
            s_telm_rx_buf[1] = IICA0->IICA00;
            switch(s_telm_state){
                case TELM_STATE_IDLE:
                    if(s_telm_rx_buf[0] & 0x80){ //读软寄存器
                        ret = s_telm_msg_cb(s_telm_rx_buf, TELM_IIC_ADDR_SIZE, s_telm_tx_buf);
                        if(ret > 0){
                            if(s_telm_state == TELM_STATE_IDLE){ //没有切到Mass模式的情形
                                s_telm_tx_buf[TELM_IIC_TX_OFFSET + TELM_IIC_VALUE_SIZE] = TELM_TX_CHECKSUM(s_telm_tx_buf, TELM_IIC_TX_OFFSET);
                                telm_iica0_trans_start(&s_telm_tx_buf[TELM_IIC_TX_OFFSET], TELM_IIC_TX_SIZE, TELM_STATE_TX);
                                s_telm_tx_start_tick = bsp_telm_get_cur_tick();
                            }
                        }
#if !BSP_TELM_PRESS_TEST
                    }else if((s_telm_rx_buf[1] == TELM_MASS_ADDR) && (s_telm_rx_buf[0] == 0)){
                        s_telm_msg_cb(s_telm_rx_buf, TELM_IIC_ADDR_SIZE, s_telm_tx_buf);
#endif
                    }else{
                        //注意此时还没有STOP条件
                        telm_iica0_trans_start(&s_telm_rx_buf[TELM_IIC_ADDR_SIZE], TELM_IIC_VALUE_SIZE + TELM_IIC_CHECKSUM_SIZE, TELM_STATE_RX);
                        s_telm_rx_start_tick = bsp_telm_get_cur_tick();
                    }
                    break;
                case TELM_STATE_RX:
                case TELM_STATE_MASS_RX:
                case TELM_STATE_TX:
                case TELM_STATE_MASS_TX:
                default:
                    //异常处理
                    IICA_ERR_PROC();
                break;
            }
            IICA0_RELEASE_WAIT();
            // if(s_telm_state == TELM_IIC_STATE_WAIT_ADDR2){
            //     // 异常处理
            //     IICA_ERR_PROC();
            //     s_telm_state = TELM_IIC_STATE_LISTEN;
            // }
        break;
        case TELM_IIC_STATE_DATA_RX:
            if(IICA0_IS_READ()){ /* 读写位为读 */
                // 异常处理
                IICA_ERR_PROC();
                break;
            }
            if(s_telm_cur_data_count < s_telm_cur_data_len){
                s_telm_cur_data_buf[s_telm_cur_data_count++] = IICA0->IICA00;
            }

            if(s_telm_cur_data_count >= s_telm_cur_data_len){
                if(s_telm_state == TELM_STATE_RX){
                    if(s_telm_rx_buf[4] != TELM_RX_CHECKSUM(s_telm_rx_buf, 0)){
                        IICA_ERR_PROC();
                    }else{
                        s_telm_msg_cb(s_telm_rx_buf, TELM_IIC_RX_SIZE, s_telm_tx_buf);
                    }
                    *((uint32_t*)s_telm_last_rx_buf) = *((uint32_t*)s_telm_rx_buf);
                }else if(s_telm_state == TELM_STATE_MASS_RX){
                    s_telm_evt_cb(BSP_TELM_EVT_MASS_RX_DONE);
                }
                //接收完成
                s_telm_state = TELM_STATE_IDLE;
                s_telm_iic_state = TELM_IIC_STATE_LISTEN;
                //由停止条件配为TELM_IIC_STATE_LISTEN
            }
            IICA0_RELEASE_WAIT();
        break;
        case TELM_IIC_STATE_DATA_TX:
            if(IICA0_IS_WRITE()){ /* 读写位为写 */
                // 异常处理
                IICA_ERR_PROC();
                break;
            }
            if(s_telm_cur_data_count < s_telm_cur_data_len){
                IICA0->IICA00 = s_telm_cur_data_buf[s_telm_cur_data_count++];
            }else if(s_telm_cur_data_count == s_telm_cur_data_len){
                if(s_telm_state == TELM_STATE_MASS_TX){
                    s_telm_evt_cb(BSP_TELM_EVT_MASS_TX_DONE);
                }
                IICA0->IICA00 = 0xAA;
                //接收完成
                s_telm_state = TELM_STATE_IDLE;
                s_telm_iic_state = TELM_IIC_STATE_LISTEN;
            }else if(s_telm_cur_data_count > s_telm_cur_data_len){
                //异常情况处理
                IICA0->IICA00 = 0x55;
                IICA_ERR_PROC();
            }
        break;
        default:
        break;
    }

}

void telm_spi_interrupt(void)
{
    //注意ST0_IRQn、IIC00_IRQn、SPI00_IRQn是相同IRQ号
    if(INTC_GetPendingIRQ(SPI00_IRQn)){
        INTC_ClearPendingIRQ(SPI00_IRQn);
    }
}

static void telm_iica0_init(void)
{
    CGC->PER0 |= CGC_PER0_IICA0EN_Msk;              /* enables input clock supply */
    IICA0->IICCTL00 = _00_IICA_OPERATION_STOP;       /* stop operation */
    INTC_DisableIRQ(IICA0_IRQn);                         /* disable INTIICA interrupt flag */           
    INTC_ClearPendingIRQ(IICA0_IRQn);                    /* clear INTIICA interrupt flag */         
    /* IICA_FAST_MODE Max rate: 1Mbps */
    IICA0->IICCTL01  = _08_IICA_OPERATE_FAST;  /* operates in fast mode */
    IICA0->IICCTL01 |= _04_IICA_FILTER_ON;     /* filter on */
    IICA0->IICWL0    = _08_IICA_LOW_WIDTH;
    IICA0->IICWH0    = _08_IICA_HIGH_WIDTH;

    IICA0->IICCTL01 |= _01_IICA_FCLK_2_SELECTED;
    IICA0->SVA0      = TELM_IIC_ADDR; /* slave address */
    IICA0->IICF0    |= _02_IICA_WITHOUT_DETECTION; /* enable generation of a start condition without detecting a stop condition */
    IICA0->IICF0    |= _01_IICA_RESERVATION_DISABLE;       /* disable communication reservation */
    IICA0->IICCTL00 |= _08_IICA_INTERRUPT_REQUEST_NINTH; /* interrupt request is generated at the ninth clock's falling edge */
    IICA0->IICCTL00 |= _04_IICA_ACKOWNLEDGMENT_ENABLE;   /* enable acknowledgment */
    IICA0->IICCTL00 |= _80_IICA_OPERATION_ENABLE;                /* enable operation */
    IICA0->IICCTL00 |= _40_IICA_OPERATION_STANDBY;       /* this exits from the current communications and sets standby mode */
    INTC_EnableIRQ(IICA0_IRQn);                          /* enable INTIICA interrupt flag */
    NVIC_SetPriority(IICA0_IRQn, IRQ_PRIOR_TELM); 
    
    /* Set SCLA0, SDAA0 pin */
    TELM_SCLA0_PORT_SETTING();
    TELM_SDAA0_PORT_SETTING();
}

static void telm_print_err_rec(uint32_t count)
{
    int i;
    if(count){
        printf("err rec line(%u):", count);
        for(i = 0; i < TELM_ERR_REC_NUM; i++){
            printf("%u ", s_telm_err_rec_list[i]);
        }
        printf("\n");
    }else{
        printf("err rec: none\n");
    }   
}

static void bsp_telm_state_periodly_print(void) {
    static uint32_t last_tick = 0;

    if (bsp_tick_get() - last_tick < 5000) {
        return;
    }
    last_tick = bsp_tick_get();
    printf("\n=====================================\n");
    printf("telm state:%s\n", s_telm_state_str[s_telm_state]);
    printf("telm iic state:%s\n", s_telm_iic_state_str[s_telm_iic_state]);
    printf("msg count:  %d\n", s_telm_msg_cnt);
    printf("cur trans len:%d\n", s_telm_cur_data_len);
    printf("cur trans count:%d\n", s_telm_cur_data_count);
    printf("last rx: 0x%x 0x%x 0x%x 0x%x\n", s_telm_last_rx_buf[0],
           s_telm_last_rx_buf[1], s_telm_last_rx_buf[2], s_telm_last_rx_buf[3]);
    printf("rx buf: 0x%x 0x%x 0x%x 0x%x 0x%x\n", s_telm_rx_buf[0],
           s_telm_rx_buf[1], s_telm_rx_buf[2], s_telm_rx_buf[3],
           s_telm_rx_buf[4]);
    printf("tx buf: 0x%x 0x%x 0x%x\n", s_telm_tx_buf[0], s_telm_tx_buf[1],
           s_telm_tx_buf[2]);
    telm_print_err_rec(s_telm_err_rec_count);

    printf("IICA0 registers:\n");
    printf("IICCTL00=0x%x\n", IICA0->IICCTL00);
    printf("IICCTL01=0x%x\n", IICA0->IICCTL01);
    printf("IICWL0=0x%x\n", IICA0->IICWL0);
    printf("IICWH0=0x%x\n", IICA0->IICWH0);
    printf("SVA0=0x%x\n", IICA0->SVA0);
    //printf("IICA00=0x%x\n", IICA0->IICA00);
    printf("IICS0=0x%x\n", IICA0->IICS0);
    printf("IICF0=0x%x\n", IICA0->IICF0);

#if BSP_TELM_PRESS_TEST
    bsp_telm_dump_fake_reg();
#endif
}


int bsp_telm_init(bsp_telm_init_param_t *param)
{
#if BSP_TELM_PRESS_TEST
    s_telm_msg_cb = bsp_telm_fake_msg_callback;
#else
    if(param){
        if(param->msg_cb){
            s_telm_msg_cb = param->msg_cb;
        }
        if(param->evt_cb){
            s_telm_evt_cb = param->evt_cb;
        }
    }
#endif
    s_telm_tx_timeout_val = TELM_IIC_TIMEOUT_MIN_MS;
    s_telm_rx_timeout_val = TELM_IIC_TIMEOUT_MIN_MS;
    telm_iica0_init();

    printf("bsp telm init OK, using i2c\n");

    return 0;
}

int bsp_telm_mass_revc(uint8_t *buf, uint16_t rx_len)
{
    s_telm_rx_timeout_val = TELM_IIC_TIMEOUT_MIN_MS + TELM_IIC_TIMEOUT_1BYTE_MS * rx_len;
    if(s_telm_rx_timeout_val > TELM_IIC_TIMEOUT_MAX_MS){
        s_telm_rx_timeout_val = TELM_IIC_TIMEOUT_MAX_MS;
    }
    s_telm_rx_start_tick = bsp_telm_get_cur_tick();
    telm_iica0_trans_start(buf, rx_len, TELM_STATE_MASS_RX); 
    return 0;
}
int bsp_telm_mass_send(uint8_t *buf, uint16_t tx_len)
{
    if(tx_len < 2){
        return -1;
    }
    tx_len--;
    s_telm_tx_timeout_val = TELM_IIC_TIMEOUT_MIN_MS + TELM_IIC_TIMEOUT_1BYTE_MS * tx_len;
    if(s_telm_tx_timeout_val > TELM_IIC_TIMEOUT_MAX_MS){
        s_telm_tx_timeout_val = TELM_IIC_TIMEOUT_MAX_MS;
    }
    s_telm_tx_start_tick = bsp_telm_get_cur_tick();
    telm_iica0_trans_start(buf, tx_len, TELM_STATE_MASS_TX); 
    return 0;
}


void bsp_telm_state_check(void) 
{
    static uint32_t last_check_tick = 0;
    static uint32_t last_err_rec_count = 0;
    uint32_t cur_tick;
    uint32_t timeout_val;
    switch (s_telm_state) {
        case TELM_STATE_IDLE:

        break;
        case TELM_STATE_RX:
            timeout_val = TELM_IIC_TIMEOUT_MIN_MS;
            cur_tick = bsp_telm_get_cur_tick();
            if((cur_tick == last_check_tick)
            || (cur_tick - s_telm_rx_start_tick < timeout_val)){
                break;
            }
            bsp_disable_irq();
            if(TELM_TICK_INVALID != s_telm_rx_start_tick){
                s_telm_rx_start_tick = TELM_TICK_INVALID;
                bsp_enable_irq();
                telm_iica0_restart();
                bsp_gpio_toggle(LED2_PORT, LED2_PIN);
                printf("####telm iic rx timeout\n");
                bsp_gpio_toggle(LED2_PORT, LED2_PIN);
            }else{
                bsp_enable_irq();
            }
            last_check_tick = cur_tick;
        break;
        case TELM_STATE_TX:
            timeout_val = TELM_IIC_TIMEOUT_MIN_MS;
            cur_tick = bsp_telm_get_cur_tick();
            if((cur_tick == last_check_tick)
            || (cur_tick - s_telm_tx_start_tick < timeout_val)){
                break;
            }
            bsp_disable_irq();
            if(TELM_TICK_INVALID != s_telm_tx_start_tick){
                s_telm_tx_start_tick = TELM_TICK_INVALID;
                bsp_enable_irq();
                telm_iica0_restart();
                bsp_gpio_toggle(LED2_PORT, LED2_PIN);
                printf("####telm iic tx timeout\n");
                printf("tx buf: 0x%x 0x%x 0x%x\n", s_telm_tx_buf[0],
                       s_telm_tx_buf[1], s_telm_tx_buf[2]);
                bsp_gpio_toggle(LED2_PORT, LED2_PIN);
            }else{
                bsp_enable_irq();
            }
            last_check_tick = cur_tick;
            break;
        case TELM_STATE_MASS_RX:
            if(bsp_tick_get() - s_telm_rx_start_tick > s_telm_rx_timeout_val){
                telm_iica0_restart();
                s_telm_evt_cb(BSP_TELM_EVT_MASS_RX_TIMEOUT);
                printf("####telm iic mass rx timeout\n");
            }
        break;
        case TELM_STATE_MASS_TX:
            timeout_val = s_telm_tx_timeout_val;
            cur_tick = bsp_telm_get_cur_tick();
            if((cur_tick == last_check_tick)
            || (cur_tick - s_telm_tx_start_tick < timeout_val)){
                break;
            }
            if(TELM_TICK_INVALID != s_telm_tx_start_tick){
                s_telm_tx_start_tick = TELM_TICK_INVALID;
                telm_iica0_restart();
                s_telm_evt_cb(BSP_TELM_EVT_MASS_TX_TIMEOUT);
                printf("####telm iic mass tx timeout\n");
            }
            last_check_tick = cur_tick;
        break;
        default:
            telm_iica0_restart();
            printf("####restart iic rx\n");
            break;
    }

    if(last_err_rec_count != s_telm_err_rec_count){
        last_err_rec_count = s_telm_err_rec_count;
        telm_print_err_rec(last_err_rec_count);
        bsp_telm_state_periodly_print();
    }

}

#else


#define TELM_SPI_TIMEOUT_MIN_MS 50 //must more than 0 ms
#define TELM_SPI_TIMEOUT_MAX_MS 500
#define TELM_SPI_TIMEOUT_1BYTE_MS 10

#define TELM_SPI_RX_COUNT	5
#define TELM_SPI_TX_COUNT	BSP_TELM_TX_LEN_MAX //3
#define BSP_TELM_TX_DMA_LEN (TELM_SPI_TX_COUNT + 2)

#define TELM_SPI_CS_FAIL_RESET_THRESHOLD  3

static uint8_t __attribute__((aligned(4))) s_telm_spi_last_rx_buf[4];

static uint8_t __attribute__((aligned(4))) s_telm_spi_rx_buf[TELM_SPI_RX_COUNT];
static uint8_t s_telm_spi_tx_buf[BSP_TELM_TX_DMA_LEN];
static uint8_t s_telm_spi_dummy_read = 0;

static uint8_t s_telm_spi_last_error = 0;
static uint32_t s_telm_spi_error_count = 0;
static uint32_t s_telm_spi_checksum_fail_count = 0;
static uint32_t s_telm_spi_checksum_fail_sum = 0;

static int telm_spi_init(void)
{
    SPI00_SlaveInit(SPI_MODE_0);
    NVIC_SetPriority(SPI00_IRQn, IRQ_PRIOR_TELM);
    return 0;
}

static inline void telm_spi_stop(void)
{
    //same as SPI00_Stop();
    /* disable INTSPI00 interrupt */
    INTC_DisableIRQ(SPI00_IRQn);
    SCI0->ST0 |= _0001_SCI_CH0_STOP_TRG_ON; //Serial channel stop register 0 
    SCI0->SOE0 &= (uint16_t)~_0001_SCI_CH0_OUTPUT_ENABLE; //Serial output enable/stop of channel 0 (SOEm0)
    /* clear INTSPI00 interrupt flag */
    INTC_ClearPendingIRQ(SPI00_IRQn);
    //disable dma
    DMA->DMAEN1 &= ~(1 << DMA_VECTOR_SPI00 % 8);
    s_telm_state = TELM_STATE_IDLE;
}

static inline int telm_spi_rx_start(uint8_t *buf, uint16_t size, uint32_t state)
{
    s_telm_spi_dummy_read = SCI0->SIO00;
    SCI0->SCR00 &= ~_C000_SCI_RECEPTION_TRANSMISSION;
    SCI0->SCR00 |= _4000_SCI_RECEPTION;
    SCI0->SMR00 &= ~_0001_SCI_BUFFER_EMPTY;

//#ifdef SPI00_WITH_DMA
    /* read receive data with DMA */
    DMAVEC->VEC[DMA_VECTOR_SPI00] = CTRL_DATA_SPI00;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMACR = (0 << CTRL_DMACR_SZ_Pos) | (0 << CTRL_DMACR_CHNE_Pos) |
                                            (1 << CTRL_DMACR_DAMOD_Pos) | (0 << CTRL_DMACR_SAMOD_Pos) |
                                            (0 << CTRL_DMACR_MODE_Pos);
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMBLS = 1;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMACT = size;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMRLD = size;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMSAR = (uint32_t)&SCI0->SIO00;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMDAR = (uint32_t)buf;

    /* init DMA registers */
    CGC->PER1 |= CGC_PER1_DMAEN_Msk;
    DMA->DMABAR = DMAVEC_BASE;
    DMA->DMAEN1 |= (1 << DMA_VECTOR_SPI00 % 8);
//#endif
    //same as SPI00_Start();
    SCI0->SOE0 |= _0001_SCI_CH0_OUTPUT_ENABLE;
    SCI0->SS0 |= _0001_SCI_CH0_START_TRG_ON;
    /* clear INTSPI00 interrupt flag */
    INTC_ClearPendingIRQ(SPI00_IRQn);
    NVIC_ClearPendingIRQ(SPI00_IRQn);
    /* enable INTSPI00 interrupt */
    INTC_EnableIRQ(SPI00_IRQn);

    SCI0->SIO00 = 0xAA;
    s_telm_state = state;
    return 0;
}


static inline int telm_spi_tx_start(uint8_t *buf, uint16_t size, uint32_t state)
{
    //要更改串行通信运行设定寄存器mn的设定时， 重新设定。
    SCI0->SCR00 &= ~_C000_SCI_RECEPTION_TRANSMISSION;//禁止接收与发送
    SCI0->SCR00 |= _8000_SCI_TRANSMISSION;//(只)使能发送
    SCI0->SMR00 |= _0001_SCI_BUFFER_EMPTY;//发送缓冲区空的时候产生中断，DMA接着搬下一个数据

//#ifdef SPI00_WITH_DMA
    /* write transfer data with DMA */
    DMAVEC->VEC[DMA_VECTOR_SPI00] = CTRL_DATA_SPI00;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMACR = (0 << CTRL_DMACR_SZ_Pos) | (0 << CTRL_DMACR_CHNE_Pos) |
                                            (0 << CTRL_DMACR_DAMOD_Pos) | (1 << CTRL_DMACR_SAMOD_Pos) |
                                            (0 << CTRL_DMACR_MODE_Pos);
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMBLS = 1;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMACT = size;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMRLD = size;
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMSAR = (uint32_t)(buf + 1);
    DMAVEC->CTRL[CTRL_DATA_SPI00].DMDAR = (uint32_t)&SCI0->SIO00;

    /* init DMA registers */
    CGC->PER1 |= CGC_PER1_DMAEN_Msk;
    DMA->DMABAR = DMAVEC_BASE;
    ///* UART0发送传送结束/SPI00传送结束或者缓冲器空/IIC00传送结束 */
    DMA->DMAEN1 |= (1 << DMA_VECTOR_SPI00 % 8);
//#endif

    //same as SPI00_Start();
    SCI0->SOE0 |= _0001_SCI_CH0_OUTPUT_ENABLE;
    SCI0->SS0 |= _0001_SCI_CH0_START_TRG_ON;
    /* clear INTSPI00 interrupt flag */
    INTC_ClearPendingIRQ(SPI00_IRQn);
    NVIC_ClearPendingIRQ(SPI00_IRQn);
    /* enable INTSPI00 interrupt */
    INTC_EnableIRQ(SPI00_IRQn);

    SCI0->SIO00 =buf[0]; /* started by writing data to SDR[7:0] */
    s_telm_tx_start_tick = bsp_telm_get_cur_tick();
    s_telm_state = state;
    return 0;
}

void telm_iica0_interrupt(void)
{
    if(INTC_GetPendingIRQ(DIV_IRQn)){
        INTC_ClearPendingIRQ(DIV_IRQn);
    }
    
    if(INTC_GetPendingIRQ(IICA1_IRQn)){
        INTC_ClearPendingIRQ(IICA1_IRQn);
    }

    if(INTC_GetPendingIRQ(IICA0_IRQn)){
        INTC_ClearPendingIRQ(IICA0_IRQn);
    }
}

void telm_spi_interrupt(void)
{
    int ret;
    volatile uint8_t err_type;

    //注意ST0_IRQn、IIC00_IRQn、SPI00_IRQn是相同IRQ号
    if(!INTC_GetPendingIRQ(SPI00_IRQn)) return;

    INTC_ClearPendingIRQ(SPI00_IRQn);
    err_type = (uint8_t)(SCI0->SSR00 & _0001_SCI_OVERRUN_ERROR);
    SCI0->SIR00 = (uint16_t)err_type;

    if (_0001_SCI_OVERRUN_ERROR == err_type){
        /* overrun error occurs */
        s_telm_spi_dummy_read = SCI0->SIO00;
        if(s_telm_state == TELM_STATE_MASS_RX){
            s_telm_evt_cb(BSP_TELM_EVT_MASS_RX_TIMEOUT);
        }else if(s_telm_state == TELM_STATE_MASS_TX){
            s_telm_evt_cb(BSP_TELM_EVT_MASS_TX_TIMEOUT);
        }
        telm_spi_stop();
        telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
        s_telm_spi_last_error = err_type;
        s_telm_spi_error_count++;
    }else{
        switch(s_telm_state)
        {
            case TELM_STATE_RX:
                /* complete receive */
                if(s_telm_spi_rx_buf[4] != TELM_RX_CHECKSUM(s_telm_spi_rx_buf, 0)){
                    telm_spi_stop();
                    telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
                    s_telm_spi_checksum_fail_count++;
                    s_telm_spi_checksum_fail_sum++;
                    *((uint32_t*)s_telm_spi_last_rx_buf) = *((uint32_t*)s_telm_spi_rx_buf);
                    break;
                }
                ret = s_telm_msg_cb(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, s_telm_spi_tx_buf);
                if(s_telm_state == TELM_STATE_RX){
                    if(ret > 0){
                        s_telm_spi_tx_buf[3] = TELM_TX_CHECKSUM(s_telm_spi_tx_buf, 1);
                        telm_spi_tx_start(s_telm_spi_tx_buf, BSP_TELM_TX_DMA_LEN, TELM_STATE_TX);
                    }else{
                        telm_spi_stop();
                        telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX); 
                    }
                }

                *((uint32_t*)s_telm_spi_last_rx_buf) = *((uint32_t*)s_telm_spi_rx_buf);
                s_telm_spi_checksum_fail_count = 0;
                s_telm_msg_cnt++;
            break;
            case TELM_STATE_TX:
                /* complete send */
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX); 
                break;
            case TELM_STATE_MASS_RX:
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
                s_telm_evt_cb(BSP_TELM_EVT_MASS_RX_DONE);
                s_telm_spi_checksum_fail_count = 0;
                break;
            case TELM_STATE_MASS_TX:
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
                s_telm_evt_cb(BSP_TELM_EVT_MASS_TX_DONE);
                s_telm_spi_checksum_fail_count = 0;
                break;
            default:
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX); 
            break;
        }
    }
}

int bsp_telm_mass_revc(uint8_t *buf, uint16_t rx_len)
{
    s_telm_rx_timeout_val = TELM_SPI_TIMEOUT_MIN_MS + TELM_SPI_TIMEOUT_1BYTE_MS * rx_len;
    if(s_telm_rx_timeout_val > TELM_SPI_TIMEOUT_MAX_MS){
        s_telm_rx_timeout_val = TELM_SPI_TIMEOUT_MAX_MS;
    }
    telm_spi_stop();
    s_telm_rx_start_tick = bsp_telm_get_cur_tick();
    telm_spi_rx_start(buf, rx_len, TELM_STATE_MASS_RX); 
    return 0;
}
int bsp_telm_mass_send(uint8_t *buf, uint16_t tx_len)
{
    s_telm_tx_timeout_val = TELM_SPI_TIMEOUT_MIN_MS + TELM_SPI_TIMEOUT_1BYTE_MS * tx_len;
    if(s_telm_tx_timeout_val > TELM_SPI_TIMEOUT_MAX_MS){
        s_telm_tx_timeout_val = TELM_SPI_TIMEOUT_MAX_MS;
    }
    telm_spi_tx_start(buf, tx_len, TELM_STATE_MASS_TX); 
    return 0;
}


int bsp_telm_init(bsp_telm_init_param_t *param)
{
    if(param){
        if(param->msg_cb){
            s_telm_msg_cb = param->msg_cb;
        }
        if(param->evt_cb){
            s_telm_evt_cb = param->evt_cb;
        }
    }
    s_telm_tx_timeout_val = TELM_SPI_TIMEOUT_MIN_MS;
    s_telm_rx_timeout_val = TELM_SPI_TIMEOUT_MIN_MS;
    telm_spi_init();
    telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);

    printf("bsp telm init OK, using spi\n");

    return 0;
}

static void bsp_telm_state_periodly_print(void) {
    static uint32_t last_tick = 0;
    if (bsp_tick_get() - last_tick < 5000) {
        return;
    }
    last_tick = bsp_tick_get();
    printf("\n=====================================\n");
    printf("telm state:%s\n", s_telm_state_str[s_telm_state]);
    printf("msg count:  %d\n", s_telm_msg_cnt);
    printf("last_error 0x%x, count %d\n", s_telm_spi_last_error,
           s_telm_spi_error_count);
    printf("checksum fail count %d, sum %d\n", s_telm_spi_checksum_fail_count, s_telm_spi_checksum_fail_sum);
    printf("last rx: 0x%x 0x%x 0x%x 0x%x\n", s_telm_spi_last_rx_buf[0],
           s_telm_spi_last_rx_buf[1], s_telm_spi_last_rx_buf[2],
           s_telm_spi_last_rx_buf[3]);
    printf("rx buf: 0x%x 0x%x 0x%x 0x%x 0x%x\n", s_telm_spi_rx_buf[0],
           s_telm_spi_rx_buf[1], s_telm_spi_rx_buf[2], s_telm_spi_rx_buf[3],
           s_telm_spi_rx_buf[4]);
    printf("tx buf: 0x%x 0x%x 0x%x 0x%x\n", s_telm_spi_tx_buf[0],
           s_telm_spi_tx_buf[1], s_telm_spi_tx_buf[2], s_telm_spi_tx_buf[3]);
}

void bsp_telm_state_check(void) 
{
#if 1
    static uint32_t last_check_tick = 0;
    uint32_t cur_tick;
    uint32_t timeout_val;
    switch (s_telm_state) {
        case TELM_STATE_RX:
        break;
        case TELM_STATE_TX:
            timeout_val = TELM_SPI_TIMEOUT_MIN_MS;
            cur_tick = bsp_telm_get_cur_tick();
            if((cur_tick == last_check_tick)
            || (cur_tick - s_telm_tx_start_tick < timeout_val)){
                break;
            }
            bsp_disable_irq();
            if(TELM_TICK_INVALID != s_telm_tx_start_tick){
                s_telm_tx_start_tick = TELM_TICK_INVALID;
                bsp_enable_irq();
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
                bsp_gpio_toggle(LED2_PORT, LED2_PIN);
                printf("####telm spi tx timeout\n");
                printf("tx buf: 0x%x 0x%x 0x%x 0x%x\n", s_telm_spi_tx_buf[0],
                       s_telm_spi_tx_buf[1], s_telm_spi_tx_buf[2],
                       s_telm_spi_tx_buf[3]);
                bsp_gpio_toggle(LED2_PORT, LED2_PIN);
            }else{
                bsp_enable_irq();
            }
            last_check_tick = cur_tick;
            break;
        case TELM_STATE_MASS_RX:
            if(bsp_tick_get() - s_telm_rx_start_tick > s_telm_rx_timeout_val){
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
                s_telm_evt_cb(BSP_TELM_EVT_MASS_RX_TIMEOUT);
                printf("####telm spi mass rx timeout\n");
            }
        break;
        case TELM_STATE_MASS_TX:
            timeout_val = s_telm_tx_timeout_val;
            cur_tick = bsp_telm_get_cur_tick();
            if((cur_tick == last_check_tick)
            || (cur_tick - s_telm_tx_start_tick < timeout_val)){
                break;
            }
            if(TELM_TICK_INVALID != s_telm_tx_start_tick){
                s_telm_tx_start_tick = TELM_TICK_INVALID;
                telm_spi_stop();
                telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
                s_telm_evt_cb(BSP_TELM_EVT_MASS_TX_TIMEOUT);
                printf("####telm spi mass tx timeout\n");
            }
            last_check_tick = cur_tick;
        break;
        default:
            printf("####restart spi rx\n");
            telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
            break;
    }
#endif

    if(TELM_SPI_CS_FAIL_RESET_THRESHOLD <= s_telm_spi_checksum_fail_count){
        bsp_delay_ms(10);
        telm_spi_stop();
        telm_spi_rx_start(s_telm_spi_rx_buf, TELM_SPI_RX_COUNT, TELM_STATE_RX);
        printf("[tm%u]###spi checksum fail (count %d, sum %d) too many, reset spi\n", 
            bsp_tick_get(), s_telm_spi_checksum_fail_count, s_telm_spi_checksum_fail_sum);
        s_telm_spi_checksum_fail_count = 0;
    }

    //bsp_telm_state_periodly_print();
}


#endif

void bsp_telm_state_print(void)
{
    bsp_telm_state_periodly_print();
}

uint32_t bsp_telm_msg_count(void)
{
    return s_telm_msg_cnt;
}

void bsp_telm_test(void)
{
    bsp_telm_state_check();
    bsp_telm_state_periodly_print();
}
