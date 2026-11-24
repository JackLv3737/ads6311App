/**
 * @file tx_app.c
 * @author rongchao
 * @brief
 * @version 0.1
 * @date 2023-08-08
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "board_config.h"
#include "system_config.h"
#include "bsp_gpio.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_adc.h"
#include "bsp_dac.h"
#include "bsp_pwm.h"
#include "bsp_timer.h"
#include "bsp_telematic.h"
#include "bsp_clkout.h"
#include "adaps_pid.h"
#include "adaps_dv8001_driver.h"
#include "adaps_pmic_driver.h"
#include "adaps_nst112_driver.h"
#include "adaps_soc_protocol.h"
#include "adaps_ina201_driver.h"
#include "adaps_rx_temprature.h"
#include "adaps_flash_driver.h"
#include "adaps_tx_switch.h"
#include "adaps_imu.h"
#include "bsp_basic.h"
#include "bsp_upgrade.h"
#include "bsp_nvram.h"
#include "usmart.h"

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 6
#define FW_VERSION_REVISION 0
#define FW_VERSION_EXTRA    0
#ifdef MODE_1D5_ENABLE
#define FW_VERSION_DEBUG_STR "1.5D"
#else
//#define FW_VERSION_DEBUG_STR ""
#endif

#define FW_VERSION_NUM ((FW_VERSION_MAJOR << 12) | (FW_VERSION_MINOR << 8) | (FW_VERSION_REVISION))

#define HW_VERSION_NUM ((HW_VERSION_MAJOR << 12) | (HW_VERSION_MINOR << 8) | (HW_VERSION_REVISION))

#define USMART_TICK_CLK 64

#define HEARTBEAT_OVERFLOW 30 // unit: 100ms

#define CALI_PVDD_CHECKSUM(m_vol, m_duty)   (m_vol + m_duty + 0xFFFF0000)

#define FRAME_TYPE_ODD 0 //奇数帧:1,3,5...
#define FRAME_TYPE_EVEN 1 //偶数帧:2,4,6...


#define PVDD_SWIO_OFF   0
#define PVDD_SWIO_NORMAL    1
#define PVDD_SWIO_SECOND    2

#define PVDD_CHN_SWITCH_ENABLE 0 //启用PVDD切换功能
#define PVDD_FIXED_CHN_NORMAL 1
#define PVDD_FIXED_CHN_SECOND 2

//option必须以--开头
//带数值的option，定义这种宏时要以=结尾
//十六进制数值以0x或0X开头
#define OPT_PVDD "--pvdd="
#define OPT_VOP "--vop="
#define OPT_TX_ERR_DISABLE_MASK "--tx_err_disable_mask="
#define OPT_TX_CHN_OPEN_TIME_MAX "--tx_chn_open_time_max="
#define OPT_DISABLE_HEARTBEAT "--disable-heartbeat"
#define OPT_DISABLE_POWER_CTRL "--disable-power-ctrl"
#define OPT_BOOT_POWERUP "--boot-powerup"
#define OPT_ENABLE_TX_SHORT_ERR "--enable-tx-short-error"
#define OPT_DISABLE_ERR_PROC "--disable-error-proc"

#define DEFINE_SUPPORT_OPT(m_opt)   m_opt ","

enum FlashWorkState_e
{
    FLASH_FSM_IDLE = 0,
    FLASH_FSM_READY_TO_READ,
    FLASH_FSM_READING,
    FLASH_FSM_READ_DONE,
    FLASH_FSM_DATA_TX_ING,
    FLASH_FSM_READY_TO_RX_DATA,
    FLASH_FSM_DATA_RX_ING,
    FLASH_FSM_READY_TO_WRITE,
    FLASH_FSM_WRITING,
    FLASH_FSM_ERR_OCCUR
};

typedef struct pvdd_switch_ctx_s
{
    uint32_t is_stream_on;
    uint32_t fixed_chn_flag;
    uint32_t frame_type;
    uint32_t ctrl_state;
    uint32_t need_switch;
    uint32_t fsync_count;
    uint32_t fsync_tick[2];
    uint32_t switch_tick[2];
    uint32_t first_subframe_count;
    uint32_t last_subframe_count;
}pvdd_switch_ctx_t;



typedef struct flash_op_ctx_s
{
    uint8_t flag;//read==1 or write==0
    uint8_t fsm_state;
    uint16_t len;
    uint16_t offset1;
    uint32_t offset;
    uint8_t buf[FLASH_OP_SIZE_MAX + 16];
}flash_op_ctx_t;


typedef struct cali_pvdd_data_s
{
    uint32_t vol;
    uint32_t duty;
    uint32_t checksum;
}cali_pvdd_data_t;

//目前硬件确认PVDD不使用PID，VOP要使用PID --2024/02/19
static uint8_t s_pvdd_pid_enable = 0;
static uint8_t s_vop_pid_enable = 1;
static uint8_t s_pvdd2_pid_enable = 0;

static uint16_t s_pvdd_oc_vol = 0;
static uint16_t s_pvdd_oc_current = 0;
static uint16_t s_pvdd_oc_pmic_state = 0;
static uint16_t s_pvdd_current_peak = 0;

static uint8_t flag_pvdd_oc = 0;
static uint8_t count_pvdd_oc = 0;

static float s_temperature_tx_borad = 0.0f;
static float s_temperature_rx_borad = 0.0f;
static uint32_t voltage_dv11 = 0, voltage_sv11 = 0;
static uint32_t voltage_vop = 0, voltage_pvdd = 0;
static uint32_t voltage_pvdd2 = 0;
static uint32_t voltage_pvdd_ict = 0;

static uint32_t sU32VoltageVref = 0;
static uint16_t pvdd_current;

static uint32_t err_int_trigger_tick = 0;

static uint16_t s_led_blink_ctrl = 10; // default: once per second

static uint32_t s_heartbeat_count = 0;


static pvdd_switch_ctx_t s_pvdd_switch_ctx = {
    .is_stream_on = 0,
    .fixed_chn_flag = PVDD_FIXED_CHN_NORMAL,
    .frame_type = FRAME_TYPE_ODD,
    .fsync_count = 0,
    .fsync_tick = {0}
};


/*
可从固件bin内容中0x1000(0x6000-0x5000)地址中来识别硬件版本;
release脚本会做判断，不能去掉此定义,也不能改地址;
*/
static const char s_hardware_name[] ATTR_USED ATTR_BUILD_AT(0x6000) = {
    DEMO_BOARD_NAME
};

/*
可从固件bin内容中0x1100(0x6100-0x5000)地址查看硬件版本号;
release脚本提取出来，不能去掉此定义,也不能改地址;
Spadisapp升级固件会判断此版本号,格式不能改变;
*/
static const char s_hardware_version[] ATTR_USED ATTR_BUILD_AT(0x6100) = {
    "v" STR(HW_VERSION_MAJOR) 
    "." STR(HW_VERSION_MINOR) 
    "." STR(HW_VERSION_REVISION)
};

/*
可从固件bin内容中0x1200(0x6200-0x5000)地址查看固件版本号;
release脚本提取出来用，不能去掉此定义,也不能改地址;
用打包工具打包的固件包名会带有此版本号字符串;
*/
static const char s_software_version[] ATTR_USED ATTR_BUILD_AT(0x6200) = {
    "v" STR(FW_VERSION_MAJOR) 
    "." STR(FW_VERSION_MINOR) 
    "." STR(FW_VERSION_REVISION)
#if FW_VERSION_EXTRA != 0
    "-" STR(FW_VERSION_EXTRA)
#endif
#ifdef FW_VERSION_DEBUG_STR
    "-" FW_VERSION_DEBUG_STR
#endif
};

/*
可从固件bin内容中0x1300(0x6300-0x5000)地址查看固件编译时间;
因hardware_version里格式不能变，DEMO_BOARD_ALIAS只能在build_time里添加;
*/
static const char s_build_time[] ATTR_USED ATTR_BUILD_AT(0x6300) = {
    __DATE__ " " __TIME__ "(" DEMO_BOARD_ALIAS ")"
};

/*
可从固件bin内容中0x1400(0x6400-0x5000)地址查看固件执行选项
*/
static const char s_boot_option[512] ATTR_USED ATTR_BUILD_AT(0x6400) = {
    //"--pvdd=30 --vop=22 --disable-power-ctrl --boot-powerup --disable-heartbeat"
    //"--disable-heartbeat"
#ifdef BOARD_MOVA
    //目前MOVA MCU需要常上电才能I2C通讯
    "--disable-power-ctrl --boot-powerup"
#endif
};

/*
可从固件bin内容中0x1600(0x6600-0x5000)地址查看固件支持的执行选项
*/

static const char s_support_options[] ATTR_USED ATTR_BUILD_AT(0x6600) = {
    DEFINE_SUPPORT_OPT(OPT_PVDD)
    DEFINE_SUPPORT_OPT(OPT_VOP)
    DEFINE_SUPPORT_OPT(OPT_TX_ERR_DISABLE_MASK)
    DEFINE_SUPPORT_OPT(OPT_TX_CHN_OPEN_TIME_MAX)
    DEFINE_SUPPORT_OPT(OPT_DISABLE_HEARTBEAT)
    DEFINE_SUPPORT_OPT(OPT_DISABLE_POWER_CTRL)
    DEFINE_SUPPORT_OPT(OPT_BOOT_POWERUP)
    DEFINE_SUPPORT_OPT(OPT_ENABLE_TX_SHORT_ERR)
    DEFINE_SUPPORT_OPT(OPT_DISABLE_ERR_PROC)
};

static uint16_t s_sys_abnormal_state_flag = 0;

static uint8_t s_is_fixed_pvdd = 0;
static uint8_t s_is_fixed_vop = 0;
static uint32_t s_boot_pvdd_mv = PVDD_VOL_DEFAULT;
static int32_t s_boot_vop_mv = VOP_VOL_DEFAULT;
#ifdef DISABLE_POWER_CTRL
static uint8_t s_disable_power_ctrl = 1;
#else
static uint8_t s_disable_power_ctrl = 0;
#endif
#ifdef PMIC_DEFAULT_POWERUP
static uint8_t s_boot_powerup = 1;
#else
static uint8_t s_boot_powerup = 0;
#endif

#ifdef HEARTBEAT_ENABLE_DEFAULT
static uint8_t s_heartbeat_enable = 1;
#else
static uint8_t s_heartbeat_enable = 0;
#endif

#ifdef ERR_PROC_ENABLE
static uint8_t s_err_proc_enable = 1;
#else
static uint8_t s_err_proc_enable = 0;
#endif

static uint32_t s_boot_option_set_flag = 0;

static flash_op_ctx_t flash_op_ctx;

static int app_upgrade_trigger(void);

