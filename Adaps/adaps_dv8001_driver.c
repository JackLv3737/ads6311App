#include "adaps_dv8001_driver.h"
#include "board_config.h"
#include "bsp_basic.h"
#include "bsp_gpio.h"
#include "bsp_spi.h"
#include "string.h"
#include "bsp_adc.h"
#include <stdio.h>
#include "adaps_pmic_driver.h"
#include "common.h"

typedef enum en_dv8001_chip_id {
  en_dv8001_chip1,
  en_dv8001_chip2,
  EN_DV8001_CHIP_COUNT
} en_dv8001_chip_id_t;

#define DV8001_CHAN_COUNT          32

#define DV8001_CHAN_DISABLE_ALL    (DV8001_CHAN_COUNT + 1)


#define DV8001_chip1_SPI_CHAN     ADS7311_CHIP1_SPI_CHN
#define DV8001_chip2_SPI_CHAN     ADS7311_CHIP2_SPI_CHN


#define DV8001_TX_CONFIG_DATA_SIZE 5

#define DV8001_CHIPID_VERSION1  0x81
#define DV8001_CHIPID_VERSION2  0x82


typedef enum en_dv8001_reg_status_mask {
    //Soft reset, write 1 reset all resigters
    OFFSET_SOFT_RESET = 0x00,

    // 1:HV60 under voltage will not cause interruption;
    // 0:HV60 under voltage will cause interruption
    OFFSET_UVLO60V_MSK,

    // 1:LDO5 over current will not cause interruption
    // 0:LDO5 over current will cause interruption
    OFFSET_LDO5_OCP_ERR_MSK,

    // 1:HV60 over voltage will not cause interruption
    // 0:HV60 over voltage will cause interruption
    OFFSET_OV60V_ERR_MSK,

    // 1:violation of eye safety time will not cause interruption
    // 0:violation of eye safety time will cause interruption
    OFFSET_OVER_TEMP_MSK,

    // 1:short circuit will not cause interruption
    // 0:short circuit will cause interruption
    OFFSET_CHANL_SHORT_MSK,

    // 0:normal mode, 1:test mode
    OFFSET_CHIP_MODE,

    //LDO5 OK flag mask
    OFFSET_LDO5_OK_MSK
}en_dv8001_reg_status_mask_t;

typedef struct dv8001_chip_state_ {
  uint8_t u8_dv8001_reg_state[ARR_LENGTH_DV8001_REG];
  uint8_t u8_dv8001_err_state;
  uint8_t u8_err_interrupt_flag;
} dv8001_chip_state_t;

static dv8001_chip_state_t s_dv8001_chip_state[EN_DV8001_CHIP_COUNT];

static uint8_t s_dv8001_chip_enable_flag = DV8001_CHIP_ALL_ENABLE;

static uint8_t s_u8_dv8001_state_clean = 0;

static uint32_t s_u32_dv8001_chan_cfg = 0;

static uint8_t *s_p_u8_dv8001_chan_cfg = (uint8_t *)(&s_u32_dv8001_chan_cfg);

//最低位为1时是复位所有寄存器，所以不能最低位置为1
//对于短路报警，因为上位机每次启动出帧时都会配下来，所以生效的是上位机的配置。
static uint8_t s_dv8001_status_mask_config = (BIT(OFFSET_CHANL_SHORT_MSK) | BIT(OFFSET_UVLO60V_MSK)) & 0xFE;

static const char *s_string_dv8001_reg_status_flag[] = {
    "UVLO3V3_FLAG",   "UVLO60V_FLAG",     "LDO5_OCP_FLAG",  "OV60V_ERR_FLAG",
    "OVER_TEMP_FLAG", "CHANL_SHORT_FLAG", "EYE_SAFTY_FLAG", "LDO5_OK_FLAG",
};

static volatile uint8_t u8_dv8001_uvlo60_vth_update_flag = 1;

static uint8_t s_dv8001_is_config = 0;

static uint32_t s_dv8001_state_update_last_time = 0;

static uint8_t s_dv8001_chipid = 0;

