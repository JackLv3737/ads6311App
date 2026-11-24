/**
 * @file bsp_i2c.c
 * @author rongchao
 * @brief
 * @version 0.1
 * @date 2023-08-08
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "BAT32A239.h"
#include "userdefine.h"
#include "bsp_basic.h"
#include "bsp_i2c.h"
#include "sci.h"
#include "gpio.h"

#define BSP_I2C_TIMEOUT_MS  50

#define BSP_USING_SW_IIC

#ifdef BSP_USING_SW_IIC

#define SCL_PORT  PORT7
#define SCL_PIN     PIN5

#define SDA_PORT  PORT7
#define SDA_PIN     PIN4


#define SWIIC_SDA_CFG_AS_IN()       //PORT_Init(SDA_PORT, SDA_PIN, PULLUP_INPUT);
#define SWIIC_SDA_CFG_AS_OUT()    //PORT_Init(SDA_PORT, SDA_PIN, OPENDRAIN_OUTPUT);

#define SWIIC_SCL_OUTPUT(m_level)   (m_level ? (PORT->PSET7 =  (1 << SCL_PIN)) : (PORT->PCLR7 = (1 << SCL_PIN)))
#define SWIIC_SDA_OUTPUT(m_level)  (m_level ? (PORT->PSET7  =  (1 << SDA_PIN)) : (PORT->PCLR7 = (1 << SDA_PIN)))
#define SWIIC_READ_SDA()    ((PORT->PREAD7) & (1 << SDA_PIN))



#define SWIIC_DELAY_US(m_us)    bsp_delay_us(m_us)


static void bsp_swiic_init(void)
{
    //sda / scl gpio init
    PORT_Init(SCL_PORT, SCL_PIN, OUTPUT);
    SWIIC_DELAY_US(4);
    PORT_Init(SDA_PORT, SDA_PIN, OPENDRAIN_OUTPUT);
    SWIIC_DELAY_US(4);
    SWIIC_SCL_OUTPUT(1);
    SWIIC_DELAY_US(4);
    SWIIC_SCL_OUTPUT(1);
}

//产生IIC起始信号
static void bsp_swiic_start(void)
{
    SWIIC_SDA_CFG_AS_OUT();     //sda线输出
    SWIIC_SDA_OUTPUT(1);
    SWIIC_SCL_OUTPUT(1);
    SWIIC_DELAY_US(4);
    SWIIC_SDA_OUTPUT(0);//START:when CLK is high,DATA change form high to low
    SWIIC_DELAY_US(4);
    SWIIC_SCL_OUTPUT(0);//钳住I2C总线，准备发送或接收数据
}
//产生IIC停止信号
static void bsp_swiic_stop(void)
{
    SWIIC_SDA_CFG_AS_OUT();//sda线输出
    SWIIC_SCL_OUTPUT(0);
    SWIIC_SDA_OUTPUT(0);//STOP:when CLK is high DATA change form low to high
    SWIIC_DELAY_US(4);
    SWIIC_SCL_OUTPUT(1);
    SWIIC_SDA_OUTPUT(1);//发送I2C总线结束信号
    SWIIC_DELAY_US(4);
}
//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
static int bsp_swiic_wait_ack(void)
{
    int ucErrTime=0;
    SWIIC_SDA_CFG_AS_IN();      //SDA设置为输入
    SWIIC_SDA_OUTPUT(1);
    SWIIC_DELAY_US(1);
    SWIIC_SCL_OUTPUT(1);
    SWIIC_DELAY_US(1);
    while(SWIIC_READ_SDA())
    {
        ucErrTime++;
        if(ucErrTime>250)
        {
            bsp_swiic_stop();
            return 1;
        }
    }
    SWIIC_SCL_OUTPUT(0);//时钟输出0
    return 0;
}
//产生ACK应答
static void bsp_swiic_gen_ack(void)
{
    SWIIC_SCL_OUTPUT(0);
    SWIIC_SDA_CFG_AS_OUT();
    SWIIC_SDA_OUTPUT(0);
    SWIIC_DELAY_US(2);
    SWIIC_SCL_OUTPUT(1);
    SWIIC_DELAY_US(2);
    SWIIC_SCL_OUTPUT(0);
    SWIIC_DELAY_US(1);
    SWIIC_SDA_OUTPUT(1);
}
//不产生ACK应答
static void bsp_swiic_gen_nack(void)
{
    SWIIC_SCL_OUTPUT(0);
    SWIIC_SDA_CFG_AS_OUT();
    SWIIC_SDA_OUTPUT(1);
    SWIIC_DELAY_US(2);
    SWIIC_SCL_OUTPUT(1);
    SWIIC_DELAY_US(2);
    SWIIC_SCL_OUTPUT(0);
}
//IIC发送一个字节
//返回从机有无应答
// 1，有应答
// 0，无应答
static void bsp_swiic_send_byte(uint8_t data)
{
    int i;
    SWIIC_SDA_CFG_AS_OUT();
    SWIIC_SCL_OUTPUT(0);//拉低时钟开始数据传输
    for(i=0;i<8;i++)
    {
        SWIIC_SDA_OUTPUT((data&0x80)>>7);
        data<<=1;
        SWIIC_DELAY_US(2);   //
        SWIIC_SCL_OUTPUT(1);
        SWIIC_DELAY_US(2);
        SWIIC_SCL_OUTPUT(0);
        SWIIC_DELAY_US(2);
    }
}
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK
static uint8_t bsp_swiic_read_byte(int is_reply_ack)
{
    unsigned char i,receive=0;
    SWIIC_SDA_CFG_AS_IN();//SDA设置为输入
    for(i=0;i<8;i++)
    {
        SWIIC_SCL_OUTPUT(0);
        SWIIC_DELAY_US(2);
        SWIIC_SCL_OUTPUT(1);
        receive<<=1;
        if(SWIIC_READ_SDA())receive++;
        SWIIC_DELAY_US(1);
    }
    if (!is_reply_ack){
        bsp_swiic_gen_nack();//发送nACK
    }else{
        bsp_swiic_gen_ack(); //发送ACK
    }
    return receive;
}


#endif




static void bsp_i2c01_Init()
{
    CGC->PER0 |= CGC_PER0_SCI0EN_Msk;
    //SCI0->SPS0 = _0000_SCI_CK01_fCLK_0 | _0001_SCI_CK00_fCLK_1;

    SCI0->ST0 |= _0002_SCI_CH1_STOP_TRG_ON;

    SCI0->SIR01 = _0004_SCI_SIRMN_FECTMN | _0002_SCI_SIRMN_PECTMN | _0001_SCI_SIRMN_OVCTMN;
    SCI0->SMR01 = _0020_SMRMN_DEFAULT_VALUE | _8000_SCI_CLOCK_SELECT_CK01 | _0000_SCI_CLOCK_MODE_CKS |
                  _0000_SCI_TRIGGER_SOFTWARE | _0000_SCI_EDGE_FALL | _0004_SCI_MODE_IIC | _0000_SCI_TRANSFER_END;
    SCI0->SCR01 = _0004_SCRMN_DEFAULT_VALUE | _0000_SCI_TIMING_1 | _0000_SCI_INTSRE_MASK | _0000_SCI_PARITY_NONE |
                  _0000_SCI_MSB | _0010_SCI_STOP_1 | _0007_SCI_LENGTH_8;
    SCI0->SDR01 = 0xC000;
    SCI0->SO0 |= _0200_SCI_CH1_CLOCK_OUTPUT_1 | _0002_SCI_CH1_DATA_OUTPUT_1;
    /* Set SCL01 pin */
    SCL01_PORT_SETTING();
    /* Set SDA01 pin */
    SDA01_PORT_SETTING();
}