static int option_get(char* all_option, char* option)
{
    int offset;
    char* pos = strstr(all_option, option);
    if (NULL == pos) {
        return -1;
    }
    offset = strlen(option);
    if (pos[offset - 1] != '=') {
        return 0;
    }

    if((pos[offset] == '0') 
    && ((pos[offset + 1] == 'x') || (pos[offset + 1] == 'X'))){
        return strtol(&pos[offset], NULL, 16);
    }else{
        return atoi(&pos[offset]);
    }
}

static void dump_boot_option(void)
{
    const char *ptr = s_boot_option;
    printf("dump boot option:");
    while (*ptr != '\0') {
        if ((ptr[0] == '-') && (ptr[1] == '-')) {
            putchar('\n');
            putchar('\t');
        }
        putchar(*ptr);
        ptr++;
    }
    putchar('\n');
}

static void dump_support_option(void)
{
    const char *ptr = s_support_options;
    printf("dump support option\n");
    while (*ptr != '\0') {
        if ((ptr[0] == '-') && (ptr[1] == '-')) {
            putchar('\n');
            putchar('\t');
        }
        if (ptr[0] != ',') {
            putchar(*ptr);
        }
        ptr++;
    }
    putchar('\n');
}

void dump_option(void)
{
    dump_support_option();
    dump_boot_option();
}

static uint32_t boot_option_get_index_in_list(char* option)
{
    int count = 0;
    int i;
    char *ptr;

    ptr = strstr(s_support_options, option);
    if(NULL == ptr){
        return -1;
    }

    while(ptr != s_support_options){
        if(*ptr == ','){
            count++;
        }
        ptr--;
    }
    return count;
}

static void boot_option_parse(void)
{
    int ret;
    int index;
    ret = option_get(s_boot_option, OPT_PVDD);
    if((ret > 18) && (ret < 65)){
        printf("got option:pvdd=%u V\n", ret);
        s_boot_pvdd_mv = ret * 1000;
        index = boot_option_get_index_in_list(OPT_PVDD);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_VOP);
    if((ret > 18) && (ret < 65)){
        printf("got option:vop=%u V\n", ret);
        s_boot_vop_mv = ret * -1000;
        index = boot_option_get_index_in_list(OPT_VOP);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_TX_ERR_DISABLE_MASK);
    if(ret >= 0){
        printf("got option:tx_chn_open_time_max=0x%X\n", ret);
        dv8001_status_mask_config((uint32_t)ret);
        index = boot_option_get_index_in_list(OPT_TX_ERR_DISABLE_MASK);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_TX_CHN_OPEN_TIME_MAX);
    if(ret >= 0){
        printf("got option:tx_chn_open_time_max=%d us\n", ret);
        if(ret == 0){
            txswt_timer_enable(0);
        }else{
            if(txswt_set_rx_expo_time1(ret) >= 0){
                txswt_timer_enable(1);
            }
        }
        index = boot_option_get_index_in_list(OPT_TX_CHN_OPEN_TIME_MAX);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_DISABLE_HEARTBEAT);
    if(ret >= 0){
        printf("got option:disable-heartbeat\n");
        s_heartbeat_enable = 0;
        index = boot_option_get_index_in_list(OPT_DISABLE_HEARTBEAT);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_DISABLE_POWER_CTRL);
    if(ret >= 0){
        printf("got option:disable-power-ctrl\n");
        s_disable_power_ctrl = 1;
        index = boot_option_get_index_in_list(OPT_DISABLE_POWER_CTRL);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_BOOT_POWERUP);
    if(ret >= 0){
        printf("got option:boot-powerup\n");
        s_boot_powerup = 1;
        index = boot_option_get_index_in_list(OPT_BOOT_POWERUP);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_ENABLE_TX_SHORT_ERR);
    if(ret >= 0){
        printf("got option:enable-tx-short-error\n");
        txswt_chn_short_protect_enable(1);
        reg_set_val_not_set_flag(OFFSET_TX_CHN_SHORT_ENABLE, 1);
        index = boot_option_get_index_in_list(OPT_ENABLE_TX_SHORT_ERR);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

    ret = option_get(s_boot_option, OPT_DISABLE_ERR_PROC);
    if(ret >= 0){
        printf("got option:disable-error-proc\n");
        s_err_proc_enable = 0;
        index = boot_option_get_index_in_list(OPT_DISABLE_ERR_PROC);
        if((index >= 0) && (index < 32)){
            s_boot_option_set_flag |= BIT(index);
        }
    }

}


static uint8_t adaps_flash_checksum(uint8_t *data, int len) {
    uint8_t checksum = 0xF;
    for (int i = 0; i < len; i++){
        checksum ^= data[i];
    }
    return checksum;
}

void pvdd_switch_by_gpio(uint32_t flag)
{
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_SWIO)){
        switch(flag){
            case PVDD_SWIO_OFF:
                bsp_gpio_write(PVDD_SW1_PORT, PVDD_SW1_PIN, GPIO_PIN_RESET);
                bsp_gpio_write(PVDD_SW2_PORT, PVDD_SW2_PIN, GPIO_PIN_RESET);
                s_pvdd_switch_ctx.ctrl_state = PVDD_SWIO_OFF;
            break;
            case PVDD_SWIO_NORMAL:
                bsp_gpio_write(PVDD_SW1_PORT, PVDD_SW1_PIN, GPIO_PIN_RESET);
                bsp_gpio_write(PVDD_SW2_PORT, PVDD_SW2_PIN, GPIO_PIN_SET);
                s_pvdd_switch_ctx.ctrl_state = PVDD_SWIO_NORMAL;
            break;
            case PVDD_SWIO_SECOND:
                bsp_gpio_write(PVDD_SW2_PORT, PVDD_SW2_PIN, GPIO_PIN_RESET);
                bsp_gpio_write(PVDD_SW1_PORT, PVDD_SW1_PIN, GPIO_PIN_SET);
                s_pvdd_switch_ctx.ctrl_state = PVDD_SWIO_SECOND;
            break;
        }
    }
}

static void report_err_by_uart(void)
{
    uint8_t buf[16];
    // format: "$ERR,errcode,checksum\r\n"
    snprintf(buf, sizeof(buf), "$ERR,%04X", reg_val_get(OFFSET_ERR_CODE));
    printf("%s,%02X\r\n", buf, calc_str_xor(buf));
}

void sys_enter_abnormal_state(uint16_t cause_err)
{
    s_sys_abnormal_state_flag++;
    reg_status_update(OFFSET_RUN_STATE, ADAPS_PROTECT_STATE);
    reg_err_code_update(cause_err, 1);
    gpio_write(RK_ERR_PORT, RK_ERR_PIN, GPIO_PIN_SET);
    report_err_by_uart();
    err_int_trigger_tick = bsp_tick_get();
    s_led_blink_ctrl = 2;
    printf("[%d]err_interrupt_gpio is pull up at %d, err_code: 0x%x\n", err_int_trigger_tick, err_int_trigger_tick, cause_err);
}

static void sys_exit_abnormal_state(void)
{
    reg_set_val(OFFSET_POWER_CTRL_ALL, 0);
    pmic_powerdown_all(NULL);
    txswt_state_clean();
    reg_status_update(OFFSET_RUN_STATE, ADAPS_NORMAL_STATE);
    reg_err_code_update(ADAPS_ERR_ALL, 0);
    gpio_write(RK_ERR_PORT, RK_ERR_PIN, GPIO_PIN_RESET);
    s_led_blink_ctrl = 10;
    printf("sys_exit_abnormal_state finish\n");
}

static void update_shark_state(void)
{
    #define SHARK_STATE_READ_INTERVAL   100
    #define SHARK_STATE_REG_UPDATE_INTERVAL   200
    static uint32_t reset_count = 0;
    static uint32_t update_reg_last_time = 0;
    uint8_t chip0_reg_val[ARR_LENGTH_DV8001_REG];
    uint8_t chip1_reg_val[ARR_LENGTH_DV8001_REG];
    uint8_t chip0_err;
    uint8_t chip1_err;
    uint16_t all_status;
    int i;
    uint32_t time_now;
    uint8_t pmic_state;
    uint8_t chip0_state;
    uint8_t chip1_state;
    uint32_t pvdd_state;
    uint32_t pvdd_vol;


    if(!txswt_is_open()){
        return;
    }

    time_now = bsp_tick_get();
    if(time_now - dv8001_get_state_update_time() >= SHARK_STATE_READ_INTERVAL){
        //bsp_disable_irq();
        txswt_pause();
        dv8001_state_update();
        txswt_resume();
        //bsp_enable_irq();
    }

    if((time_now - update_reg_last_time) >= SHARK_STATE_REG_UPDATE_INTERVAL){
        update_reg_last_time = time_now;
        dv8001_state_get(chip0_reg_val, chip1_reg_val, &chip0_err, &chip1_err);
        reg_status_update(OFFSET_SHARK_ERR_PIN_STATE, COMBINE_U16(chip1_err, chip0_err));
        if(s_err_proc_enable){
            if((!(reg_val_get(OFFSET_ERR_DISABLE_MASK) & ADAPS_TX_SWITCH_ERR))
            && !(reg_val_get(OFFSET_ERR_CODE) & ADAPS_TX_SWITCH_ERR)
            && (chip1_err | chip0_err)){
                do{
                    chip0_state = chip0_reg_val[OFFSET_STATUS_FLAG];
                    chip1_state = chip1_reg_val[OFFSET_STATUS_FLAG];
                    all_status = COMBINE_U16(chip1_state, chip0_state);
                    all_status &= COMBINE_U16(DV8001_STATE_ERR_MASK, DV8001_STATE_ERR_MASK);
                    if (all_status) {
                        // 如果仅报低压警告(UVLO60V或LDO5_OK置位)，则判断是否是因为没上电或正在上电
                        if (((reset_count < 5) &&
                            !(all_status &
                            (~COMBINE_U16(BIT(OFFSET_UVLO60V_FLAG) |
                                                BIT(OFFSET_LDO5_OK_FLAG),
                                            BIT(OFFSET_UVLO60V_FLAG) |
                                                BIT(OFFSET_LDO5_OK_FLAG)))))) {
                            pmic_state = pmic_get_state();
                            pvdd_state = pmic_pvdd_wave_state();
                            pvdd_vol = pmic_get_pvdd_vol();
                            if ((pmic_state != ADAPS_PMIC_POWER_UP_DONE) ||
                                (pvdd_state != WAVE_STATE_STABLE_NEAR) ||
                                (pvdd_vol < DV8001_DRIVER_VOL_MIN)) {
                                txswt_pause();
                                dv8001_reset();
                                txswt_resume();
                                reset_count++;
                                printf(
                                    "[%d]reset tx because system is in innormal "
                                    "state, "
                                    "chip0 state 0x%02X, chip1 state 0x%02X, pmic "
                                    "state %d, pvdd state 0x%X, pvdd vol %d mv\n",
                                    reset_count, chip0_state, chip1_state,
                                    pmic_state, pvdd_state, pvdd_vol);
                                break;
                            }
                        }
                        reset_count = 0;
                        printf("ADAPS_TX_SWITCH_ERR detect! chip0 state 0x%02X, chip1 state 0x%02X\n", chip0_state, chip1_state);
                        txswt_print_state();
                        //reg_err_code_update(ADAPS_TX_SWITCH_ERR, 1);
                        sys_enter_abnormal_state(ADAPS_TX_SWITCH_ERR);
                    }else{
                        reset_count = 0;
                    }
                }while(0);
            }
        }

        for(i = 0; i < ARR_LENGTH_DV8001_REG; i++){
            reg_status_update(OFFSET_SHARK_REG_00 + i, COMBINE_U16(chip1_reg_val[i], chip0_reg_val[i]));
        }
    }
    
}