static uint32_t s_chip1_cur_open_chn = 0;
static uint32_t s_chip2_cur_open_chn = 0;

// TODO JUST FOR DEBUG
// 公用一个enable的gpio无法分开
void dv8001_chip_enable(uint32_t flag) {
  switch (flag) {
  case DV8001_CHIP_ALL_DISABLE:
    //gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_RESET);
    s_dv8001_chip_enable_flag = DV8001_CHIP_ALL_DISABLE;
    break;
  case DV8001_CHIP_1EN_2DIS:
    //gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_SET);
    s_dv8001_chip_enable_flag = DV8001_CHIP_1EN_2DIS;
    break;
  case DV8001_CHIP_1DIS_2EN:
    //gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_SET);
    s_dv8001_chip_enable_flag = DV8001_CHIP_1DIS_2EN;
    break;
  default: // all enable
    //gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_SET);
    s_dv8001_chip_enable_flag = DV8001_CHIP_ALL_ENABLE;
    break;
  }
}

static inline void dv8001_err_interrupt_proc(uint8_t chip)
{
    //在没有config之前都忽略报警
    if(!s_dv8001_is_config){
      return;
    }
    if (pmic_get_state() != ADAPS_PMIC_POWER_UP_DONE){
        s_dv8001_chip_state[chip].u8_err_interrupt_flag++;
        return;
    }
    if(pmic_get_pvdd_vol() < DV8001_DRIVER_VOL_MIN){
        s_dv8001_chip_state[chip].u8_err_interrupt_flag++;
        return;
    }
    
    s_dv8001_chip_state[chip].u8_dv8001_err_state = 1;
}

static void dv8001_chip1_err_callback(void) {
    dv8001_err_interrupt_proc(en_dv8001_chip1);
}

static void dv8001_chip2_err_callback(void) {
    dv8001_err_interrupt_proc(en_dv8001_chip2);
}

void dv8001_reset(void)
{
  memset(&s_dv8001_chip_state, 0, sizeof(s_dv8001_chip_state));
  dv8001_single_write_data(DV8001_chip1_SPI_CHAN, OFFSET_STATUS_FLAG, 0xff);
  dv8001_single_write_data(DV8001_chip2_SPI_CHAN, OFFSET_STATUS_FLAG, 0xff);
  dv8001_state_update();
  //printf("shark state reset & update done\n");
}


void dv8001_config(void)
{
    //默认值为0xBE
    dv8001_single_write_data(DV8001_chip1_SPI_CHAN, OFFSET_STATUS_MSK, s_dv8001_status_mask_config);
    dv8001_single_write_data(DV8001_chip2_SPI_CHAN, OFFSET_STATUS_MSK, s_dv8001_status_mask_config);

    //默认值为0x0C
    dv8001_single_write_data(DV8001_chip1_SPI_CHAN, OFFSET_ANA_EN2, 0x0D);
    dv8001_single_write_data(DV8001_chip2_SPI_CHAN, OFFSET_ANA_EN2, 0x0D);

    //默认值为0x63
    dv8001_single_write_data(DV8001_chip1_SPI_CHAN, OFFSET_ANA_EN1, 0X6B);
    dv8001_single_write_data(DV8001_chip2_SPI_CHAN, OFFSET_ANA_EN1, 0X6B);
    printf("shark config done, status mask 0x%x\n", s_dv8001_status_mask_config);

    //shark02才配0x10寄存器
    if(dv8001_single_read_data(DV8001_chip1_SPI_CHAN, OFFSET_CHIP_ID) == DV8001_CHIPID_VERSION2){
      dv8001_single_write_data(DV8001_chip1_SPI_CHAN, OFFSET_TEST_EN, 0xFC);
    }

    if(dv8001_single_read_data(DV8001_chip2_SPI_CHAN, OFFSET_CHIP_ID) == DV8001_CHIPID_VERSION2){
      dv8001_single_write_data(DV8001_chip2_SPI_CHAN, OFFSET_TEST_EN, 0xFC);
    }

}