int bsp_i2c_init(void)
{
#ifdef BSP_USING_SW_IIC
    bsp_swiic_init();
#else
    bsp_i2c01_Init();
#endif
    return 0;
}

int bsp_i2c_write(uint8_t chn, uint8_t DevAddr, uint8_t regaddr, uint8_t *pbuf, uint16_t datalen)
{
    uint32_t start_time;
    int i;
    uint8_t new_buf[I2C_DATA_MAX_LENGTH + 1];
    if(datalen > I2C_DATA_MAX_LENGTH || datalen < I2C_DATA_MIN_LENGTH){
        return -1;
    }

#ifdef BSP_USING_SW_IIC
    bsp_swiic_start();
    bsp_swiic_send_byte(DevAddr);
    if(bsp_swiic_wait_ack()){
        return -1;
    }
    bsp_swiic_send_byte(regaddr);
    if(bsp_swiic_wait_ack()){
        return -1;
    }
    for(i = 0; i < datalen; i++){
        bsp_swiic_send_byte(pbuf[i]);
        if(bsp_swiic_wait_ack()){
            return -1;
        }
    }
    bsp_swiic_stop();

#else
    new_buf[0] = regaddr;
    for(i=0;i<datalen;i++){
        new_buf[i+1] = pbuf[i];
    }

    g_iic01_tx_end = 0;
    IIC01_MasterSend(DevAddr, new_buf, datalen + 1);
    start_time = bsp_tick_get();
    while(g_iic01_tx_end == 0){
        if((bsp_tick_get() - start_time) > BSP_I2C_TIMEOUT_MS){
            IIC01_Stop();
            IIC01_StopCondition();
            printf("bsp i2c write timeout, chn %u, dev addr 0x%x\n", chn, DevAddr);
            return -1;
        }
    }

#endif

    return 0;
}

