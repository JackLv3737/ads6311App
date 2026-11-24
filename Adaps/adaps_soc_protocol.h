/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_pmic_driver.h
* Version      : 1.0.0
* Description  : head file for adaps_soc_protocol.c
* Creation Date: 2023/05/30
* Author       : wang.yi
***********************************************************************************************************************/
#ifndef ADAPS_PROTOCOL_SOC_H
#define ADAPS_PROTOCOL_SOC_H

#include <stdint.h>
#include "common.h"

enum adaps_dbg_type_t {
    ADAPS_DBG_TRACE_REGS_READ       = BIT(0),
    ADAPS_DBG_TRACE_REGS_WRITE      = BIT(1),
    ADAPS_DBG_TRACE_VOLTAGE_UPDATE  = BIT(2),
    ADAPS_DBG_TRACE_CURRENT_UPDATE  = BIT(3),
    ADAPS_DBG_TRACE_VOLTAGE_CTRL    = BIT(4),
    ADAPS_DBG_TRACE_TEMPERATURE_READ= BIT(5),
    ADAPS_DBG_TRACE_INA_OUT_UPDATE  = BIT(6),

};

enum adaps_run_state_t {
    ADAPS_UNKNOWN_STATE             = 0,
    ADAPS_NORMAL_STATE              = 1,
    ADAPS_PROTECT_STATE             = 2,
};

enum adaps_error_type_t {
    ADAPS_PVDD_PEAK_CURRENT_OVER_ERR= BIT(0),
    ADAPS_TX_SWITCH_ERR             = BIT(1),
    ADAPS_PVDD_VOLTAGE_ERR          = BIT(2),
    ADAPS_VOP_VOLTAGE_ERR           = BIT(3),
    ADAPS_DV11_VOLTAGE_ERR          = BIT(4),
    ADAPS_SV11_VOLTAGE_ERR          = BIT(5),
    ADAPS_ERR_ALL = 0xFFFF
};

enum adaps_warn_type_t {
    ADAPS_PVDD_AVRG_CURRENT_OVER_WARN       = BIT(0),
    ADAPS_PVDD_VOLTAGE_WARN                 = BIT(1),
    ADAPS_VOP_VOLTAGE_WARN                  = BIT(2),
};

enum adaps_rx_work_mode_t{
    ADAPS_RX_ECHO_MODE = 0,
    ADAPS_RX_ECHO_COMBINED_IN_CHIP_MODE,
    ADAPS_RX_FHR_MODE,
    ADAPS_RX_FHR_COMBINED_IN_CHIP_MODE,
    ADAPS_RX_RANGE_MODE,
    ADAPS_RX_RANGE_COMBINED_IN_CHIP_MODE,
    ADAPS_RX_PCM_MODE,
};

enum adaps_cali_pvdd_cmd_t
{
    ADAPS_CALI_PVDD_RESET = 0x1111,
    ADAPS_CALI_PVDD_START = 0x2222,
    ADAPS_CALI_PVDD_DELETE = 0x5555,
};

enum adaps_cali_pvdd_state_t
{
    ADAPS_CALI_PVDD_STATE_UNKOWN = 0,
    ADAPS_CALI_PVDD_STATE_ING = 1,
    ADAPS_CALI_PVDD_STATE_SUCCESS = 2,
    ADAPS_CALI_PVDD_STATE_FAIL = 3,
    ADAPS_CALI_PVDD_STATE_DELETE_FAIL = 4,
};

enum adaps_cali_pvdd_errcode_t
{
    ADAPS_CALI_PVDD_ERR_NONE = 0,
    ADAPS_CALI_PVDD_ERR_COMMON = (1 << 8),
    ADAPS_CALI_PVDD_ERR_ILLEGAL_CMD = (2 << 8),
    ADAPS_CALI_PVDD_ERR_ILLEGAL_VOL = (3 << 8),
    ADAPS_CALI_PVDD_ERR_VOL_UNSTABLE = (4 << 8),
    ADAPS_CALI_PVDD_ERR_TIMEOUT = (5 << 8),
    ADAPS_CALI_PVDD_ERR_SAVE_FAIL = (6 << 8),
    
};

enum adaps_reg_flag_e
{
    REG_FLAG_CTRL = BIT(0),
    REG_FLAG_UPDATE = BIT(1),
    REG_FLAG_ALL    = 0xFF
};


typedef enum
{
  POWER_DISABLE = 0,
  POWER_ENABLE = 1,
}POWER_GPIO_PinState;

