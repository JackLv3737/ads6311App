/**
 * @file system_config.h
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef __SYSTEM_CONFIG_H__
#define __SYSTEM_CONFIG_H__

//#define MODE_1D5_ENABLE

//打开此宏：禁止SOC控制TX/RX上下电
//打开此宏需要同时打开PMIC_DEFAULT_POWERUP宏
//#define DISABLE_POWER_CTRL

//打开此宏：MCU启动时默认TX/RX上电
//#define PMIC_DEFAULT_POWERUP

#define HEARTBEAT_ENABLE_DEFAULT
//打开此宏：心跳超时后TX/RX下电
//#define HEARTBEAT_TIMEOUT_POWERDOWN

//打开此宏：使能TX通道定时关断功能
#define TXSWT_TIMER_ENABLE

#define ERR_PROC_ENABLE

#define ERR_DISABLE_MASK_DEFAULT   0xFFFF

#define DV11_VOL_LIMIT_HIGH   1400 //unit:mv
#define DV11_VOL_LIMIT_LOW   800 //unit:mv
#define SV11_VOL_LIMIT_HIGH   1400 //unit:mv
#define SV11_VOL_LIMIT_LOW   800 //unit:mv

#define VOP_ERR_WARN_LIMIT  200 //unit:mv
#define PVDD_ERR_FAULT_LIMIT 2000 //unit:mv
#define PVDD_ERR_WARN_LIMIT 300 //unit:mv

//TX通道定时关断最小时间
#define TXSWT_CHN_OFF_TIMER_MIN_TIME   1150 //unit:us



#if defined(DISABLE_POWER_CTRL) && (!defined(PMIC_DEFAULT_POWERUP))
#error should define PMIC_DEFAULT_POWERUP if PMIC_DEFAULT_POWERUP is defined
#endif

#if defined(DISABLE_POWER_CTRL) && defined(HEARTBEAT_ENABLE_DEFAULT) && defined(HEARTBEAT_TIMEOUT_POWERDOWN)
#error should NOT define DISABLE_POWER_CTRL if HEARTBEAT_ENABLE_DEFAULT and HEARTBEAT_TIMEOUT_POWERDOWN are both defined
#endif


#endif //__SYSTEM_CONFIG_H__