int bsp_i2c_read(uint8_t chn, uint8_t DevAddr, uint8_t regaddr, uint8_t *pbuf, uint16_t datalen)
{
    uint32_t start_time;
    uint8_t temp;
    if((!pbuf) || (datalen == 0)){
        return -1;
    }
#ifdef BSP_USING_SW_IIC

    if(bsp_i2c_write(chn, DevAddr, regaddr, &temp, 0)){
        return -1;
    }

    bsp_swiic_start();
    bsp_swiic_send_byte(DevAddr | 0x01);
    if(bsp_swiic_wait_ack()){
        return -1;
    }

    while(datalen > 1){
        *pbuf++ = bsp_swiic_read_byte(1);
        datalen--;
    }

    *pbuf++ = bsp_swiic_read_byte(0);
    bsp_swiic_stop();

#else
    g_iic01_tx_end = 0;
    IIC01_MasterSend(DevAddr, &regaddr, 1);
    start_time = bsp_tick_get();
    while(g_iic01_tx_end == 0){
        if((bsp_tick_get() - start_time) > BSP_I2C_TIMEOUT_MS){
            IIC01_Stop();
            IIC01_StopCondition();
            printf("bsp i2c write timeout, chn %u, dev addr 0x%x\n", chn, DevAddr);
            return -1;
        }
    }

    IIC01_StopCondition();
    g_iic01_rx_end = 0;
    IIC01_MasterReceive(DevAddr, pbuf, datalen);
    start_time = bsp_tick_get();
    while(g_iic01_rx_end == 0){
        if((bsp_tick_get() - start_time) > BSP_I2C_TIMEOUT_MS){
            IIC01_Stop();
            IIC01_StopCondition();
            printf("bsp i2c read timeout, chn %u, dev addr 0x%x\n", chn, DevAddr);
            return -1;
        }
    }
#endif
    return 0;
}

void bsp_i2c_test(void)
{
    char data[2];
    printf("\n%s ++++\n", __FUNCTION__);
    data[0] = 0x12;
    data[1] = 0x12;
    bsp_i2c_write(0, 0x48, 00, data, sizeof(data));
    printf("\n%s ----\n", __FUNCTION__);
}