typedef enum
{
  FLASH_STATE_IDLE = 0,
  FLASH_STATE_BUSY = 1,
  FLASH_STATE_READY = 2,
  FLASH_STATE_DATA_ERR = 3,
  FLASH_STATE_OP_ERR = 4,
}FlashState_e;

typedef enum en_adaps_hawk_dvk_uc930_mcu_reg_addr {
  STATUS_REG_BASE = 0x0,
  OFFSET_DEV_ID = 0x0,
  OFFSET_TEMP_A = 0x1,
  OFFSET_TEMP_B = 0x2,
  OFFSET_VOL_DV11 = 0x3,
  OFFSET_VOL_SV11 = 0x4,
  OFFSET_CURRENT_PVDD = 0x5,
  OFFSET_ERR_CODE = 0x6,
  OFFSET_FLASH_OP_STATE = 0x07,
  OFFSET_FLASH_OP_SIZE_MAX = 0x08,
  OFFSET_WARN_CODE = 0x9,
  OFFSET_PMIC_STATE = 0x0A,
  OFFSET_RUN_STATE = 0x0B,

  OFFSET_TEMP_RX_BOARD = 0x0C,
  OFFSET_TEMP_MCU_CHIP = 0x0D,

  OFFSET_BOOT_OPTION_SET1 = 0x0E,
  OFFSET_BOOT_OPTION_SET2 = 0x0F,

  OFFSET_FIRMWARE_VER = 0x10,
  OFFSET_HARDWARE_VER = 0x11,
  OFFSET_FW_VERSION_EXTRA = 0x12,
  OFFSET_HW_VARIANT_VER = 0x13,

  OFFSET_CALI_PVDD_STATE = 0x14,
  OFFSET_CALI_PVDD_SAVED_VOL = 0x15,
  OFFSET_CALI_PVDD_SAVED_DUTY = 0x16,
  OFFSET_CALI_PVDD_CALC_VOL = 0x17, //理论计算的电压值，单位:10mv

  OFFSET_VOP_VOL_ABS = 0x18, //单位为10mv
  OFFSET_PVDD_VOL = 0x19, //单位为10mv
  OFFSET_VREF_VOL = 0x1A, //单位为mv
  OFFSET_PVDDL_VOL = 0x1B, //单位为10mv
  OFFSET_PVDD_ICT_VOL = 0x1C, //单位为10mv

  OFFSET_BOARD_TAG = 0x1E,
  OFFSET_TELM_METHOD = 0x1F,
  
  OFFSET_APPLICATION_TYPE_U16 = 0x20, // APP:= 0XA, BOOTLOADER:= 0XB
  OFFSET_UPGRADE_PROGRAM_FLAG_U16,

  OFFSET_CHECKSUM_31_16_U16,
  OFFSET_CHECKSUM_15_0_U16,
  OFFSET_CHECKSUM_STATE_U16,

  OFFSET_LENGTH_31_16_U16,
  OFFSET_LENGTH_15_0_U16,
  OFFSET_LENGTH_STATE_U16,

  OFFSET_SINGLE_W_FLASH_LENGTH_31_16_U16,
  OFFSET_SINGLE_W_FLASH_LENGTH_15_0_U16,
  OFFSET_SINGLE_W_FLASH_LENGTH_STATE_U16,

  OFFSET_UPDATE_APP_STATE_U16,

  OFFSET_UPDATE_ALL_APP_STATE_U16,

  OFFSET_SYS_SOFT_RESET_U16,

  OFFSET_MCU_BOOTLOADER_STATE_REG_ADDR_U16,

  OFFSET_MCU_BOOTLOADER_ERROR_REG_ADDR_U16,

  OFFSET_MCU_SPI_CHECKSUM_ERROR_REG_ADDR_U16,

  OFFSET_APP_UPGRADE_FLAG_31_16_U16,
  OFFSET_APP_UPGRADE_FLAG_15_0_U16,
  OFFSET_APP_UPGRADE_FLAG_STATE_U16,

  OFFSET_DEBUG_MSG_FUNC_REG_ADDR_U16, // 0X0:不输出debug信息 0X1:输出debug信息
  OFFSET_DEBUG_MSG_FUNC_STATE_REG_ADDR_U16,

  OFFSET_BOOTLOADER_STATE_U16,

  OFFSET_LENGTH_MCU_BOOTLOADER_REG_ADDR_U16,


  OFFSET_CALI_PVDDL_STATE = 0x40,
  OFFSET_CALI_PVDDL_SAVED_VOL = 0x41,
  OFFSET_CALI_PVDDL_SAVED_DUTY = 0x42,
  OFFSET_CALI_PVDDL_CALC_VOL = 0x43, //理论计算的电压值，单位:10mv

  OFFSET_SHARK_ERR_PIN_STATE = 0x4F,
  OFFSET_SHARK_REG_00 = 0x50,
  OFFSET_SHARK_REG_01,
  OFFSET_SHARK_REG_02,
  OFFSET_SHARK_REG_03,
  OFFSET_SHARK_REG_04,
  OFFSET_SHARK_REG_05,
  OFFSET_SHARK_REG_06,
  OFFSET_SHARK_REG_07,
  OFFSET_SHARK_REG_08,
  OFFSET_SHARK_REG_09,
  OFFSET_SHARK_REG_0A,
  OFFSET_SHARK_REG_0B,
  OFFSET_SHARK_REG_0C,
  OFFSET_SHARK_REG_0D,
  OFFSET_SHARK_REG_0E,
  OFFSET_SHARK_REG_0F,
  OFFSET_SHARK_REG_10 = 0x60,

  CONTROL_REG_BASE = 0x80,
  OFFSET_VOL_PVDD = 0x80, //单位：10mv
  OFFSET_VOL_VOP = 0x81, //负值，单位：10mv
  OFFSET_DEBUG = 0x82,
  OFFSET_VCSEL_ENABLE = 0x88,

  OFFSET_RX_WORK_MODE   = 0x8A,

  OFFSET_ERR_ACK = 0x8E,
  OFFSET_ERR_DISABLE_MASK = 0x8F,

  OFFSET_FLASH_OP_OFFSET1 = 0x90,
  OFFSET_FLASH_OP_OFFSET2 = 0x91,
  OFFSET_FLASH_OP_CMD = 0x92,
  OFFSET_DATA_TRANS_CMD = 0x93,

  OFFSET_CALI_PVDD_TARGET_VOL = 0x98,
  OFFSET_CALI_PVDD_CMD = 0x99,

  OFFSET_IMU_WORK_CTRL = 0x9C,
  OFFSET_IMU_SAMPLE_PERIOD = 0x9D,
  OFFSET_VROLL_NUM = 0x9F,

  OFFSET_TX_CHN_SHORT_ENABLE = 0xA0,
  OFFSET_TX_CHN_RESET_PERIODLY_EN = 0xA1,

  OFFSET_SENSOR_STREAM_STATE = 0xA4,
  OFFSET_PVDD_4_WEAK_LASER = 0xA5,
  OFFSET_MAX_SUBFRAME_EXP_TIME = 0xA6,
  OFFSET_NUM_SUBFRAME = 0xA7,
  OFFSET_MAX_WEAK_SUBFRAME_EXP_TIME = 0xA8,
  OFFSET_VROLL_OFFSET   = 0xA9,
  OFFSET_BMI_EN   = 0xAA,
  OFFSET_ADS6311_I2C_EN   = 0xAB,
  OFFSET_ADS6311_EN   = 0xAC,

  OFFSET_PVDDL_EN   = 0xAF,
  OFFSET_DV11 = 0xB0,
  OFFSET_SV11 = 0xB1,
  OFFSET_AV11 = 0xB2,
  OFFSET_MP_VCC11A = 0xB3,
  OFFSET_IOV18 = 0xB4,
  OFFSET_MP_VCC18A = 0xB5,
  OFFSET_IOV33_DRV = 0xB6,
  OFFSET_IOV33_TRIG = 0xB7,
  OFFSET_AV33 = 0xB8,
  OFFSET_SV33 = 0xB9,
  OFFSET_VCC_3V3 = 0xBA,
  OFFSET_VOP = 0xBB,
  OFFSET_PVDD = 0xBC,
  OFFSET_POWER_CTRL_ALL = 0xBD,
  OFFSET_HEART_BEAT_SWITCH = 0xBE,
  OFFSET_VCC_5V_DRV = 0xBF,

  OFFSET_TX_FIXED_CHN9 = 0xC0,
  OFFSET_TX_FIXED_CHN10 = 0xC1,
  OFFSET_TX_FIXED_CHN11 = 0xC2,
  OFFSET_TX_FIXED_CHN12 = 0xC3,
  OFFSET_TX_FIXED_CHN13 = 0xC4,
  OFFSET_TX_FIXED_CHN14 = 0xC5,
  OFFSET_TX_FIXED_CHN15 = 0xC6,
  OFFSET_TX_FIXED_CHN16 = 0xC7,

    OFFSET_EXPO_TIME_CH01 = 0xD0,
    OFFSET_EXPO_TIME_CH02,
    OFFSET_EXPO_TIME_CH03,
    OFFSET_EXPO_TIME_CH04,
    OFFSET_EXPO_TIME_CH05,
    OFFSET_EXPO_TIME_CH06,
    OFFSET_EXPO_TIME_CH07,
    OFFSET_EXPO_TIME_CH08,
    OFFSET_EXPO_TIME_CH09,
    OFFSET_EXPO_TIME_CH10,
    OFFSET_EXPO_TIME_CH11,
    OFFSET_EXPO_TIME_CH12,
    OFFSET_EXPO_TIME_CH13,
    OFFSET_EXPO_TIME_CH14,
    OFFSET_EXPO_TIME_CH15,
    OFFSET_EXPO_TIME_CH16,
    OFFSET_EXPO_TIME_CH17 = 0xE0,
    OFFSET_EXPO_TIME_CH18,
    OFFSET_EXPO_TIME_CH19,
    OFFSET_EXPO_TIME_CH20,
    OFFSET_EXPO_TIME_CH21,
    OFFSET_EXPO_TIME_CH22,
    OFFSET_EXPO_TIME_CH23,
    OFFSET_EXPO_TIME_CH24,
    OFFSET_EXPO_TIME_CH25,
    OFFSET_EXPO_TIME_CH26,
    OFFSET_EXPO_TIME_CH27,
    OFFSET_EXPO_TIME_CH28,
    OFFSET_EXPO_TIME_CH29,
    OFFSET_EXPO_TIME_CH30,
    OFFSET_EXPO_TIME_CH31,
    OFFSET_EXPO_TIME_CH32 = 0xEF,

  OFFSET_TX_FIXED_CHN = 0xF0, // for roi cali
  OFFSET_TX_SIDE_ENABLE = 0xF1,
  OFFSET_TX_HIT_CHANNEL = 0xF2,
  OFFSET_TX_FIXED_COLUMN = 0xF3, //for 2d roi cali
  OFFSET_TX_HIT_COLUMN = 0xF4,
  OFFSET_TX_FIXED_ZONE = 0xF5,
  OFFSET_TX_HIT_ZONE = 0xF6,
  OFFSET_FIXED_PVDD_CHN = 0xF7,
  OFFSET_TX_FIXED_CHN2 = 0xF8,
  OFFSET_TX_FIXED_CHN3 = 0xF9,
  OFFSET_TX_FIXED_CHN4 = 0xFA,
  OFFSET_TX_FIXED_CHN5 = 0xFB,
  OFFSET_TX_FIXED_CHN6 = 0xFC,
  OFFSET_TX_FIXED_CHN7 = 0xFD,
  OFFSET_TX_FIXED_CHN8 = 0xFE,

  REG_MAX = 0xFF,
} en_adaps_hawk_dvk_uc930_mcu_reg_addr_t;