static void check_txswt_state(void)
{
    update_shark_state();
}

static void led_toggle(void)
{
    static uint8_t led_toggle_flag = 0;

    switch (led_toggle_flag++ % 4)
    {
    case 0:
        gpio_write(LED2_PORT, LED2_PIN, GPIO_PIN_SET);
        break;
    case 1:
        gpio_write(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        break;
    case 2:
        gpio_write(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
        break;
    case 3:
        gpio_write(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
        break;
    }
}

static void led_control(void)
{
    static uint8_t count = 0;
    if (count++ % s_led_blink_ctrl == 0)
    {
        led_toggle();
    }
}

void temperature_update()
{

    static uint8_t count = 0;
    int16_t temp_int16;

    count++;
    if (count >= 10) // periodic 1s
    {
#ifdef HW_REMOVE_NST112
        //Demo4上TX温度采用NTC
        s_temperature_tx_borad = rx_temprature_get();
        //SpadisApp显示的OFFSET_TEMP_B的值
        reg_status_update(STATUS_REG_BASE + OFFSET_TEMP_B, (int16_t)(s_temperature_tx_borad * 100));
#else
        //旧TX板贴有NST112A与NST112B，而新TX板只贴一片NST112，型号不固定
        //SpadisApp显示的OFFSET_TEMP_B的值，这里改成只读CHIPB，由nst112驱动适配硬件差异
        s_temperature_tx_borad = get_temperature_nst112(CHIPB);
        temp_int16 = (int16_t)(s_temperature_tx_borad * 100);
        reg_status_update(STATUS_REG_BASE + OFFSET_TEMP_B, (int16_t)(s_temperature_tx_borad * 100));

        s_temperature_rx_borad = rx_temprature_get();
        reg_status_update(STATUS_REG_BASE + OFFSET_TEMP_RX_BOARD, (int16_t)(s_temperature_rx_borad * 100));
        if (get_dbgflag() & ADAPS_DBG_TRACE_TEMPERATURE_READ)
        {
            printf("rx board temprature %.03f degree\n", s_temperature_rx_borad);
        }
#endif
        count = 0;
    }
}

static void voltage_update(void)
{
    uint32_t voltage;
    uint32_t adc_val;
    uint16_t err_mask = reg_val_get(OFFSET_ERR_DISABLE_MASK);
    uint32_t wave_state;
    uint32_t target_vol;
    uint32_t cur_vol;
    uint32_t err_flag;

#ifndef HW_REMOVE_DV11_ADC
    voltage = pmic_sample_dv11_vol(&err_flag);
    if (voltage_dv11 != voltage){
        voltage_dv11 = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_VOL_DV11, voltage);
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update voltage_dv11 to %d\r\n", voltage_dv11);
        }
    }

    if(s_err_proc_enable){
        if(err_flag && (!(err_mask & ADAPS_DV11_VOLTAGE_ERR))
        && (!(reg_val_get(OFFSET_ERR_CODE) & ADAPS_DV11_VOLTAGE_ERR))){
            printf("####dv11 err occur, voltage:%d\r\n", voltage_dv11);
            sys_enter_abnormal_state(ADAPS_DV11_VOLTAGE_ERR);
        }
    }
#endif

#ifndef HW_REMOVE_SV11_ADC
    voltage = pmic_sample_sv11_vol(&err_flag);
    if (voltage_sv11 != voltage){
        voltage_sv11 = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_VOL_SV11, voltage);
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update voltage_sv11 to %d\r\n", voltage_sv11);
        }
    }

    if(s_err_proc_enable){
        if(err_flag && (!(err_mask & ADAPS_SV11_VOLTAGE_ERR))
        && (!(reg_val_get(OFFSET_ERR_CODE) & ADAPS_SV11_VOLTAGE_ERR))){
            printf("####sv11 err occur, voltage:%d\r\n", voltage_sv11);
            sys_enter_abnormal_state(ADAPS_SV11_VOLTAGE_ERR);
        }
    }

#endif

#ifndef HW_REMOVE_VREF_ADC
    voltage = (adc_get_result(ADC_CHANNEL_VREF) * VREF / ADC_MAX);
    if (sU32VoltageVref != voltage){
        sU32VoltageVref = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_VREF_VOL, voltage);
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update sU32VoltageVref to %d\r\n", sU32VoltageVref);
        }
    }
#endif

#ifdef HW_HAVE_PVDDL
    voltage = pmic_sample_pvdd2_vol(NULL, 1);
    if (voltage_pvdd2 != voltage){
        voltage_pvdd2 = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_PVDDL_VOL, (int16_t)(voltage / 10));
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update voltage_pvdd2 to %d\r\n", voltage_pvdd2);
        }
    }
#endif

#ifdef HW_HAVE_PVDD_ICT
    voltage = pmic_sample_pvdd_ict_vol();
    if (voltage_pvdd_ict != voltage){
        voltage_pvdd_ict = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_PVDD_ICT_VOL, (int16_t)(voltage / 10));
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update voltage_pvdd_ict to %d\r\n", voltage_pvdd_ict);
        }
    }
#endif

    voltage = pmic_sample_pvdd_vol(NULL, 1);
    if (voltage_pvdd != voltage){
        voltage_pvdd = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_PVDD_VOL, (int16_t)(voltage / 10));
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update voltage_pvdd to %d\r\n", voltage_pvdd);
        }
        if(s_err_proc_enable){
            //PVDD校准过之后才判断是否warning/err
            //上电或pvdd配置后，需要等待pvdd稳定
            //TX不控制时才判断
            //PVDD按帧切换使能时不判断
            if((pmic_get_state() == ADAPS_PMIC_POWER_UP_DONE)
            && (s_pvdd_switch_ctx.fixed_chn_flag == PVDD_FIXED_CHN_NORMAL)
            && (reg_val_get(OFFSET_CALI_PVDD_SAVED_VOL) == reg_val_get(OFFSET_VOL_PVDD))
            && (pmic_pvdd_adjust_is_finish())
            && ((bsp_tick_get() - txswt_ctrl_last_time_get()) > 1000)){
                wave_state = pmic_pvdd_wave_state();
                //不是在上升或下降状态才做判断, warn状态在电压上升或下降时保持
                if((wave_state != WAVE_STATE_UNKNOWN)
                && ((wave_state & WAVE_STATE_RELATE_MASK) != WAVE_STATE_FALLING)
                && ((wave_state & WAVE_STATE_RELATE_MASK) != WAVE_STATE_RISING)){
                    if(!(err_mask & ADAPS_PVDD_VOLTAGE_ERR)
                    && (!(reg_val_get(OFFSET_ERR_CODE) & ADAPS_PVDD_VOLTAGE_ERR))){
                        target_vol = reg_val_get(OFFSET_VOL_PVDD) * 10;
                        cur_vol = pmic_pvdd_avrg_vol();
                        if(ABS(target_vol, cur_vol) > PVDD_ERR_FAULT_LIMIT){
                            printf("####pvdd: err occur, wave state = 0x%04x(%s), target_vol = %d, avrg_vol = %d\r\n", 
                                wave_state, pmic_wave_state_str(wave_state), target_vol, cur_vol);
                            sys_enter_abnormal_state(ADAPS_PVDD_VOLTAGE_ERR);
                        }
                    }
                    if(wave_state == WAVE_STATE_STABLE_NEAR){
                        if(reg_val_get(OFFSET_WARN_CODE) & ADAPS_PVDD_VOLTAGE_WARN){
                            printf("pvdd: warn disappear, wave state = 0x%04x(%s), voltage_pvdd = %d mv\r\n", 
                                wave_state, pmic_wave_state_str(wave_state), voltage_pvdd);
                            reg_warn_code_update(ADAPS_PVDD_VOLTAGE_WARN, 0);
                        }
                    }else if((!txswt_is_enable())
                    && (!(reg_val_get(OFFSET_WARN_CODE) & ADAPS_PVDD_VOLTAGE_WARN))){
                        //不发光的时候才报警
                        printf("****pvdd: warn occur, wave state = 0x%04x(%s), voltage_pvdd = %d mv\r\n", 
                            wave_state, pmic_wave_state_str(wave_state), voltage_pvdd);
                        reg_warn_code_update(ADAPS_PVDD_VOLTAGE_WARN, 1);
                    }
                }
            }
        }
    }

    voltage = pmic_sample_vop_vol(NULL, 1);
    if (voltage_vop != voltage){
        voltage_vop = voltage;
        reg_status_update(STATUS_REG_BASE + OFFSET_VOP_VOL_ABS, (int16_t)(voltage / 10));
        if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_UPDATE){
            printf("update voltage_vop to %d\r\n", voltage_vop);
        }
        if(s_err_proc_enable){
            //目前没有VOP(过压/欠压)报错的需求
            //上电或vop配置后，需要等待VOP稳定(PID调节到稳态也需要时间)
            if((pmic_get_state() == ADAPS_PMIC_POWER_UP_DONE)
            && pmic_vop_adjust_is_finish()){
                wave_state = pmic_vop_wave_state();
                if(wave_state != WAVE_STATE_UNKNOWN){
                    if(wave_state == WAVE_STATE_STABLE_NEAR){
                        if(reg_val_get(OFFSET_WARN_CODE) & ADAPS_VOP_VOLTAGE_WARN){
                            printf("vop: warn disappear, wave state = 0x%04x(%s)\r\n", wave_state, pmic_wave_state_str(wave_state));
                            reg_warn_code_update(ADAPS_VOP_VOLTAGE_WARN, 0);
                        }
                    }else if(((wave_state & WAVE_STATE_RELATE_MASK) != WAVE_STATE_FALLING)
                    &&((wave_state & WAVE_STATE_RELATE_MASK) != WAVE_STATE_RISING)
                    &&((!s_vop_pid_enable) || (vop_pid_adjust_state_get() >= PID_STATE_ADJUST_DONE))){
                        if(!(reg_val_get(OFFSET_WARN_CODE) & ADAPS_VOP_VOLTAGE_WARN)){
                            printf("****vop: warn occur, wave state = 0x%04x(%s), pid state %d\r\n", 
                                wave_state, pmic_wave_state_str(wave_state), vop_pid_adjust_state_get());
                            reg_warn_code_update(ADAPS_VOP_VOLTAGE_WARN, 1);
                        }
                    }
                }
            }
        }
    }

}

