#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "usmart.h"
#include "usmart_str.h"
#include "board_config.h"
#include "bsp_pwm.h"
#include "bsp_dac.h"
#include "bsp_basic.h"
#include "adaps_soc_protocol.h"
#include "adaps_pid.h"
#include "adaps_pmic_driver.h"
#include "adaps_tx_switch.h"
#include "adaps_dv8001_driver.h"


extern void printenv(void);
extern void sys_enter_abnormal_state(uint16_t cause_err);
extern void pmic_powerdown_all(void (*power_ctrl_cb)(void));
extern void pmic_powerup_all(void (*power_ctrl_cb)(void));
extern void txswt_test(void);

extern int bsp_dac_set_value(uint16_t val);

extern void bsp_telm_state_print(void);

extern void pwm_set_vop_duty(uint32_t duty);
extern void pwm_set_pvdd_duty(uint32_t duty);

extern void vop_pid_state_print(void);
extern void pvdd_pid_state_print(void);

extern void vop_pid_enable(uint32_t enable);
extern void pvdd_pid_enable(uint32_t enable);

extern void bsp_timer_oneshot_test(unsigned int timeout_us);

extern int pvdd_calibration(uint32_t target_vol_mv);
extern void pvdd_adjust_by_vol(uint32_t target_vol_mv, uint32_t check_interval_ms, uint32_t check_time_ms);
extern void pvdd_adjust_by_duty(uint32_t target_duty, uint32_t check_interval_ms, uint32_t check_time_ms);
extern void pvdd2_adjust_by_duty(uint32_t target_duty, uint32_t check_interval_ms, uint32_t check_time_ms);

extern void bsp_gpio_test(int test_write);
extern void bsp_adc_test(uint32_t times);

extern void adaps_flash_chipid(void);
extern void adaps_flash_test(void);
extern int adaps_flash_test_02(void);
extern void imu_test(void);

extern void pvdd_switch_by_gpio(uint32_t flag);

void dump_option(void);

static void wrap_pmic_powerdown_all(void)
{
    pmic_powerdown_all(NULL);
}

static void wrap_pmic_powerup_all(void)
{
    pmic_powerup_all(NULL);
}

static void wrap_switch_pvdd(uint32_t flag, uint32_t time_us)
{
    switch(flag){
        case 0:
            pvdd_switch_by_gpio(0);
        break;
        case 1:
            pvdd_switch_by_gpio(0);
            bsp_delay_us(time_us);
            pvdd_switch_by_gpio(1);
        break;
        case 2:
            pvdd_switch_by_gpio(0);
            bsp_delay_us(time_us);
            pvdd_switch_by_gpio(2);
        break;
    }
}

static struct _m_usmart_nametab usmart_nametab[] =
{
#if USMART_USE_WRFUNS == 1     
    (void *)read_addr, "uint32_t read_addr(uint32_t addr)",
    (void *)write_addr, "void write_addr(uint32_t addr, uint32_t val)",
#endif
    (void *)bsp_reboot,"reboot()",
    (void *)bsp_delay_ms, "void bsp_delay_ms(uint16_t nms)",
    (void *)bsp_delay_us, "void bsp_delay_us(uint32_t nus)",
    (void *)set_target_pvdd, "void set_target_pvdd(uint32_t val)",
    (void *)set_target_VOP, "void set_target_VOP(int32_t val)",
    (void *)reg_dump, "void reg_dump()",
    (void *)printenv, "void printenv()",
    (void *)dump_option, "void dump_option()",
    (void *)wrap_pmic_powerdown_all, "void pmic_powerdown_all()",
    (void *)wrap_pmic_powerup_all,"void pmic_powerup_all()",
    (void *)txswt_print_state,"void txswt_print_state()",
    (void *)txswt_enable,"txswt_enable()",
    (void *)txswt_disable,"txswt_disable()",
    (void *)txswt_test,"txswt_test()",
    (void *)txswt_set_hit_chn,"void txswt_set_hit_chn(uint32_t hit_chn)",
    (void *)txswt_set_hit_two_chn,"void txswt_set_hit_two_chn(uint32_t chn1, uint32_t chn2)",
    (void *)txswt_set_hit_multi_chn,"void txswt_set_hit_multi_chn(uint32_t chn_mask)",
    (void *)txswt_set_fixed_chn,"void txswt_set_fixed_chn(uint32_t fixed_chn)",
    (void *)txswt_set_fixed_chn2,"void txswt_set_fixed_chn2(uint32_t fixed_chn)",
    (void *)txswt_set_fixed_chn3,"void txswt_set_fixed_chn3(uint32_t fixed_chn)",
    (void *)txswt_set_fixed_chn4,"void txswt_set_fixed_chn4(uint32_t fixed_chn)",
    (void *)txswt_config_fixed_chn, "void txswt_config_fixed_chn(uint32_t index, uint32_t fixed_chn)",
    (void *)txswt_set_vroll_offset,"void txswt_set_vroll_offset(uint32_t offset)",
    (void *)set_dbgflag, "void set_dbgflag(uint16_t val)",
    (void *)bsp_dac_set_value, "int bsp_dac_set_value(uint16_t val)",
    (void *)dv8001_print_state, "void dv8001_print_state(void)",
    (void *)dv8001_set_channelx_en, "uint32_t dv8001_set_channelx_en(uint8_t channel_number)",
    (void *)dv8001_reset, "void dv8001_reset(void)",

