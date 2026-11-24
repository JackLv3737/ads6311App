#ifndef _ADAPS_DV8001_H
#define _ADAPS_DV8001_H

#include <stdint.h>

#define DV8001_CHAN_USE_COUNT 32 //(28)

#define DV8001_CHIP1_FLAG         1
#define DV8001_CHIP2_FLAG         2

#define DV8001_CHIP_ALL_DISABLE   0 // 0:all disable
#define DV8001_CHIP_1EN_2DIS                                                  \
  DV8001_CHIP1_FLAG // 1:chain1 enable, chain2 disable
#define DV8001_CHIP_1DIS_2EN                                                  \
  DV8001_CHIP2_FLAG // 2:chain1 disable, chain2 enable
#define DV8001_CHIP_ALL_ENABLE                                                \
  (DV8001_CHIP1_FLAG | DV8001_CHIP2_FLAG) // 3:all enable
	
#define DV8001_DRIVER_VOL_MIN   9000//unit:mv

typedef enum en_dv8001_reg_addr {
  OFFSET_CHIP_ID = 0x00,
  OFFSET_CHANL_CFG0 = 0x01,
  OFFSET_CHANL_CFG1,
  OFFSET_CHANL_CFG2,
  OFFSET_CHANL_CFG3,
  OFFSET_CHANL_ERR0 = 0x05,
  OFFSET_CHANL_ERR1,
  OFFSET_CHANL_ERR2,
  OFFSET_CHANL_ERR3,
  OFFSET_STATUS_FLAG = 0x09,
  OFFSET_STATUS_MSK,
  OFFSET_CHANL_MAX_TIME,
  OFFSET_ANA_EN1,
  OFFSET_ANA_EN2,
  OFFSET_DIG_TEST,
  OFFSET_ANA_TEST,
  OFFSET_TEST_EN,

  ARR_LENGTH_DV8001_REG,

} en_dv8001_reg_addr_t;

typedef enum en_dv8001_reg_status_flag {
  OFFSET_UVLO3V3_FLAG = 0X0,
  OFFSET_UVLO60V_FLAG,
  OFFSET_LDO5_OCP_FLAG,
  OFFSET_OV60V_ERR_FLAG,
  OFFSET_OVER_TEMP_FLAG,
  OFFSET_CHANL_SHORT_FLAG,
  OFFSET_EYE_SAFTY_FLAG,
  OFFSET_LDO5_OK_FLAG,

  ARR_LENGTH_DV8001_REG_STATUS_FLAG,
} en_dv8001_reg_status_flag_t;

#define DV8001_STATE_ERR_MASK 0xFE


typedef void (*dv8001_action_t)(void);
void dv8001_init(void);
void dv8001_open(void);
void dv8001_close(void);
void dv8001_config(void);
void dv8001_reset(void);

void dv8001_reconfigure_chn(void);

void dv8001_chip_enable(uint32_t flag);

uint32_t dv8001_set_channelx_en(uint8_t channel_number);
uint32_t dv8001_set_channel(uint8_t channel_number, dv8001_action_t action_after_done);
uint32_t dv8001_set_mutli_channel(uint32_t channel_mask1, uint32_t channel_mask2, dv8001_action_t action_after_done);
int dv8001_all_channels_disable(void);
int dv8001_shutdown_all_channels(void);

void dv8001_print_state(void);
int dv8001_state_check(void);
int dv8001_state_clean(void);

int dv8001_write_data(uint8_t chip_id, uint8_t *pdata_w, uint8_t *pdata_r, uint16_t size);
int dv8001_read_data(uint8_t chip_id, uint8_t *pdata_w, uint8_t *pdata_r, uint16_t size);
int dv8001_single_write_data(uint8_t chip_id, uint8_t addr, uint8_t val);
uint8_t dv8001_single_read_data(uint8_t chip_id, uint8_t addr);

void dv8001_chain_enable(uint32_t flag);
int dv8001_state_update(void);
int dv8001_uvlo60_vth_update(void);

void dv8001_notify_pvdd_changed(uint32_t pvdd_vol);

int dv8001_state_get(uint8_t *chip0_reg_val, uint8_t *chip1_reg_val, uint8_t *chip0_err, uint8_t *chip1_err);
void dv8001_get_chip_err(uint8_t *chip0_err, uint8_t *chip1_err);
void dv8001_get_reg_val(uint8_t reg_addr, uint8_t *chip0_val, uint8_t *chip1_val);

uint32_t dv8001_get_state_update_time(void);

void dv8001_chn_short_protect_enable(uint32_t enable);

void dv8001_status_mask_config(uint32_t mask);


#endif /*_ADAPS_DV8001_H*/