static void voltage_update_periodly(void)
{
    static uint8_t count = 0;
    count++;
    if (count >= 2){ // periodic 200ms
        voltage_update();
        count = 0;
    }
}

static void pvdd_current_update()
{
    static uint8_t count = 0;
    uint16_t current;
    uint32_t adc_val;
    
    count++;
    if (count >= 10){ // periodic 1000ms
        //cost 2ms
        adc_val = bsp_adc_get_result_average(ADC_CHANNEL_CURRENT_PVDD, 100);
        // current = ((vol / 50) / 20) * 1000 = vol
        current = (uint16_t)(adc_val * VREF / ADC_MAX); // uint:mA
        if (pvdd_current != current){
            pvdd_current = current;
            reg_status_update(STATUS_REG_BASE + OFFSET_CURRENT_PVDD, current);
            if (get_dbgflag() & ADAPS_DBG_TRACE_CURRENT_UPDATE){
                printf("update pvdd current to %d\r\n", pvdd_current);
            }
        }
        count = 0;
    }
}

static void vop_voltage_control(void)
{
    static uint32_t last_ctrl_time = 0;
    uint32_t adc_vop;
    int32_t pwm_val;
    int16_t vop_vol;
    uint32_t time_now;

    time_now = bsp_tick_get();
    if((time_now - last_ctrl_time) < VOP_PID_PERIOD){
        return;
    }
    last_ctrl_time = time_now;

    vop_vol = reg_val_get(OFFSET_VOL_VOP);
    if(vop_vol == 0){
        return;
    }
    
    pwm_val = pmic_get_vop_duty();
    adc_vop = adc_get_result_average(ADC_CHANNEL_VOP, 10);
    pwm_val = pwm_val + pid_control_vop(pwm_val, adc_vop);
    pmic_set_vop_pwm(pwm_val);

    if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_CTRL){
        printf("[tm%u]vop ctrl: vol target = %d, cur = %d, adc:= %d, vop pwm_val:= %d\r\n", 
            bsp_tick_get(), vop_vol * 10, (uint32_t)(VOP_VOL_CALC(adc_vop) * 1000), adc_vop, pwm_val);
    }

}


static void pvdd_voltage_control(void)
{
    static uint32_t last_ctrl_time = 0;
    uint32_t adc_pvdd;
    uint32_t cur_vol;
    int32_t pwm_val;
    int16_t pvdd_setting = reg_val_get(OFFSET_VOL_PVDD);
    uint32_t pvdd_vol;
    uint32_t time_now;

    if(pvdd_setting == 0){
        return;
    }

    time_now = bsp_tick_get();
    if((time_now - pmic_powerup_done_time()) < (PVDD_PID_DELAY_TIME - INA_RESET_KEEP_TIME)){
        return;
    }

    if((time_now - last_ctrl_time) < PVDD_PID_PERIOD){
        return;
    }
    last_ctrl_time = time_now;

    pwm_val = pmic_get_pvdd_duty();
    cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
    pvdd_vol = pvdd_setting * 10;
    if(ABS(pvdd_vol, cur_vol) > PVDD_PID_CTRL_THRESHOLD){
        pvdd_pid_context_reset();
        pwm_val = (int32_t)PVDD_PWM_DUTY_CALC(pvdd_setting);
    }else{
        pwm_val = pwm_val + pid_control_pvdd(adc_pvdd);
    }

    if((pvdd_vol >= PVDD_USUALLY_VOL_MIN) && (pwm_val > PVDD_USUALLY_PWM_MAX)){
        pwm_val = PVDD_USUALLY_PWM_MAX;
    }

    pmic_set_pvdd_pwm(pwm_val);

    if (get_dbgflag() & ADAPS_DBG_TRACE_VOLTAGE_CTRL){
        printf("pvdd ctrl: target vol = %d, cur vol = %d, adc = %d,  pwm_val = %d\r\n", pvdd_vol, cur_vol, adc_pvdd, pwm_val);
    }
}


int cali_pvdd_data_save(uint32_t vol_mv, uint32_t duty)
{
    int ret;
    cali_pvdd_data_t data;
    memset(&data, 0, sizeof(cali_pvdd_data_t));
    data.vol = vol_mv;
    data.duty = duty;
    data.checksum = CALI_PVDD_CHECKSUM(vol_mv, duty);
    ret = bsp_nvram_write(BSP_NVRAM_PARTITION1, 0, (uint8_t *)&data, sizeof(cali_pvdd_data_t));
    if(ret){
        printf("cali_pvdd_data_save fail\n");
        return -1;
    }

    printf("cali_pvdd_data_save ok\n");

    return 0;
}

int cali_pvdd_data_load(cali_pvdd_data_t *data)
{
    int ret;
    if(NULL == data){
        return -1;
    }
    ret = bsp_nvram_read(BSP_NVRAM_PARTITION1, 0, (uint8_t *)data, sizeof(cali_pvdd_data_t));
    if(ret){
        printf("cali_pvdd_data_load fail\n");
        return -1;
    }
    if(data->checksum != CALI_PVDD_CHECKSUM(data->vol, data->duty)){
        printf("cali_pvdd_data checksum fail\n");
        return -1;
    }

    if((data->vol < PVDD_CALI_VOL_MIN) 
    || (data->vol > PVDD_CALI_VOL_MAX)
    || (data->duty > PWM_MAX_VALUE)){
        printf("cali_pvdd_data invalid vol(%d) or duty(%d)\n", data->vol, data->duty);
        return -1;
    }

    return 0;
    
}

void pvdd_cali_info_print(void)
{
    printf("pvdd cali info:\n");
    printf("target vol: %d\n", reg_val_get(OFFSET_CALI_PVDD_TARGET_VOL));
    printf("cmd: 0x%x\n", reg_val_get(OFFSET_CALI_PVDD_CMD));
    printf("state: 0x%x\n", reg_val_get(OFFSET_CALI_PVDD_STATE));
    printf("saved vol: %d\n", reg_val_get(OFFSET_CALI_PVDD_SAVED_VOL));
    printf("saved duty: %d\n", reg_val_get(OFFSET_CALI_PVDD_SAVED_DUTY));
}


int pvdd_calibration(uint32_t target_vol_mv)
{
    const uint32_t cali_precision_list[] = {
        PVDD_CALI_PRECISION_SEQ1,
        PVDD_CALI_PRECISION_SEQ2,
        PVDD_CALI_PRECISION_SEQ3
    };
    uint32_t cur_vol;
    uint32_t last_vol;
    uint32_t adc_pvdd;
    int32_t pwm_val;
    int32_t calc_duty;
    uint32_t vol_for_calc = target_vol_mv / 10;
    uint32_t vol_delta_abs;
    uint32_t cali_start_time;
    uint32_t stable_start_time = 0;
    uint32_t start_time;
    int32_t cali_ok = 0;
    uint32_t cali_precision;
    int32_t i;


    reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_ING);

    pvdd_pid_context_reset();
    pwm_val = pmic_get_pvdd_duty();
    cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
    cali_start_time = bsp_tick_get();
    calc_duty = (int32_t)PVDD_PWM_DUTY_CALC(vol_for_calc);
    printf("\n[%d]cur vol = %d, cur duty = %d, target = %d, calc duty = %d\n", cali_start_time, cur_vol, pwm_val, target_vol_mv, calc_duty);
    

    // 1. 配置成理论计算的占空比，并等待稳定
    pmic_set_pvdd_pwm(calc_duty);
    bsp_delay_ms(100);
    cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
    start_time = bsp_tick_get();
    while(1){
        if((bsp_tick_get() - start_time) > PVDD_CALI_STABLE_TIMEOUT){
            printf("pvdd cali fail, the vol is unstable! cur_vol %d\n", cur_vol);
            reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_FAIL | ADAPS_CALI_PVDD_ERR_VOL_UNSTABLE);
            reg_set_val(OFFSET_CALI_PVDD_CALC_VOL, 0); //设为0表示未知
            return 0;
        }
    
        bsp_delay_ms(100);
        last_vol = cur_vol;
        cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
        vol_delta_abs = ABS(last_vol, cur_vol);
        if(vol_delta_abs < PVDD_CALI_STABLE_THRESHOLD){
            if(stable_start_time == 0){
                stable_start_time = bsp_tick_get();
            }else if((bsp_tick_get() - stable_start_time) > PVDD_CALI_STABLE_THRESHOLD){
                printf("[%d]cur vol is reach calc vol %d\n", bsp_tick_get(), cur_vol);
                reg_set_val(OFFSET_CALI_PVDD_CALC_VOL, cur_vol / 10);
                break;
            }
        }else if(stable_start_time){
            stable_start_time = 0;
            printf("[%d]cur vol = %d\n", bsp_tick_get(), cur_vol);
        }
    }

    for(i = 0; (!cali_ok) && (i < ARRAY_SIZE(cali_precision_list)); i++){
        pwm_val = pmic_get_pvdd_duty();
        cali_precision = cali_precision_list[i];
        stable_start_time = 0;
        start_time = bsp_tick_get();
        printf("[%d]start to cali with precision %d mv\n", start_time, cali_precision);
        while((bsp_tick_get() - start_time) < PVDD_CALI_TIMEOUT){
            bsp_delay_ms(PVDD_CALI_PID_INTERVAL);
            cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
            vol_delta_abs = ABS(target_vol_mv, cur_vol);
            if(vol_delta_abs < cali_precision){
                if(stable_start_time == 0){
                    stable_start_time = bsp_tick_get();
                    printf("[%d]**stable start**cur vol = %d, duty = %d, target = %d\n", bsp_tick_get(), cur_vol, pwm_val, target_vol_mv);
                    continue;
                }else if((bsp_tick_get() - stable_start_time) < PVDD_CALI_STABLE_TIME){
                    continue;
                }
                cali_ok = 1;
                break;
            }else if(vol_delta_abs < PVDD_CALI_CTRL_THRESHOLD){
                //用P调节
                pwm_val = pwm_val +  (int)(PVDD_CALI_PARAM_P * ((float)cur_vol - (float)target_vol_mv));
            }else{
                //设回理论计算电压
                pwm_val = (int32_t)PVDD_PWM_DUTY_CALC(vol_for_calc);
            }
            pmic_set_pvdd_pwm(pwm_val);
            if(stable_start_time){
                stable_start_time = 0;
                printf("[%d]cur vol = %d, duty = %d, target = %d\n", bsp_tick_get(), cur_vol, pwm_val, target_vol_mv);
            }
        }
    }


    if(cali_ok){
        printf("[%d]pvdd cali end(cost %d ms), cur vol = %d, duty = %d, target = %d, err = %d\n", bsp_tick_get(), bsp_tick_get() - cali_start_time, cur_vol, pwm_val, target_vol_mv, target_vol_mv - cur_vol);
        if(cali_pvdd_data_save(target_vol_mv, pwm_val) == 0){
            reg_set_val(OFFSET_CALI_PVDD_SAVED_VOL, (int16_t)vol_for_calc);
            reg_set_val(OFFSET_CALI_PVDD_SAVED_DUTY, (int16_t)pwm_val);
            reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_SUCCESS);
        }else{
            reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_FAIL | ADAPS_CALI_PVDD_ERR_SAVE_FAIL);
            cali_ok = 0;
        }

    }else{
        reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_FAIL | ADAPS_CALI_PVDD_ERR_TIMEOUT);
        printf("[%d]pvdd cali fail, cur vol = %d, duty = %d, target = %d\n", bsp_tick_get(), cur_vol, pwm_val, target_vol_mv);
    }

    return cali_ok;
    
}