typedef enum en_adaps_hawk_mcu_bootloader_upgrade_program_flag {

    UPGRADE_PROGRAM_FLAG_NO_UPDATE = 0XA,
    UPGRADE_PROGRAM_FLAG_UPDATE = 0XB,
} en_adaps_hawk_mcu_bootloader_upgrade_program_flag_t;

typedef enum en_adaps_hawk_mcu_bootloader_application_type {

    APPLICATION_TYPE_APP = 0XA,
    APPLICATION_TYPE_BOOTLOADER = 0XB,
} en_adaps_hawk_mcu_bootloader_application_type_t;

void reg_control_set(uint16_t index, int16_t val);
void reg_status_update(uint16_t index, int16_t val);
void reg_err_code_update(uint16_t errcode, uint16_t is_set);
void reg_warn_code_update(uint16_t warncode, uint16_t is_set);
int16_t reg_val_get(uint16_t index);
void reg_dump(void);
uint16_t get_dbgflag(void);
void set_dbgflag(uint16_t val);
void reg_set_val(uint16_t index, int16_t val);
void reg_set_val_not_set_flag(uint16_t index, int16_t val);


uint8_t reg_get_flag(uint16_t index);
uint8_t reg_reset_flag(uint16_t index, uint8_t mask);

#endif