void dv8001_open(void)
{
    s_dv8001_is_config = 0;
    gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_RESET);
    delay_us(10);
    gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_SET);
    // 延时不能太短，否则通讯异常
    delay_us(500);
    dv8001_config();
    dv8001_reset();
    //因为shark默认30V以下会报警，配置完（设为18V报警阈值）之后才设置这个标志
    s_dv8001_is_config = 1;
    printf("shark open done\n");
}

void dv8001_close(void)
{
    s_dv8001_is_config = 0;
    gpio_write(S1_S2_EN_PORT, S1_S2_EN_PIN, GPIO_PIN_RESET);
}


void dv8001_init(void)
{
  memset(&s_dv8001_chip_state, 0, sizeof(s_dv8001_chip_state));
  gpio_interrupt_set_cb(S1_ERR_PORT, S1_ERR_PIN, dv8001_chip1_err_callback);
#ifndef HW_ONLY_ONE_ADS7311
  gpio_interrupt_set_cb(S2_ERR_PORT, S2_ERR_PIN, dv8001_chip2_err_callback);
#endif

}

uint32_t dv8001_set_mutli_channel(uint32_t channel_mask1, uint32_t channel_mask2, dv8001_action_t action_after_done) {
  uint8_t u8_set_chan_tx_buf[DV8001_TX_CONFIG_DATA_SIZE] = {OFFSET_CHANL_CFG0, 0, 0, 0, 0};
  uint8_t u8_set_chan_rx_buf[DV8001_TX_CONFIG_DATA_SIZE] = {0, 0, 0, 0, 0};
  uint8_t chip1_need_open_chn = ((channel_mask1) && (s_dv8001_chip_enable_flag & DV8001_CHIP_1EN_2DIS));
  
  // 1. 关闭上次通道, 即使这次是同一通道，也要上关闭再打开
  // 为了简化控制(不用记忆上次通道)，执行关闭所有通道
  // 因为利用spi通讯时间来延时，如果第一片芯片不用打开通道，则先关闭第二片的通道
  if(chip1_need_open_chn){
    dv8001_write_data(DV8001_chip1_SPI_CHAN, u8_set_chan_tx_buf,
                                u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
    dv8001_write_data(DV8001_chip2_SPI_CHAN, u8_set_chan_tx_buf,
                                u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
  }else{
    dv8001_write_data(DV8001_chip2_SPI_CHAN, u8_set_chan_tx_buf,
                                u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
    dv8001_write_data(DV8001_chip1_SPI_CHAN, u8_set_chan_tx_buf,
                                u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
  }

  // 适当延时来等待上次通道完全下电
  // 实际延时是含spi通讯消耗的时间
  delay_us(1);

  // 存在打开多个通道的情形，简化为配置所有通道
  if (chip1_need_open_chn) {
    memcpy((u8_set_chan_tx_buf + 1), &channel_mask1, sizeof(channel_mask1));
    dv8001_write_data(DV8001_chip1_SPI_CHAN, u8_set_chan_tx_buf,
                      u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
  }
  if ((channel_mask2) && (s_dv8001_chip_enable_flag & DV8001_CHIP_1DIS_2EN)) {
    memcpy((u8_set_chan_tx_buf + 1), &channel_mask2, sizeof(channel_mask2));
    dv8001_write_data(DV8001_chip2_SPI_CHAN, u8_set_chan_tx_buf,
                      u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
  }

  // 适当延时等待本次通道完全充电
  // 此延时应以最坏情况（最后配置那芯片的最后8个中的通道打开）考虑，故不考虑spi通讯耗时
  delay_us(10);

  if (action_after_done) {
    action_after_done();
  }

  dv8001_state_update();

  return 0;
}

uint32_t dv8001_set_channel(uint8_t channel_number,
                            dv8001_action_t action_after_done) {
  uint32_t channel_mask1;
  uint32_t channel_mask2;

  // channel从1开始到32
  if ((channel_number < 1) ||
      (channel_number > DV8001_CHAN_DISABLE_ALL)) {
    return 1;
  }

  //   s_cur_setting_chn = channel_number;
  if (channel_number <= DV8001_CHAN_USE_COUNT) {
    channel_number = channel_number - 1;
    //  10/19,应系统需求，改为两列相同通道同时发光，不错开
    channel_mask1 = 1 << channel_number;
    channel_mask2 = 1 << channel_number;
    if(s_dv8001_chip_enable_flag & DV8001_CHIP1_FLAG){
      s_chip1_cur_open_chn = channel_number;
    }else{
      s_chip1_cur_open_chn = DV8001_CHAN_DISABLE_ALL;
    }
    if(s_dv8001_chip_enable_flag & DV8001_CHIP2_FLAG){
      s_chip2_cur_open_chn = channel_number;
    }else{
      s_chip2_cur_open_chn = DV8001_CHAN_DISABLE_ALL;
    }
    //channel_mask2 = 1 << ((channel_number + CHANNEL_OFFSET) % 32);
  } else {
    channel_mask1 = 0;
    channel_mask2 = 0;
    s_chip1_cur_open_chn = DV8001_CHAN_DISABLE_ALL;
    s_chip2_cur_open_chn = DV8001_CHAN_DISABLE_ALL;
  }

  return dv8001_set_mutli_channel(channel_mask1, channel_mask2, action_after_done);
 
}

void dv8001_reconfigure_chn(void)
{
  uint8_t close_data[2];
  uint8_t open1_data[2];
  uint8_t open2_data[2];
  uint8_t read_data[2];
  uint32_t offset = 0;

  //先关断，再打开;
  if(s_chip1_cur_open_chn < 8){
    offset = 1;
    open1_data[1] = 1 << s_chip1_cur_open_chn;
  }else if(s_chip1_cur_open_chn < 16){
    offset = 2;
    open1_data[1] = 1 << (s_chip1_cur_open_chn - 8);
  }else if(s_chip1_cur_open_chn < 24){
    offset = 3;
    open1_data[1] = 1 << (s_chip1_cur_open_chn - 16);
  }else if(s_chip1_cur_open_chn < 32){
    offset = 4;
    open1_data[1] = 1 << (s_chip1_cur_open_chn - 24);
  }else{
    offset = 0;
    open1_data[1] = 0;
  }

  if(s_chip2_cur_open_chn < 8){
    offset = 1;
    open2_data[1] = 1 << s_chip2_cur_open_chn;
  }else if(s_chip2_cur_open_chn < 16){
    offset = 2;
    open2_data[1] = 1 << (s_chip2_cur_open_chn - 8);
  }else if(s_chip2_cur_open_chn < 24){
    offset = 3;
    open2_data[1] = 1 << (s_chip2_cur_open_chn - 16);
  }else if(s_chip2_cur_open_chn < 32){
    offset = 4;
    open2_data[1] = 1 << (s_chip2_cur_open_chn - 24);
  }else{
    offset = 0;
    open2_data[1] = 0;
  }

  close_data[0] = offset;
  close_data[1] = 0;
  dv8001_write_data(DV8001_chip1_SPI_CHAN, close_data, read_data, 2);
  dv8001_write_data(DV8001_chip2_SPI_CHAN, close_data, read_data, 2);
  open1_data[0] = offset;
  dv8001_write_data(DV8001_chip1_SPI_CHAN, open1_data, read_data, 2);
  open2_data[0] = offset;
  dv8001_write_data(DV8001_chip2_SPI_CHAN, open2_data, read_data, 2);
}


uint32_t dv8001_set_channelx_en(uint8_t channel_number) {
  uint32_t ret;
  ret = dv8001_set_channel(channel_number, NULL);
  dv8001_print_state();
  return ret;
}

int dv8001_all_channels_disable(void) {
  return dv8001_set_channel(DV8001_CHAN_DISABLE_ALL, NULL);
}

int dv8001_shutdown_all_channels(void) {
    uint8_t u8_set_chan_tx_buf[DV8001_TX_CONFIG_DATA_SIZE] = {OFFSET_CHANL_CFG0, 0, 0, 0, 0};
    uint8_t u8_set_chan_rx_buf[DV8001_TX_CONFIG_DATA_SIZE] = {0, 0, 0, 0, 0};
    dv8001_write_data(DV8001_chip1_SPI_CHAN, u8_set_chan_tx_buf,
                              u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
    dv8001_write_data(DV8001_chip2_SPI_CHAN, u8_set_chan_tx_buf,
                              u8_set_chan_rx_buf, DV8001_TX_CONFIG_DATA_SIZE);
    return 0;
}


void dv8001_print_state(void) {
    dv8001_chip_state_t dv8001_state[EN_DV8001_CHIP_COUNT];
    bsp_disable_irq();
    memcpy(dv8001_state, s_dv8001_chip_state, sizeof(dv8001_state));
    bsp_enable_irq();
    
  for (int i = 0; i < EN_DV8001_CHIP_COUNT; i++) {
    printf("dv8001 chip%d(id:0x%x) state:\r\n", i, dv8001_state[i].u8_dv8001_reg_state[0]);
    printf("%s\r\n",
           ((dv8001_state[i].u8_dv8001_err_state == 0) ? "no error!"
                                                            : "error!"));

    if(i == en_dv8001_chip1){
        printf("err pin state %d\n", bsp_gpio_read(S1_ERR_PORT, S1_ERR_PIN));
    }else{
        printf("err pin state %d\n", bsp_gpio_read(S2_ERR_PORT, S2_ERR_PIN));
    }

    printf("err interrupt flag %d\n", dv8001_state[i].u8_err_interrupt_flag);

    uint8_t u8_chan_open_flag = 0;
    uint8_t u8_chan_short_circuit_flag = 0;


    for (int k = OFFSET_CHANL_CFG0; k <= ARR_LENGTH_DV8001_REG; k++) {
      if (OFFSET_CHANL_CFG3 >= k) {
        if (0 != dv8001_state[i].u8_dv8001_reg_state[k]) {
          uint8_t u8_chan_open = dv8001_state[i].u8_dv8001_reg_state[k];
          for (int z = 0; z < 8; z++) {
            if (1 == (u8_chan_open & 0X1)) {
              printf("channel %d open\r\n", (k - 1) * 8 + z + 1);
            }
            u8_chan_open >>= 1;
          }
          u8_chan_open_flag = 1;
        }
      }
      else if ((OFFSET_CHANL_CFG3 < k) && (OFFSET_STATUS_FLAG > k)) {
        if (0 != dv8001_state[i].u8_dv8001_reg_state[k]) {
          uint8_t u8_chan_short_circuit = dv8001_state[i].u8_dv8001_reg_state[k];
          for (int z = 0; z < 8; z++) {
            if (1 == (u8_chan_short_circuit & 0X1)) {
              printf("channel %d short circuit\r\n", (k - 5) * 8 + z + 1);
            }
            u8_chan_short_circuit >>= 1;
          }
          u8_chan_short_circuit_flag = 1;
        }
      }
      else if (OFFSET_STATUS_FLAG == k) {
          if (0 != dv8001_state[i].u8_dv8001_reg_state[k]) {
            uint8_t u8_status_flag = dv8001_state[i].u8_dv8001_reg_state[k];
            // 3.3V低压报警位有效时是0，为1时是3.3v正常状态
            u8_status_flag &= 0xFE;
            for (int z = 0; z < 8; z++) {
              if (1 == (u8_status_flag & 0X1)) {
                printf("%s\r\n", s_string_dv8001_reg_status_flag[z]);
              }
              u8_status_flag >>= 1;
            }
          }
      }

    }
    if (0 == u8_chan_open_flag) {
      printf("no channel open\r\n");
    }

    if (0 == u8_chan_short_circuit_flag) {
      printf("no channel short circuit\r\n");
    }

  }

    printf("reg    ");
    for (int j = 0; j < ARR_LENGTH_DV8001_REG; j++) {
      printf("%02X ", j);
    }
    printf("\n");

  for (int i = 0; i < EN_DV8001_CHIP_COUNT; i++) {
    printf("chip%d  ", i);
    for (int j = 0; j < ARR_LENGTH_DV8001_REG; j++) {
      printf("%02X ", dv8001_state[i].u8_dv8001_reg_state[j]);
    }
    printf("\n");
  }

  printf("status mask config: 0x%x\n", s_dv8001_status_mask_config);
  
}

int dv8001_state_check(void) {
    int ret = 0;
    if (s_u8_dv8001_state_clean) {
        return 0;
    }

    if(pmic_get_pvdd_vol() < DV8001_DRIVER_VOL_MIN){
        u8_dv8001_uvlo60_vth_update_flag = 1;
        return 0;
    }

    for (int i = 0; i < EN_DV8001_CHIP_COUNT; i++) {
        if(s_dv8001_chip_state[i].u8_err_interrupt_flag){
            s_dv8001_chip_state[i].u8_err_interrupt_flag = 0;
            u8_dv8001_uvlo60_vth_update_flag = 1;
        }
    }

    if((s_dv8001_chip_state[en_dv8001_chip1].u8_dv8001_err_state)){
        if(bsp_gpio_read(S1_ERR_PORT, S1_ERR_PIN) == GPIO_PIN_RESET){
            s_dv8001_chip_state[en_dv8001_chip1].u8_dv8001_err_state = 0;
            u8_dv8001_uvlo60_vth_update_flag = 1;
        }else{
            ret  =1;
        }
    }

    if((s_dv8001_chip_state[en_dv8001_chip2].u8_dv8001_err_state)){
        if(bsp_gpio_read(S2_ERR_PORT, S2_ERR_PIN) == GPIO_PIN_RESET){
            s_dv8001_chip_state[en_dv8001_chip2].u8_dv8001_err_state = 0;
            u8_dv8001_uvlo60_vth_update_flag = 1;
        }else{
            ret  =1;
        }
    }

    return ret;
}

int dv8001_state_clean(void) {
  s_u8_dv8001_state_clean = 1;
  memset(&s_dv8001_chip_state, 0, sizeof(s_dv8001_chip_state));
  return 0;
}

int dv8001_state_get(uint8_t *chip0_reg_val, uint8_t *chip1_reg_val, uint8_t *chip0_err, uint8_t *chip1_err)
{
    dv8001_chip_state_t dv8001_state[EN_DV8001_CHIP_COUNT];
    bsp_disable_irq();
    memcpy(dv8001_state, s_dv8001_chip_state, sizeof(dv8001_state));
    bsp_enable_irq();
    if(chip0_reg_val){
        memcpy(chip0_reg_val, dv8001_state[0].u8_dv8001_reg_state, ARR_LENGTH_DV8001_REG);
    }
    if(chip1_reg_val){
        memcpy(chip1_reg_val, dv8001_state[1].u8_dv8001_reg_state, ARR_LENGTH_DV8001_REG);
    }
    if(chip0_err){
        *chip0_err = (bsp_gpio_read(S1_ERR_PORT, S1_ERR_PIN) == GPIO_PIN_SET ? 1 : 0);
    }

    if(chip1_err){
        *chip1_err = (bsp_gpio_read(S2_ERR_PORT, S2_ERR_PIN) == GPIO_PIN_SET ? 1 : 0);
    }

    return 0;
    
}


void dv8001_get_chip_err(uint8_t *chip0_err, uint8_t *chip1_err)
{
    if(chip0_err){
        *chip0_err = bsp_gpio_read(S1_ERR_PORT, S1_ERR_PIN) == GPIO_PIN_SET ? 1 : 0;
    }

    if(chip1_err){
        *chip1_err = bsp_gpio_read(S2_ERR_PORT, S2_ERR_PIN) == GPIO_PIN_SET ? 1 : 0;
    }
}

void dv8001_get_reg_val(uint8_t reg_addr, uint8_t *chip0_val, uint8_t *chip1_val)
{
    if(reg_addr > ARR_LENGTH_DV8001_REG){
        return;
    }
    if(chip0_val){
        *chip0_val = s_dv8001_chip_state[en_dv8001_chip1].u8_dv8001_reg_state[reg_addr];
    }

    if(chip1_val){
        *chip1_val = s_dv8001_chip_state[en_dv8001_chip2].u8_dv8001_reg_state[reg_addr];
    }

}



int dv8001_write_data(uint8_t chip_id, uint8_t *pdata_w, uint8_t *pdata_r, uint16_t size)
{
  return bsp_spi_write_and_read_data(chip_id, pdata_w, pdata_r, size);
}

int dv8001_read_data(uint8_t chip_id, uint8_t *pdata_w, uint8_t *pdata_r, uint16_t size)
{
  pdata_w[0] |= 0X80;
  return bsp_spi_write_and_read_data(chip_id, pdata_w, pdata_r, size);
}


int dv8001_single_write_data(uint8_t chip_id, uint8_t addr, uint8_t val)
{
  uint8_t pdata_w[2] = {addr, val};
  uint8_t pdata_r[2] = {0};
  uint16_t size = 2;
  return bsp_spi_write_and_read_data(chip_id, pdata_w, pdata_r, size);
}

uint8_t dv8001_single_read_data(uint8_t chip_id, uint8_t addr)
{
  uint8_t pdata_w[2] = {addr | 0X80};
  uint8_t pdata_r[2] = {0};
  uint16_t size = 2;
  bsp_spi_write_and_read_data(chip_id, pdata_w, pdata_r, size);
  return pdata_r[1];
}

int dv8001_state_update(void) 
{
  uint8_t u8_get_dv8001_state_tx_buf[ARR_LENGTH_DV8001_REG + 1] = {0};
  uint8_t u8_get_dv8001_state_rx_buf[ARR_LENGTH_DV8001_REG + 1] = {0};
  dv8001_read_data(DV8001_chip1_SPI_CHAN, u8_get_dv8001_state_tx_buf,
                   u8_get_dv8001_state_rx_buf, ARR_LENGTH_DV8001_REG + 1);
  memcpy(s_dv8001_chip_state[en_dv8001_chip1].u8_dv8001_reg_state,
         u8_get_dv8001_state_rx_buf + 1, ARR_LENGTH_DV8001_REG);
  memset(u8_get_dv8001_state_rx_buf, 0, sizeof(u8_get_dv8001_state_rx_buf));
  dv8001_read_data(DV8001_chip2_SPI_CHAN, u8_get_dv8001_state_tx_buf,
                   u8_get_dv8001_state_rx_buf, ARR_LENGTH_DV8001_REG + 1);
  memcpy(s_dv8001_chip_state[en_dv8001_chip2].u8_dv8001_reg_state,
         u8_get_dv8001_state_rx_buf + 1, ARR_LENGTH_DV8001_REG);
  s_u8_dv8001_state_clean = 0;
  s_dv8001_state_update_last_time = bsp_tick_get();
  return 0;
}

// app_run设置并复位shark芯片

int dv8001_uvlo60_vth_update(void)
{
  if (u8_dv8001_uvlo60_vth_update_flag){
    memset(&s_dv8001_chip_state, 0, sizeof(s_dv8001_chip_state));
    dv8001_single_write_data(DV8001_chip1_SPI_CHAN, OFFSET_STATUS_FLAG, 0XFF);
    dv8001_single_write_data(DV8001_chip2_SPI_CHAN, OFFSET_STATUS_FLAG, 0XFF);
    u8_dv8001_uvlo60_vth_update_flag = 0;
  }

  return 0;
}


void dv8001_notify_pvdd_changed(uint32_t pvdd_vol)
{
      u8_dv8001_uvlo60_vth_update_flag = 1;
}


uint32_t dv8001_get_state_update_time(void)
{
    return s_dv8001_state_update_last_time;
}

void dv8001_chn_short_protect_enable(uint32_t enable)
{
    if(enable){
        s_dv8001_status_mask_config &= (~(1 << OFFSET_CHANL_SHORT_MSK));
    }else{
        s_dv8001_status_mask_config |= (1 << OFFSET_CHANL_SHORT_MSK);
    }
    printf("dv8001_chn_short_protect_enable %d, status mask val: 0x%x\n", enable, s_dv8001_status_mask_config);
}

void dv8001_status_mask_config(uint32_t mask)
{
  s_dv8001_status_mask_config = mask & 0xFE;
}