void test_pvdd_calibration(void)
{
    uint32_t target_vol_mv;
    while(1){
        target_vol_mv = 20000 + rand() % 42000;
        if(!pvdd_calibration(target_vol_mv))
        {
            break;
        }
    }

}

void pvdd_adjust_by_vol(uint32_t target_vol_mv, uint32_t check_interval_ms, uint32_t check_time_ms)
{

    uint32_t cur_vol;
    uint32_t adc_pvdd;
    int32_t pwm_val;
    uint32_t vol_for_calc = target_vol_mv / 10;
    uint32_t start_time;

    pwm_val = pmic_get_pvdd_duty();
    cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
    start_time = bsp_tick_get();
    printf("\n[%d]cur vol = %d, cur duty = %d, target = %d, calc duty = %d\n", start_time, cur_vol, pwm_val, target_vol_mv, (int32_t)PVDD_PWM_DUTY_CALC(vol_for_calc));

    pwm_val = (int32_t)PVDD_PWM_DUTY_CALC(vol_for_calc);
    pmic_set_pvdd_pwm(pwm_val);
    while((bsp_tick_get() - start_time) < check_time_ms){
        bsp_delay_ms(check_interval_ms);
        cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
        printf("[%d]cur vol = %d, duty = %d, target = %d\n", bsp_tick_get(), cur_vol, pwm_val, target_vol_mv);
    }

}


void pvdd_adjust_by_duty(uint32_t target_duty, uint32_t check_interval_ms, uint32_t check_time_ms)
{
    uint32_t cur_vol;
    uint32_t adc_pvdd;
    int32_t pwm_val;
    uint32_t start_time;

    pwm_val = pmic_get_pvdd_duty();
    cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
    start_time = bsp_tick_get();
    printf("\n[%d]cur vol = %d, cur duty = %d, target duty = %d\n", start_time, cur_vol, pwm_val, target_duty);

    pmic_set_pvdd_pwm(target_duty);
    while((bsp_tick_get() - start_time) < check_time_ms){
        bsp_delay_ms(check_interval_ms);
        cur_vol = pmic_sample_pvdd_vol(&adc_pvdd, 0);
        printf("[%d]cur vol = %d, target duty = %d\n", bsp_tick_get(), cur_vol, target_duty);
    }
}

void pvdd2_adjust_by_duty(uint32_t target_duty, uint32_t check_interval_ms, uint32_t check_time_ms)
{
#ifdef HW_HAVE_PVDDL
    uint32_t cur_vol;
    uint32_t adc_pvdd;
    int32_t pwm_val;
    uint32_t start_time;

    pwm_val = pmic_get_pvdd2_duty();
    cur_vol = pmic_sample_pvdd2_vol(&adc_pvdd, 0);
    start_time = bsp_tick_get();
    printf("\n[%d]cur vol = %d, cur duty = %d, target duty = %d\n", start_time, cur_vol, pwm_val, target_duty);

    pmic_set_pvdd2_pwm(target_duty);
    while((bsp_tick_get() - start_time) < check_time_ms){
        bsp_delay_ms(check_interval_ms);
        cur_vol = pmic_sample_pvdd2_vol(&adc_pvdd, 0);
        printf("[%d]cur vol = %d, target duty = %d\n", bsp_tick_get(), cur_vol, target_duty);
    }
#else
    printf("Unsupport pvdd2...\n");
#endif
}


static void heartbeat_proc(void)
{
    if(!s_heartbeat_enable){
        return;
    }
    s_heartbeat_count++;
    if (reg_val_get(OFFSET_HEART_BEAT_SWITCH) && (s_heartbeat_count > HEARTBEAT_OVERFLOW))
    {
        s_heartbeat_count = 0;
        reg_control_set(OFFSET_VCSEL_ENABLE, 0);
#ifdef HEARTBEAT_TIMEOUT_POWERDOWN
        printf("heartbeat timeout, pmic powerdown all...\r\n");
        reg_set_val(OFFSET_POWER_CTRL_ALL, 0);
#else
        printf("heartbeat timeout, let vcsel disable...\r\n");
#endif
    }
}

void Upgrade_Program(void)
{
    app_upgrade_trigger();
}

//此callback会在中断isr中调用，不能有耗时长程序
static void bsp_telm_evt_callback(bsp_telm_evt_t evt)
{
    switch(evt)
    {
        case BSP_TELM_EVT_MASS_TX_DONE:
            if(flash_op_ctx.fsm_state == FLASH_FSM_DATA_TX_ING){
                flash_op_ctx.fsm_state = FLASH_FSM_IDLE;
            }
        break;
        case BSP_TELM_EVT_MASS_RX_DONE:
            if(flash_op_ctx.fsm_state == FLASH_FSM_DATA_RX_ING){
                flash_op_ctx.fsm_state = FLASH_FSM_READY_TO_WRITE;
            }
        break;
        case BSP_TELM_EVT_MASS_TX_TIMEOUT:
            if(flash_op_ctx.fsm_state == FLASH_FSM_DATA_TX_ING){
                flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
            }
        break;
        case BSP_TELM_EVT_MASS_RX_TIMEOUT:
            if(flash_op_ctx.fsm_state == FLASH_FSM_DATA_RX_ING){
                flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
            }
        break;
    }

}

//此callback是在中断isr中调用，不能有耗时长程序
static int bsp_telm_msg_callback(uint8_t *rx_msg, uint16_t rx_len, uint8_t *tx_msg)
{
    int16_t val;
    uint8_t msg0;
    uint8_t msg1;
    uint16_t u16val;
    if (rx_msg){
        s_heartbeat_count = 0;
        msg0 = rx_msg[0];
        if (msg0 & 0x80){ // read reg
            if (!tx_msg){
                return -1;
            }

            if (msg0 & 0x7F){ // reg addr more than 0xFF
                tx_msg[0] = 0xAA;
                tx_msg[1] = 0;
                tx_msg[2] = 0;
            }else if(rx_msg[1] == OFFSET_DATA_TRANS_CMD){
                if(flash_op_ctx.fsm_state != FLASH_FSM_READ_DONE){
                    tx_msg[0] = 0xBB;
                    tx_msg[1] = 0;
                    tx_msg[2] = 0;
                    return 3;
                }
                flash_op_ctx.fsm_state = FLASH_FSM_DATA_TX_ING;
                bsp_telm_mass_send(flash_op_ctx.buf, flash_op_ctx.len + 2);
                return 0;
            }else{
                val = reg_val_get(rx_msg[1]);
                tx_msg[0] = rx_msg[1]; // dummy
                tx_msg[1] = val >> 8;
                tx_msg[2] = val & 0xFF;
            }
            return 3; // less than BSP_TELM_TX_LEN_MAX
        }else if (!msg0){
            u16val = (rx_msg[2] << 8) | rx_msg[3];
            msg1 = rx_msg[1];
            switch(msg1)
            {
                case OFFSET_POWER_CTRL_ALL:
                    if(s_disable_power_ctrl){
                        reg_set_val_not_set_flag(OFFSET_POWER_CTRL_ALL, u16val);
                        return 0;
                    }
                    //保护状态下，通过SOC发送给MCU的TX/RX上电指令将无效
                    if((u16val)
                    && (reg_val_get(OFFSET_RUN_STATE) == ADAPS_PROTECT_STATE)){
                        return 0;
                    }
                break;
                case OFFSET_SENSOR_STREAM_STATE:
                    if(!bsp_hw_check_feature(HW_FEATURE_PVDD_SWIO)){
                        //Demo3或Demo4V0不支持PVDD切换
                        break;
                    }
                    if(u16val){
                        if(reg_val_get(OFFSET_SENSOR_STREAM_STATE) == 0){
                            //如果stream是从关到开
                            switch(s_pvdd_switch_ctx.fixed_chn_flag){
                                case PVDD_CHN_SWITCH_ENABLE:
                                    //因为这种方式是第一帧切，第一个FSYNC会切成奇数帧FRAME_TYPE_ODD
                                    s_pvdd_switch_ctx.frame_type = FRAME_TYPE_EVEN;
                                    txswt_switch_rx_expo_time(SEL_EXPO_TIME1);
                                break;
                                case PVDD_FIXED_CHN_SECOND:
                                    pvdd_switch_by_gpio(PVDD_SWIO_SECOND);
                                    txswt_switch_rx_expo_time(SEL_EXPO_TIME2);
                                break;
                                case PVDD_FIXED_CHN_NORMAL:
                                default:
                                    pvdd_switch_by_gpio(PVDD_SWIO_NORMAL);
                                    txswt_switch_rx_expo_time(SEL_EXPO_TIME1);
                                break;
                            }
                        }
                        s_pvdd_switch_ctx.is_stream_on = 1;
                    }else{
                        s_pvdd_switch_ctx.is_stream_on = 0;
                        if(reg_val_get(OFFSET_SENSOR_STREAM_STATE)){
                            //如果stream是从开到关
                            if(s_pvdd_switch_ctx.fixed_chn_flag == PVDD_FIXED_CHN_SECOND){
                                pvdd_switch_by_gpio(PVDD_SWIO_SECOND);
                                txswt_switch_rx_expo_time(SEL_EXPO_TIME2);
                            }else{
                                pvdd_switch_by_gpio(PVDD_SWIO_NORMAL);
                                txswt_switch_rx_expo_time(SEL_EXPO_TIME1);
                            }
                        }
                    }
                break;
                case OFFSET_FIXED_PVDD_CHN:
                    if(!bsp_hw_check_feature(HW_FEATURE_PVDD_SWIO)){
                        //Demo3或Demo4V0不支持PVDD切换
                        break;
                    }
                    switch(u16val){
                        case PVDD_FIXED_CHN_NORMAL:
                            s_pvdd_switch_ctx.fixed_chn_flag = PVDD_FIXED_CHN_NORMAL;
                            pvdd_switch_by_gpio(PVDD_SWIO_NORMAL);
                            txswt_switch_rx_expo_time(SEL_EXPO_TIME1);
                        break;
                        case PVDD_FIXED_CHN_SECOND:
                            s_pvdd_switch_ctx.fixed_chn_flag = PVDD_FIXED_CHN_SECOND;
                            pvdd_switch_by_gpio(PVDD_SWIO_SECOND);
                            txswt_switch_rx_expo_time(SEL_EXPO_TIME2);
                        break;
                        default:
                            s_pvdd_switch_ctx.fixed_chn_flag = PVDD_CHN_SWITCH_ENABLE;
                        break;
                    }
                break;
                case OFFSET_VOL_PVDD:
                    if(s_is_fixed_pvdd){
                        u16val = s_boot_pvdd_mv / 10;
                    }
                break;
                case OFFSET_VOL_VOP:
                    if(s_is_fixed_vop){
                        u16val = s_boot_vop_mv / 10;
                    }
                break;
            }
            reg_control_set(msg1, u16val);
            switch(msg1)
            {
                case OFFSET_DATA_TRANS_CMD:
                    switch(flash_op_ctx.fsm_state)
                    {
                        case FLASH_FSM_READY_TO_RX_DATA:
                            flash_op_ctx.fsm_state = FLASH_FSM_DATA_RX_ING;
                            bsp_telm_mass_revc(flash_op_ctx.buf, flash_op_ctx.len + 1);
                        break;
                        default:
                            reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_OP_ERR);
                            flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
                        break;
                    }
                break;
                case OFFSET_FLASH_OP_CMD:
                    flash_op_ctx.flag = ((u16val & 0x8000) ? FLASH_FLAG_READ : FLASH_FLAG_WRITE);
                    flash_op_ctx.len = u16val & 0x7FFF;
                    if(flash_op_ctx.len > FLASH_OP_SIZE_MAX){
                        reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_OP_ERR);
                        flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
                        break;
                    }
                    reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_BUSY);
                    if(flash_op_ctx.flag == FLASH_FLAG_WRITE){ //flash write
                        flash_op_ctx.fsm_state = FLASH_FSM_READY_TO_RX_DATA;
                    }else{
                        flash_op_ctx.fsm_state = FLASH_FSM_READY_TO_READ;
                    }
                break;
                case OFFSET_FLASH_OP_OFFSET1:
                    flash_op_ctx.offset1 = u16val;
                break;
                case OFFSET_FLASH_OP_OFFSET2:
                    flash_op_ctx.offset = (flash_op_ctx.offset1 << 16) | u16val;
                break;
            }
            return 0;
        }
    }
    return 0;
}