    (void *)dv8001_single_write_data, "int dv8001_single_write_data(uint8_t chip_id, uint8_t addr, uint8_t val)",
    (void *)dv8001_single_read_data, "uint8_t dv8001_single_read_data(uint8_t chip_id, uint8_t addr)",
    (void *)dv8001_state_update, "int dv8001_state_update(void)",
    (void *)dv8001_status_mask_config, "void dv8001_status_mask_config(uint32_t mask)",
    (void *)pwm_set_vop_duty, "void pwm_set_vop_duty(uint32_t duty)",
    (void *)pwm_set_pvdd_duty,"void pwm_set_pvdd_duty(uint32_t duty)",

    (void *)reg_control_set, "void reg_control_set(uint16_t index, int16_t val)",

    (void *)dv8001_chip_enable, "void dv8001_chip_enable(uint32_t flag)",
    (void *)bsp_telm_state_print, "void bsp_telm_state_print(void)",
    (void *)vop_pid_state_print, "void vop_pid_state_print(void)",
    (void *)pvdd_pid_state_print, "void pvdd_pid_state_print(void)",
    (void *)vop_pid_enable, "void vop_pid_enable(uint32_t enable)",
    (void *)pvdd_pid_enable, "void pvdd_pid_enable(uint32_t enable)",
    (void *)bsp_timer_oneshot_test, "void bsp_timer_oneshot_test(unsigned int timeout_us)",
    (void *)txswt_timer_enable, "void txswt_timer_enable(uint32_t enable)",
    (void *)txswt_set_rx_expo_time1, "int txswt_set_rx_expo_time1(uint32_t time_us)",
    (void *)txswt_set_rx_expo_time2, "int txswt_set_rx_expo_time2(uint32_t time_us)",
    (void *)pvdd_calibration, "int pvdd_calibration(uint32_t target_vol_mv)",
    (void *)pvdd_adjust_by_vol, "void pvdd_adjust_by_vol(uint32_t target_vol_mv, uint32_t check_interval_ms, uint32_t check_time_ms)",
    (void *)pvdd_adjust_by_duty, "void pvdd_adjust_by_duty(uint32_t target_duty, uint32_t check_interval_ms, uint32_t check_time_ms)",
    (void *)pvdd2_adjust_by_duty, "void pvdd2_adjust_by_duty(uint32_t target_duty, uint32_t check_interval_ms, uint32_t check_time_ms)",
    (void *)sys_enter_abnormal_state, "void sys_enter_abnormal_state(uint16_t cause_err)",
    (void *)adaps_flash_chipid, "void adaps_flash_chipid(void)",
    (void *)adaps_flash_test, "void adaps_flash_test(void)",
//    (void *)adaps_flash_test_02, "int adaps_flash_test_02(void)",
    (void *)bsp_gpio_test, "void bsp_gpio_test(int test_write)",
    (void *)bsp_adc_test, "void bsp_adc_test(uint32_t times)",
    (void *)wrap_switch_pvdd, "void switch_pvdd(uint32_t flag, uint32_t time_us)",
    (void *)txswt_chn_reconfig_enable, "void txswt_chn_reconfig_enable(uint32_t enable)",
#ifdef HW_HOST_BMI_CHIP
    (void *)imu_test, "void imu_test(void)",
#endif
};

/******************************************************************************************/

/* func control manager init
 * state func name & nums
 */
struct _m_usmart_dev usmart_dev =
{
    usmart_nametab,
    usmart_init,
    usmart_cmd_rec,
    usmart_exe,
    usmart_scan,
    sizeof(usmart_nametab) / sizeof(struct _m_usmart_nametab), /*num of funcs */
    0,      /*params num */
    0,      /* func ID */
    1,      /* params display format,0,dec;1,hex */
    0,      /* params type.bitx:,0,num;1,str */
    0,      /* each param storage length, need MAX_PARM init */
    0,      /* func params,need PARM_LEN init */
};



