static void flash_op_proc(void)
{
    uint8_t checksum;
    int16_t i16tmp;
    switch(flash_op_ctx.fsm_state)
    {
        case FLASH_FSM_IDLE:
            i16tmp = reg_val_get(OFFSET_FLASH_OP_STATE);
            switch(i16tmp)
            {
                case FLASH_STATE_BUSY:
                case FLASH_STATE_READY:
                    //因为可能被中断改写fsm_state，这里再判断一次
                    if(flash_op_ctx.fsm_state == FLASH_FSM_IDLE){
                        //printf("flash state reset to idle, cur state %d\n", i16tmp);
                        reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_IDLE);
                    }
                break;
            }
        break;
        case FLASH_FSM_READY_TO_READ:
            if(flash_op_ctx.flag != FLASH_FLAG_READ){
                printf("flash read: strange state\n");
                reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_OP_ERR);
                flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
                break;
            }
            //printf("to read flash offset %d, len %d\n", flash_op_ctx.offset, flash_op_ctx.len);
            flash_op_ctx.fsm_state = FLASH_FSM_READING;
            adaps_flash_read_more_bytes(flash_op_ctx.offset, flash_op_ctx.buf, flash_op_ctx.len);
            checksum = adaps_flash_checksum(flash_op_ctx.buf, flash_op_ctx.len);
            flash_op_ctx.buf[flash_op_ctx.len] = checksum;
            flash_op_ctx.fsm_state = FLASH_FSM_READ_DONE;
            flash_op_ctx.offset += flash_op_ctx.len;
            reg_control_set(OFFSET_FLASH_OP_OFFSET1, flash_op_ctx.offset >> 16);
            reg_control_set(OFFSET_FLASH_OP_OFFSET2, flash_op_ctx.offset & 0xFFFF);
            reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_READY);
        break;
        case FLASH_FSM_READY_TO_WRITE:
            if(flash_op_ctx.flag != FLASH_FLAG_WRITE){
                printf("flash write: strange state\n");
                reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_OP_ERR);
                flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
                break;
            }
            checksum = adaps_flash_checksum(flash_op_ctx.buf, flash_op_ctx.len);
            if(checksum != flash_op_ctx.buf[flash_op_ctx.len]){
                printf("flash write: recv data checksum fail\n");
                reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_DATA_ERR);
                flash_op_ctx.fsm_state = FLASH_FSM_ERR_OCCUR;
                break;
            }
            //printf("to write flash offset %d, len %d\n", flash_op_ctx.offset, flash_op_ctx.len);
            flash_op_ctx.fsm_state = FLASH_FSM_WRITING;
            adaps_flash_write_more_bytes(flash_op_ctx.offset, flash_op_ctx.buf, flash_op_ctx.len);
            flash_op_ctx.offset += flash_op_ctx.len;
            reg_control_set(OFFSET_FLASH_OP_OFFSET1, flash_op_ctx.offset >> 16);
            reg_control_set(OFFSET_FLASH_OP_OFFSET2, flash_op_ctx.offset & 0xFFFF);
            flash_op_ctx.fsm_state = FLASH_FSM_IDLE;
            reg_status_update(OFFSET_FLASH_OP_STATE, FLASH_STATE_IDLE);
        break;
        case FLASH_FSM_ERR_OCCUR:
            printf("flash err occurred(state %d), now fsm reset to idle\n", reg_val_get(OFFSET_FLASH_OP_STATE));
            flash_op_ctx.fsm_state = FLASH_FSM_IDLE;
        break;
    }
}


static int hawk_tx_bsp_init(void)
{
    reg_status_update(OFFSET_FIRMWARE_VER, FW_VERSION_NUM);
    reg_status_update(OFFSET_HARDWARE_VER, HW_VERSION_NUM);
    reg_status_update(OFFSET_FW_VERSION_EXTRA, FW_VERSION_EXTRA);
    reg_status_update(OFFSET_APPLICATION_TYPE_U16, APPLICATION_TYPE_APP);
    reg_status_update(OFFSET_UPGRADE_PROGRAM_FLAG_U16, UPGRADE_PROGRAM_FLAG_NO_UPDATE);
    bsp_basic_init(s_hardware_version, s_software_version);
    printf("Hardware info: %s, name: %s(%s), tag: 0x%x, variant: %s\r\n", s_hardware_version, s_hardware_name, DEMO_BOARD_ALIAS, DEMO_BOARD_TAG, bsp_hw_variant_name());
    printf("software version:%s\r\n", s_software_version);
    printf("build time:%s\r\n", s_build_time);
    if(bsp_hw_variant_ver() != HW_VARIANT_VER0){
        s_boot_vop_mv = VOP_VOL_DEFAULT_V1;
    }
    reg_status_update(OFFSET_HW_VARIANT_VER, bsp_hw_variant_ver());
#ifdef BOARD_TAG_USE_VARIANT
    switch(bsp_hw_variant_ver()){
        case HW_VARIANT_VER1:
            reg_status_update(OFFSET_BOARD_TAG, 1);
        break;
        default:
            reg_status_update(OFFSET_BOARD_TAG, 0);
        break;
    }
#else
    reg_status_update(OFFSET_BOARD_TAG, DEMO_BOARD_TAG);
#endif
    bsp_nvram_init();
    bsp_gpio_init();
#ifndef HW_REMOVE_DAC
    bsp_dac_init();
#endif
#ifndef HW_REMOVE_NST112
    bsp_i2c_init();
#endif

    bsp_adc_init(ADC_INIT_ALL);
    bsp_pwm_init();
    bsp_timer_init();
    bsp_upgrade_init();
    bsp_spi_init();
#ifdef HW_HAVE_CLKOUT
    bsp_clkout_init();
    bsp_clkout_enable(BSP_CLKOUT_CHN0, 1);
#endif
    usmart_dev.init(USMART_TICK_CLK);

    return 0;
}


static int app_default_config(void)
{
    cali_pvdd_data_t pvdd_data;

#ifdef HW_TELM_USING_IIC
    reg_status_update(OFFSET_TELM_METHOD, 0x01);
#else
    reg_status_update(OFFSET_TELM_METHOD, 0xFF);
#endif

    pvdd_switch_by_gpio(PVDD_SWIO_NORMAL);

    reg_status_update(OFFSET_FIXED_PVDD_CHN, PVDD_FIXED_CHN_NORMAL);
    

    reg_status_update(OFFSET_BOOT_OPTION_SET1, s_boot_option_set_flag & 0xFFFF);
    reg_status_update(OFFSET_BOOT_OPTION_SET2, (s_boot_option_set_flag >> 16) & 0xFFFF);

    reg_status_update(OFFSET_FLASH_OP_SIZE_MAX, FLASH_OP_SIZE_MAX);
    if(cali_pvdd_data_load(&pvdd_data) == 0){
        reg_set_val(OFFSET_CALI_PVDD_SAVED_VOL, (int16_t)(pvdd_data.vol / 10));
        reg_set_val(OFFSET_CALI_PVDD_SAVED_DUTY, (int16_t)pvdd_data.duty);
        printf("load cali pvdd: vol %d, duty %d\n", pvdd_data.vol, pvdd_data.duty);
    }

#ifdef HEARTBEAT_ENABLE_DEFAULT
    reg_control_set(OFFSET_HEART_BEAT_SWITCH, 1);
#else
    reg_control_set(OFFSET_HEART_BEAT_SWITCH, 0);
#endif

    reg_control_set(OFFSET_TX_SIDE_ENABLE, DV8001_CHIP_ALL_ENABLE);

    reg_control_set(OFFSET_ERR_DISABLE_MASK, ERR_DISABLE_MASK_DEFAULT);
    reg_control_set(OFFSET_VOL_PVDD, s_boot_pvdd_mv / 10);
    reg_control_set(OFFSET_VOL_VOP, s_boot_vop_mv / 10);

    if(s_boot_powerup){
        //这行代码不能去掉，因为在main() while(1)里来执行上电动作
        reg_set_val(OFFSET_POWER_CTRL_ALL, 1);
    }else{
        reg_set_val(OFFSET_POWER_CTRL_ALL, 0);
    }

    return 0;
}

static void pvdd_current_overflow_callback(void)
{
    s_pvdd_oc_vol = pmic_get_pvdd_vol();
    s_pvdd_oc_current = pvdd_current;
    s_pvdd_oc_pmic_state = pmic_get_state();
    flag_pvdd_oc = 1;
    
}

static void pvdd_switch_callback(void)
{
    int16_t int16Val;
    txswt_reset_cur_subframe_index();

    //这里只是标志切换，实际切换PVDD是在第一帧
    if(s_pvdd_switch_ctx.frame_type == FRAME_TYPE_EVEN){
        s_pvdd_switch_ctx.frame_type = FRAME_TYPE_ODD;
        if(s_pvdd_switch_ctx.fixed_chn_flag == PVDD_CHN_SWITCH_ENABLE){
            txswt_switch_rx_expo_time(SEL_EXPO_TIME1);
        }
    }else{
        s_pvdd_switch_ctx.frame_type = FRAME_TYPE_EVEN;
        if(s_pvdd_switch_ctx.fixed_chn_flag == PVDD_CHN_SWITCH_ENABLE){
            txswt_switch_rx_expo_time(SEL_EXPO_TIME2);
        }
    }
    s_pvdd_switch_ctx.need_switch = 1;
    s_pvdd_switch_ctx.fsync_tick[s_pvdd_switch_ctx.frame_type] = bsp_tick_get();
    s_pvdd_switch_ctx.fsync_count++;
}

static void txswt_first_subframe_start_callback(void)
{
    if((s_pvdd_switch_ctx.is_stream_on)
    && (s_pvdd_switch_ctx.ctrl_state == PVDD_SWIO_OFF)){
        if(s_pvdd_switch_ctx.frame_type == FRAME_TYPE_ODD){
            //奇数帧切到强PVDD
            pvdd_switch_by_gpio(PVDD_SWIO_NORMAL);
        }else{
            //偶数帧切到弱PVDD
            pvdd_switch_by_gpio(PVDD_SWIO_SECOND);
        }
    }
    s_pvdd_switch_ctx.first_subframe_count++;
}

static void txswt_last_subframe_end_callback(void)
{
    //最后一个子帧用来关断两路PVDD
    if((s_pvdd_switch_ctx.is_stream_on) 
    && (pmic_get_state() == ADAPS_PMIC_POWER_UP_DONE)
    && s_pvdd_switch_ctx.need_switch){
        s_pvdd_switch_ctx.need_switch = 0;
        //这里关掉两路PVDD以重新切换PVDD
        //若是固定PVDD通道，则不用关
        if(s_pvdd_switch_ctx.fixed_chn_flag == PVDD_CHN_SWITCH_ENABLE){
            pvdd_switch_by_gpio(PVDD_SWIO_OFF);
        }
    }
    s_pvdd_switch_ctx.switch_tick[s_pvdd_switch_ctx.frame_type] = bsp_tick_get();
    s_pvdd_switch_ctx.last_subframe_count++;
}


int app_init()
{
    bsp_telm_init_param_t telm_param;
    bsp_enable_irq();
    hawk_tx_bsp_init();
    bsp_gpio_toggle(LED2_PORT, LED2_PIN);
    dump_support_option();
    dump_boot_option();
    boot_option_parse();
    app_default_config();
#ifndef HW_REMOVE_NST112
    nst112_init(); /*temperature sensor*/
#endif
#ifdef HW_HOST_BMI_CHIP
    imu_init();
#endif
    txswt_init();
    bsp_gpio_toggle(LED2_PORT, LED2_PIN);
    //demo4 1V1版本去掉了PVDD电流检测功能
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
        gpio_interrupt_set_cb(INA_CMP_PORT, INA_CMP_PIN, pvdd_current_overflow_callback);
    }
    
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_SWIO)){
        //demo4 1V1版本支持PVDD切换，用到FSYNC信号、第一与最后一个子帧
        txswt_set_callback(TXSWT_FIRST_SUBFRAME_START, txswt_first_subframe_start_callback);
        txswt_set_callback(TXSWT_LAST_SUBFRAME_END, txswt_last_subframe_end_callback);
        gpio_interrupt_set_cb(RX_FSYNC_PORT, RX_FSYNC_PIN, pvdd_switch_callback);
    }

    /*timer regist*/
    timer_cb_regist(temperature_update);
    timer_cb_regist(voltage_update_periodly);
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
        timer_cb_regist(ina201_update);
    }
    timer_cb_regist(heartbeat_proc);
    timer_cb_regist(check_txswt_state);
#ifndef HW_REMOVE_RUN_LED
    timer_cb_regist(led_control);
#endif
    timer_cb_regist(Upgrade_Program);
    //demo4 1V1版本去掉了PVDD电流检测功能
    if(bsp_hw_check_feature(HW_FEATURE_PVDD_CURRENT)){
        timer_cb_regist(pvdd_current_update);
    }
    bsp_gpio_toggle(LED2_PORT, LED2_PIN);

    //所有模块都初始化完成后才使能telm通讯
    telm_param.msg_cb = bsp_telm_msg_callback;
    telm_param.evt_cb = bsp_telm_evt_callback;
    bsp_telm_init(&telm_param);

    return 0;
}

int app_run(void)
{
    int32_t i32tmp;
    int16_t i16val;

    //test_pvdd_calibration();

    reg_status_update(OFFSET_RUN_STATE, ADAPS_NORMAL_STATE);
    while (1){

        //pmic power ctrl
        if (reg_get_flag(OFFSET_POWER_CTRL_ALL)){
            reg_reset_flag(OFFSET_POWER_CTRL_ALL, REG_FLAG_ALL);
            vop_pid_context_reset();
            pvdd_pid_context_reset();
            if (reg_val_get(OFFSET_POWER_CTRL_ALL)){
                //如果不是上电完成状态，就清除warning，重新检测
                if(pmic_get_state() != ADAPS_PMIC_POWER_UP_DONE){
                    reg_warn_code_update(ADAPS_PVDD_VOLTAGE_WARN | ADAPS_VOP_VOLTAGE_WARN, 0);
                }
                pmic_powerup_all(voltage_update);
                txswt_open();
                txswt_notify_pvdd_changed(reg_val_get(OFFSET_VOL_PVDD));
#ifndef HW_REMOVE_NST112
                //demo3 TX板上的温感芯片由VCC_3V3供电，上电后要重新初始化温感芯片
                nst112_init();
#endif
                printf("pmic all power up done\n");
            }else{
                uint32_t pdown_tick = 0;
                txswt_close();
                //如果处于保护状态，则不清除状态，以保留现场
                if(reg_val_get(OFFSET_RUN_STATE) != ADAPS_PROTECT_STATE){
                    txswt_state_clean();
                }
                //如果不是下电完成状态，就清除warning，重新检测
                if(pmic_get_state() != ADAPS_PMIC_POWER_DOWN_DONE){
                    reg_warn_code_update(ADAPS_PVDD_VOLTAGE_WARN | ADAPS_VOP_VOLTAGE_WARN, 0);
                }
                pmic_powerdown_all(voltage_update);
                pdown_tick = bsp_tick_get();
                printf("pmic all power down done at %d\n", pdown_tick);
                printf("time diff %d\n", pdown_tick - err_int_trigger_tick);
            }
        }

        timer_periodcallback();
        bsp_telm_state_check();
        //vop /pvdd ctrl
        if(pmic_get_state() == ADAPS_PMIC_POWER_UP_DONE){

            if(reg_get_flag(OFFSET_VOL_PVDD)){
                reg_reset_flag(OFFSET_VOL_PVDD, REG_FLAG_ALL);
                pvdd_pid_context_reset();
                i32tmp = reg_val_get(OFFSET_VOL_PVDD);
                pmic_adjust_pvdd_vol(i32tmp);
                printf("[%u]pmic adjust pvdd to %d mv done\n", bsp_tick_get(), i32tmp * 10);
                txswt_notify_pvdd_changed(i32tmp);
            }

#ifdef HW_HAVE_PVDDL
            if(reg_get_flag(OFFSET_PVDD_4_WEAK_LASER)){
                reg_reset_flag(OFFSET_PVDD_4_WEAK_LASER, REG_FLAG_ALL);
                //pvdd_pid_context_reset();
                i32tmp = reg_val_get(OFFSET_PVDD_4_WEAK_LASER);
                pmic_adjust_pvdd2_vol(i32tmp);
                printf("[%u]pmic adjust pvdd2 to %d mv done\n", bsp_tick_get(), i32tmp * 10);
            }
#endif

            if(reg_get_flag(OFFSET_VOL_VOP)){
                reg_reset_flag(OFFSET_VOL_VOP, REG_FLAG_ALL);
                vop_pid_context_reset();
                i32tmp = reg_val_get(OFFSET_VOL_VOP);
                pmic_adjust_vop_vol(i32tmp);
                printf("[%u]pmic adjust vop to %d mv done\n", bsp_tick_get(), i32tmp * 10);
            }
        
            if(s_vop_pid_enable){
                vop_voltage_control();
            }

            if(s_pvdd_pid_enable){
                pvdd_voltage_control();
            }

        }

        //vcsel enable ctrl
        if(reg_get_flag(OFFSET_VCSEL_ENABLE)){
            reg_reset_flag(OFFSET_VCSEL_ENABLE, REG_FLAG_ALL);
            if(reg_val_get(OFFSET_VCSEL_ENABLE)){
                txswt_enable();
                printf("txswt enable\n");
            }else{
                txswt_disable();
                printf("txswt disable\n");
            }
        }

        //tx side ctrl
        if(reg_get_flag(OFFSET_TX_SIDE_ENABLE)){
            reg_reset_flag(OFFSET_TX_SIDE_ENABLE, REG_FLAG_ALL);
            i16val = reg_val_get(OFFSET_TX_SIDE_ENABLE);
            if((0 <= i16val) && (i16val <= DV8001_CHIP_ALL_ENABLE)){
                dv8001_chip_enable(i16val);
                printf("tx side ctrl %d ok\n", i16val);
            }else{
                printf("invalid tx side ctrl %d \n", i16val);
            }
        }

        if(reg_get_flag(OFFSET_TX_FIXED_ZONE)){
            reg_reset_flag(OFFSET_TX_FIXED_ZONE, REG_FLAG_ALL);
            txswt_set_fixed_zone(reg_val_get(OFFSET_TX_FIXED_ZONE));
        }

        if(reg_get_flag(OFFSET_TX_HIT_ZONE)){
            reg_reset_flag(OFFSET_TX_HIT_ZONE, REG_FLAG_ALL);
            txswt_set_hit_zone(reg_val_get(OFFSET_TX_HIT_ZONE));
        }

        if(reg_get_flag(OFFSET_VROLL_OFFSET)){
            reg_reset_flag(OFFSET_VROLL_OFFSET, REG_FLAG_ALL);
            i16val = reg_val_get(OFFSET_VROLL_OFFSET);
            txswt_set_vroll_offset(i16val);
            printf("set_vroll_offset %d\n", i16val);
        }

        if(reg_get_flag(OFFSET_VROLL_NUM)){
            reg_reset_flag(OFFSET_VROLL_NUM, REG_FLAG_ALL);
            i16val = reg_val_get(OFFSET_VROLL_NUM);
            txswt_set_subframe_count(i16val);
            printf("set_vroll_num %d\n", i16val);
            //vroll num更新后vroll offset计算方式可能会变，需刷新offset
            txswt_set_vroll_offset(reg_val_get(OFFSET_VROLL_OFFSET));
        }

        //pvdd cali
        if(reg_get_flag(OFFSET_CALI_PVDD_CMD)){
            reg_reset_flag(OFFSET_CALI_PVDD_CMD, REG_FLAG_ALL);
            i16val = reg_val_get(OFFSET_CALI_PVDD_CMD);
            switch(i16val){
                case ADAPS_CALI_PVDD_RESET:
                    reg_set_val(OFFSET_CALI_PVDD_STATE, 0);
                break;
                case ADAPS_CALI_PVDD_START:
                    i16val = reg_val_get(OFFSET_CALI_PVDD_TARGET_VOL);
                    i32tmp = i16val * 10;
                    if((i32tmp < PVDD_CALI_VOL_MIN) || (i32tmp > PVDD_CALI_VOL_MAX)){
                        reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_FAIL | ADAPS_CALI_PVDD_ERR_ILLEGAL_VOL);
                        printf("cali pvdd: illegal vol %d\n", i32tmp);
                        break;
                    }
                    if(pmic_get_state() == ADAPS_PMIC_POWER_DOWN_DONE){
                        pmic_powerup_all(voltage_update);
                        bsp_delay_ms(500);
                        pvdd_calibration(i32tmp);
                        pmic_powerdown_all(voltage_update);
                    }else{
                        pvdd_calibration(i32tmp);
                    }

                    
                break;
                case ADAPS_CALI_PVDD_DELETE:
                    reg_set_val(OFFSET_CALI_PVDD_SAVED_VOL, 0);
                    reg_set_val(OFFSET_CALI_PVDD_SAVED_DUTY, 0);
                    if(cali_pvdd_data_save(0, 0)){
                        reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_STATE_DELETE_FAIL);
                    }
                break;

                default:
                    reg_set_val(OFFSET_CALI_PVDD_STATE, ADAPS_CALI_PVDD_ERR_ILLEGAL_CMD);
                    printf("cali pvdd: invalid cmd 0x%x\n", i16val);
                break;
            }

        }

        //tx chn short protect enable ctrl
        if(reg_get_flag(OFFSET_TX_CHN_SHORT_ENABLE)){
            reg_reset_flag(OFFSET_TX_CHN_SHORT_ENABLE, REG_FLAG_ALL);
            txswt_chn_short_protect_enable(reg_val_get(OFFSET_TX_CHN_SHORT_ENABLE));
            txswt_chip_config();
        }
        
        // err state proc
        if(reg_get_flag(OFFSET_ERR_ACK)){
            reg_reset_flag(OFFSET_ERR_ACK, REG_FLAG_ALL);
            if(reg_val_get(OFFSET_ERR_ACK) != reg_val_get(OFFSET_ERR_CODE)){
                gpio_write(RK_ERR_PORT, RK_ERR_PIN, GPIO_PIN_SET);
                report_err_by_uart();
            }
        }

        // quit protect state check
        if((reg_get_flag(OFFSET_ERR_CODE) & REG_FLAG_CTRL)
        && (reg_val_get(OFFSET_RUN_STATE) == ADAPS_PROTECT_STATE)){
            sys_exit_abnormal_state();
        }

        // update rx expo time
        if(reg_get_flag(OFFSET_MAX_SUBFRAME_EXP_TIME)){
            reg_reset_flag(OFFSET_MAX_SUBFRAME_EXP_TIME, REG_FLAG_ALL);
            txswt_set_rx_expo_time1(reg_val_get(OFFSET_MAX_SUBFRAME_EXP_TIME));
        }

        if(reg_get_flag(OFFSET_MAX_WEAK_SUBFRAME_EXP_TIME)){
            reg_reset_flag(OFFSET_MAX_WEAK_SUBFRAME_EXP_TIME, REG_FLAG_ALL);
            txswt_set_rx_expo_time2(reg_val_get(OFFSET_MAX_WEAK_SUBFRAME_EXP_TIME));
        }

        if(reg_get_flag(OFFSET_TX_CHN_RESET_PERIODLY_EN)){
            reg_reset_flag(OFFSET_TX_CHN_RESET_PERIODLY_EN, REG_FLAG_ALL);
            i16val = reg_val_get(OFFSET_TX_CHN_RESET_PERIODLY_EN);
            txswt_chn_reconfig_enable(i16val);
            printf("txswt chn reconfig enable %d\n", i16val);
        }
        
        usmart_dev.scan();
#ifdef HW_HAVE_SPI_FLASH
        flash_op_proc();
#endif
#ifdef HW_HOST_BMI_CHIP
        imu_proc();
#endif
    }

    return 0;
}

void printenv(void)
{
    pvdd_switch_ctx_t pvdd_switch_ctx;
    printf("\r\n");
    printf("Hardware info: %s, name: %s(%s), tag: 0x%x, variant: %s\r\n", s_hardware_version, s_hardware_name, DEMO_BOARD_ALIAS, DEMO_BOARD_TAG, bsp_hw_variant_name());
    printf("Software version: %s\r\n", s_software_version);
    printf("Build Time:       %s\r\n", s_build_time);
    printf("Boot Option Flag: 0x%X\r\n", s_boot_option_set_flag);
    printf("Debug flag:       %d\r\n", get_dbgflag());
    printf("Heartbeat count:  %d\r\n", s_heartbeat_count);
    printf("\r\n");
    printf("TemperatureTx:    %f degree\r\n", s_temperature_tx_borad);
#ifndef HW_REMOVE_RX_TEMP
    printf("TemperatureRx:    %f degree\r\n", s_temperature_rx_borad);
#endif
    printf("run state:       %d\r\n", reg_val_get(OFFSET_RUN_STATE));
    printf("err code:        0x%x\r\n", reg_val_get(OFFSET_ERR_CODE));
    printf("warn code:        0x%x\r\n", reg_val_get(OFFSET_WARN_CODE));
    printf("pimc state:       %s\r\n", pmic_get_state_string(pmic_get_state()));
#ifndef HW_REMOVE_VREF_ADC
    printf("vref voltage:     %d mv\r\n", sU32VoltageVref);
#endif
#ifndef HW_REMOVE_DV11_ADC
    printf("Dv11 voltage:     %d mv\r\n", voltage_dv11);
#endif
#ifndef HW_REMOVE_SV11_ADC
    printf("Sv11 voltage:     %d mv\r\n", voltage_sv11);
#endif
    printf("vop voltage:      %d mv (pwm: %d, pid:%s, wave:%s)\r\n", 
        voltage_vop, pmic_get_vop_duty(), s_vop_pid_enable ? "enable" : "disable",
        pmic_wave_state_str(pmic_vop_wave_state()));
    printf("pvdd voltage:     %d mv (pwm: %d, pid:%s, wave:%s)\r\n", 
        voltage_pvdd, pmic_get_pvdd_duty(), s_pvdd_pid_enable ? "enable" : "disable",
        pmic_wave_state_str(pmic_pvdd_wave_state()));
    printf("Pvdd current:     %d mA (overcurrent: %s)\r\n", pvdd_current, GPIO_PIN_RESET == ina201_get_alarm_flag() ? "false" : "true");
    if(flag_pvdd_oc){
        printf("###Pvdd current overflow, oc vol %d, oc current %d, oc pmic state: %s\r\n", 
            s_pvdd_oc_vol, s_pvdd_oc_current, pmic_get_state_string(s_pvdd_oc_pmic_state));
    }
    pvdd_cali_info_print();
#ifdef HW_HAVE_PVDDL
    printf("pvdd2 voltage:     %d mv (pwm: %d, pid:%s, wave:%s)\r\n", 
        voltage_pvdd2, pmic_get_pvdd2_duty(), s_pvdd2_pid_enable ? "enable" : "disable",
        pmic_wave_state_str(pmic_pvdd2_wave_state()));
#endif
#ifdef HW_HAVE_PVDD_ICT
    printf("pvdd ict voltage:     %d mv\n", voltage_pvdd_ict);
#endif
#ifdef HW_HAVE_SPI_FLASH
    printf("flash fsm state: %d\r\n", flash_op_ctx.fsm_state);
    printf("flash op flag: %d\r\n", flash_op_ctx.flag);
    printf("flash op offset: %d\r\n", flash_op_ctx.offset);
#endif
    bsp_disable_irq();
    pvdd_switch_ctx = s_pvdd_switch_ctx;
    bsp_enable_irq();
    if(!bsp_hw_check_feature(HW_FEATURE_PVDD_SWIO)){
        printf("NOT have pvdd switch feature\n");
    }else{
        printf("Have pvdd switch (by gpio) feature\n");
    }
    printf("pvdd gpio ctrl state:%d, fixed chn flag: %d\n", pvdd_switch_ctx.ctrl_state, pvdd_switch_ctx.fixed_chn_flag);
    printf("is stream on : %d, cur frame type: %d\n", pvdd_switch_ctx.is_stream_on, pvdd_switch_ctx.frame_type);
    printf("fsync count: %d\n", pvdd_switch_ctx.fsync_count);
    printf("fsync tick:[%u],[%u]\n", pvdd_switch_ctx.fsync_tick[0], pvdd_switch_ctx.fsync_tick[1]);
    printf("swtich tick:[%u],[%u]\n", pvdd_switch_ctx.switch_tick[0], pvdd_switch_ctx.switch_tick[1]);
    printf("first subframe count: %d\n", pvdd_switch_ctx.first_subframe_count);
    printf("last subframe count: %d\n", pvdd_switch_ctx.last_subframe_count);
    printf("\r\n");
}

//for debug
void pwm_set_vop_duty(uint32_t duty)
{
    pwm_set_duty(PWM_VOP_CHANNEL, duty);
}

void set_warn_code(uint16_t warn_code)
{
    reg_control_set(OFFSET_WARN_CODE, warn_code);
}

//for debug
void pwm_set_pvdd_duty(uint32_t duty)
{
    pwm_set_duty(PWM_PVDD_CHANNEL, duty);
}

static int app_upgrade_trigger(void)
{
    if (0XB == reg_val_get(OFFSET_UPGRADE_PROGRAM_FLAG_U16))
    {
        bsp_upgrade_trigger();
    }
    return 0;
}

void vop_pid_enable(uint32_t enable)
{
    if(s_vop_pid_enable != enable){
        pmic_adjust_vop_vol(reg_val_get(OFFSET_VOL_VOP));
        vop_pid_context_reset();
        s_vop_pid_enable = enable;
        printf("\nvop pid %s\n", s_vop_pid_enable ? "enable" : "disable");
    }
}

void pvdd_pid_enable(uint32_t enable)
{
    if(s_pvdd_pid_enable != enable){
        pmic_adjust_pvdd_vol(reg_val_get(OFFSET_VOL_PVDD));
        pvdd_pid_context_reset();
        s_pvdd_pid_enable = enable;
        printf("\npvdd pid %s\n", s_pvdd_pid_enable ? "enable" : "disable");
    }
}

