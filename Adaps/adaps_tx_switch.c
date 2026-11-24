/**
 * @file adaps_tx_switch.c
 * @author rongchao (rongchao.xu@adaps-ph.com)
 * @brief 
 * @version 0.1
 * @date 2023-09-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdio.h>
#include <system_config.h>
#include "bsp_gpio.h"
#include "bsp_basic.h"
#include "bsp_timer.h"
#include "bsp_spi.h"

#include "adaps_tx_switch.h"
#include "adaps_dv8001_driver.h"

#define FIXED_CHN_NUM   16

#define TXSWT_TIMER_SHUTDOWN_ENABLE

#define TXSWT_EXPO_TIME_DEFAULT 1000 //unit:us
#define TXSWT_CHN_ON_MARGIN_TIME    150 //unit:us

enum TXSWT_TIMER_STATE_e
{
    TXSWT_TIMER_STATE_STOP = 0,
    TXSWT_TIMER_STATE_RUNNING,
    TXSWT_TIMER_STATE_STOPPING,
    TXSWT_TIMER_STATE_TIMEOUT,
};

#define TXSWT_REC_NUM   32
#define TXSWT_VROLL_CHECK_CNT 2
#define TXSWT_VROLL_DEC_ERR 0xFF

#define TXSWT_DV8001_RECONFIG_PERIOD    250 //unit:us
#define TXSWT_DV8001_RECONFIG_TIMES_CALC(m_expo_time)  (((m_expo_time) / TXSWT_DV8001_RECONFIG_PERIOD) - 1)

#define TXSWT_CLOSE_CHN_TIMER   BSP_TIMER_CHN0


#define TXSWT_SHUTDOWN_TIMER_START()  do{\
    if(!s_txswt_timer_enable) break;\
    if((s_rx_expo_time_using_flag != SEL_EXPO_TIME1) && s_rx_expo_time2_us){ \
        bsp_timer_oneshot_start(TXSWT_CLOSE_CHN_TIMER, s_rx_expo_time2_us + TXSWT_CHN_ON_MARGIN_TIME, txswt_shutdown_chn_timer_cb);\
    }else{\
        bsp_timer_oneshot_start(TXSWT_CLOSE_CHN_TIMER, s_rx_expo_time1_us + TXSWT_CHN_ON_MARGIN_TIME, txswt_shutdown_chn_timer_cb);\
    }\
    s_txswt_timer_state = TXSWT_TIMER_STATE_RUNNING;\
}while(0)


#define TXSWT_RECONFIG_TIMER_START()  do{\
    if(!s_txswt_timer_enable) break;\
    bsp_timer_oneshot_start(TXSWT_CLOSE_CHN_TIMER, TXSWT_DV8001_RECONFIG_PERIOD, txswt_reconfig_chn_timer_cb);\
    s_txswt_timer_state = TXSWT_TIMER_STATE_RUNNING;\
}while(0)


/* m_check_exp表示是否检查timer状态 */
#define TXSWT_TIMER_STOP(m_check_exp) do{\
    uint32_t unexpected_count;\
    uint32_t last_state;\
    if(!s_txswt_timer_enable) break;\
    unexpected_count = s_txswt_timer_unexpected_count;\
    last_state = s_txswt_timer_state;\
    s_txswt_timer_state = TXSWT_TIMER_STATE_STOPPING;\
    if(m_check_exp && (last_state == TXSWT_TIMER_STATE_RUNNING) && (s_txswt_timer_unexpected_count == unexpected_count)){\
        s_txswt_timer_unexpected_count++;\
    }\
    bsp_timer_oneshot_stop(TXSWT_CLOSE_CHN_TIMER);\
    s_txswt_timer_state = TXSWT_TIMER_STATE_STOP;\
}while(0)

static void txswt_action_dummy(void);

static const gpio_unit_t txswt_vroll_gpio[] = {
    GPIO_CONFIG(RX_VROLL0_PORT, RX_VROLL0_PIN),
    GPIO_CONFIG(RX_VROLL1_PORT, RX_VROLL1_PIN),
    GPIO_CONFIG(RX_VROLL2_PORT, RX_VROLL2_PIN),
    GPIO_CONFIG(RX_VROLL3_PORT, RX_VROLL3_PIN),
    GPIO_CONFIG(RX_VROLL4_PORT, RX_VROLL4_PIN)
};

static int s_txswt_is_open = 0;
static int s_txswt_is_enable = 1;
static int s_txswt_is_pause = 0;
//s_txswt_fixed_chn_list[0]=100 for auto roll
static uint32_t s_txswt_fixed_chn_list[FIXED_CHN_NUM];
static uint32_t s_txswt_fixed_mask = 0;

static uint32_t s_txswt_hit_chn = 0;
static uint32_t s_txswt_hit_multi_chn = 0;
static uint32_t s_txswt_req_count = 0;
static uint32_t s_txswt_vroll_dec_err_cnt = 0;
static uint8_t s_txswt_chn_rec_val[TXSWT_REC_NUM];
static uint32_t s_txswt_chn_rec_tick[TXSWT_REC_NUM];
static uint32_t s_rx_expo_time1_us = TXSWT_EXPO_TIME_DEFAULT;
static uint32_t s_rx_expo_time2_us = TXSWT_EXPO_TIME_DEFAULT;
static uint32_t s_rx_expo_time_using_flag = SEL_EXPO_TIME1;
static uint32_t s_txswt_ctrl_last_time = 0; //记录最近1次TX控制时间
#ifdef TXSWT_TIMER_ENABLE
static uint32_t s_txswt_timer_enable = 1;
#else
static uint32_t s_txswt_timer_enable = 0;
#endif
static uint32_t s_txswt_timer_state = TXSWT_TIMER_STATE_STOP;
static uint32_t s_txswt_timer_cb_exec_count = 0;
static uint32_t s_txswt_timer_unexpected_count = 0;

static uint32_t s_txswt_chn_reconfig_enable = 0;

static uint32_t s_dv8001_reconfigure_count = 0;
static uint32_t s_dv8001_reconfigure_times = TXSWT_DV8001_RECONFIG_TIMES_CALC(TXSWT_EXPO_TIME_DEFAULT);

static uint32_t s_txswt_subframe_count = 32;
static uint32_t s_txswt_cur_subframe_index = 0;

static uint32_t s_txswt_fov_mode = 0; // 0=对称，1=非对称
static uint32_t s_txswt_vroll_offset = 0;

static txswt_action_t s_txswt_last_subframe_end_action = txswt_action_dummy;
static txswt_action_t s_txswt_first_subframe_start_action = txswt_action_dummy;

static void txswt_action_dummy(void)
{

}

static uint8_t txswt_vroll_decode(void)
{
    int i;
    int cnt;
    const gpio_unit_t *gpio;
    uint8_t tmp;
    uint8_t value[TXSWT_VROLL_CHECK_CNT];
    //多次读vroll信号确认
    for(cnt = 0; cnt < TXSWT_VROLL_CHECK_CNT; cnt++){
        tmp = 0;
        for(i = 0; i < sizeof(txswt_vroll_gpio) / sizeof(gpio_unit_t); i++){
            gpio = &txswt_vroll_gpio[i];
            if(gpio_read(gpio->port, gpio->pin) == GPIO_PIN_SET){
                tmp |= (1 << i);
            }
        }
        if(cnt && (tmp != value[cnt - 1])){
            s_txswt_vroll_dec_err_cnt++;
            return TXSWT_VROLL_DEC_ERR;
        }
        value[cnt] = tmp;
    }

    return value[0] + 1;
}

static void txswt_shutdown_chn_timer_cb(void)
{
    if(s_txswt_timer_state != TXSWT_TIMER_STATE_STOPPING){
        s_txswt_timer_state = TXSWT_TIMER_STATE_TIMEOUT;
        if(s_txswt_is_open && s_txswt_is_enable && !s_txswt_is_pause){
            dv8001_shutdown_all_channels();
        }
    }else{
        s_txswt_timer_unexpected_count++;
    }
    if(s_txswt_cur_subframe_index >= s_txswt_subframe_count){
        s_txswt_last_subframe_end_action();
    }
    s_txswt_timer_cb_exec_count++;
}

static void txswt_reconfig_chn_timer_cb(void)
{
    uint32_t timer_is_start = 0;
    if(s_txswt_timer_state != TXSWT_TIMER_STATE_STOPPING){
        s_txswt_timer_state = TXSWT_TIMER_STATE_TIMEOUT;
        if(s_txswt_is_open && s_txswt_is_enable && !s_txswt_is_pause){
            dv8001_reconfigure_chn();
        }
        if(++s_dv8001_reconfigure_count < s_dv8001_reconfigure_times){
            TXSWT_RECONFIG_TIMER_START();
            timer_is_start = 1;
        }
    }else{
        s_txswt_timer_unexpected_count++;
    }
    if((!timer_is_start) && (s_txswt_cur_subframe_index >= s_txswt_subframe_count)){
        s_txswt_last_subframe_end_action();
    }
    s_txswt_timer_cb_exec_count++;
}


static void txswt_reply_ack(void)
{
    // 3. ACK高信号（1个脉冲）通知hawk芯片
    gpio_write(RX_SWTACK_PORT, RX_SWTACK_PIN, GPIO_PIN_SET);
    delay_us(1);

    // 4. 恢复ACK信号
    gpio_write(RX_SWTACK_PORT, RX_SWTACK_PIN, GPIO_PIN_RESET);
    
    // 5. 启动定时器，超时后(已曝光完)关闭通道
    if(s_txswt_chn_reconfig_enable){
        TXSWT_RECONFIG_TIMER_START();
    }else{
        TXSWT_SHUTDOWN_TIMER_START();
    }


}

static void txswt_req_callback(void)
{
    uint8_t chn;
    uint8_t vroll_code;
    uint32_t channel_mask;

	s_dv8001_reconfigure_count = 0;

    //期望：执行到这里时，timer是stop的，即提前关掉所有通道
    TXSWT_TIMER_STOP(1);
    s_txswt_cur_subframe_index++;
    if(s_txswt_cur_subframe_index == 1){
        s_txswt_first_subframe_start_action();
    }

    // 1. VROLLx MCU译码32ch垂直通道选择
    vroll_code = txswt_vroll_decode();
    
    if(s_txswt_is_open && s_txswt_is_enable && !s_txswt_is_pause){

        dv8001_uvlo60_vth_update();
        chn = vroll_code + s_txswt_vroll_offset;
        if((vroll_code == TXSWT_VROLL_DEC_ERR)
        || (0 == vroll_code)
        || (chn > DV8001_CHAN_USE_COUNT)){
            //如果译码错误，或vcsel不使能，则关闭所有通道
            dv8001_all_channels_disable();
        }else{

    // 2. dv8001通道切换
            if(s_txswt_fixed_chn_list[0]){
                if(s_txswt_fixed_chn_list[0] >= 100){
                    static uint16_t auto_roll_chn = 0;
                    dv8001_set_channel(auto_roll_chn + 1, txswt_reply_ack);
                    auto_roll_chn = (auto_roll_chn + 1) % DV8001_CHAN_USE_COUNT;
                }else{
                    dv8001_set_mutli_channel(s_txswt_fixed_mask, s_txswt_fixed_mask, txswt_reply_ack);
                }
            }else if(s_txswt_hit_chn){
                if(chn == s_txswt_hit_chn){
                    if(s_txswt_hit_multi_chn){
                        dv8001_set_mutli_channel(s_txswt_hit_multi_chn, s_txswt_hit_multi_chn, txswt_reply_ack);
                    }else{
                        dv8001_set_channel(chn, txswt_reply_ack);
                    }
                }else{
                    dv8001_all_channels_disable();
                    txswt_reply_ack();
                }
            }else{
#ifdef MODE_1D5_ENABLE
                // 对于1.5D方式，通过HROLL0来确认是左边发光还是右边发光
                if(gpio_read(RX_HROLL0_PORT, RX_HROLL0_PIN)){
                    dv8001_set_mutli_channel(0, 1 << (chn - 1), txswt_reply_ack);
                    chn += 100;
                }else{
                    dv8001_set_mutli_channel(1 << (chn - 1), 0, txswt_reply_ack);
                }
#else
                dv8001_set_channel(chn, txswt_reply_ack);
#endif
            }
        }
        s_txswt_ctrl_last_time = bsp_tick_get();
    }else{
        txswt_reply_ack();
    }

    //记录vroll译码值
    s_txswt_chn_rec_val[s_txswt_req_count % TXSWT_REC_NUM] = vroll_code;
    //s_txswt_chn_rec_tick[s_txswt_req_count % TXSWT_REC_NUM] = HAL_GetTick();
    s_txswt_req_count++;

}


int txswt_init(void)
{   
    dv8001_init();
    gpio_interrupt_set_cb(TX_REQ_PORT, TX_REQ_PIN, txswt_req_callback);
    return 0;
}

void txswt_print_state(void)
{
    int i;
    uint8_t chn;
    uint8_t chn_rec_val[TXSWT_REC_NUM];
    uint32_t rec_cnt;
    printf("\ntxswt open: %d\n", s_txswt_is_open);
    printf("txswt enable: %d\n", s_txswt_is_enable);
    printf("txswt chn reconfig enable: %d\n", s_txswt_chn_reconfig_enable);
    if(s_txswt_timer_enable){
        printf("txswt rx expo time: time1 %d us, time2 %d us, using %d\n", s_rx_expo_time1_us, s_rx_expo_time2_us, s_rx_expo_time_using_flag);
        printf("txswt timer shutdown enable, time margin %d us\n", TXSWT_CHN_ON_MARGIN_TIME);
        printf("txswt timer state:%d\n", s_txswt_timer_state);
        printf("txswt timer cb exec count:%d\n", s_txswt_timer_cb_exec_count);
        printf("txswt timer unexpected count %d\n", s_txswt_timer_unexpected_count);
    }else{
        printf("txswt timer shutdown disable\n");
    }
    printf("txswt ctrl last time: %d\n", s_txswt_ctrl_last_time);
    printf("txswt vroll offset: 0x%x\n", s_txswt_vroll_offset);
    printf("txswt vroll state: 0x%x\n", txswt_vroll_decode());
    printf("txswt hroll0 state: 0x%x\n", gpio_read(RX_HROLL0_PORT, RX_HROLL0_PIN));
    printf("txswt swt req state: %d\n", gpio_read(TX_REQ_PORT, TX_REQ_PIN));
    printf("txswt req count:%u\n", s_txswt_req_count);
    printf("txswt vroll dec err cnt:%u\n", s_txswt_vroll_dec_err_cnt);
    printf("txswt fixed chn mask:0x%x\n", s_txswt_fixed_mask);
    printf("txswt fixed chn list:");
    for(i = 0; i < FIXED_CHN_NUM - 1; i++){
        printf("%d,", s_txswt_fixed_chn_list[i]);
    }
    printf("%d\n", s_txswt_fixed_chn_list[i]);
    printf("txswt hit chn: %d\n", s_txswt_hit_chn);
    printf("txswt hit multi chn mask: 0x%x\n", s_txswt_hit_multi_chn);
    printf("txswt subframe count: %d\n", s_txswt_subframe_count);
    printf("txswt fov mode: %d\n", s_txswt_fov_mode);
    printf("txswt cur subframe index: %d\n", s_txswt_cur_subframe_index);
    printf("txswt chn rec:\n");
    if(s_txswt_req_count){
        bsp_disable_irq();
        rec_cnt = s_txswt_req_count;
        memcpy(chn_rec_val, s_txswt_chn_rec_val, rec_cnt > TXSWT_REC_NUM ? TXSWT_REC_NUM : rec_cnt);
        bsp_enable_irq();
        for(i = 0; (i < TXSWT_REC_NUM) && (i < rec_cnt); i++){
            chn = chn_rec_val[(rec_cnt + i) % TXSWT_REC_NUM];
            printf("%d[%d] ", chn % 100, chn / 100);
        }
        printf("\n");
    }else{
        printf("--none--\n");
    }
    dv8001_print_state();

}

int txswt_disable(void)
{
    //先把flag置为0，避免中断里执行spi操作
    s_txswt_is_enable = 0;
    TXSWT_TIMER_STOP(0);
    dv8001_all_channels_disable();

    return s_txswt_is_enable;
}

int txswt_enable(void)
{
    if(!s_txswt_is_enable){
        bsp_spi_init();
        txswt_notify_pvdd_changed(0);
    }
    s_txswt_is_enable = 1;
    return s_txswt_is_enable;
}

int txswt_is_enable(void)
{
    return s_txswt_is_enable;
}

int txswt_state_check(void)
{
    return dv8001_state_check();
}

int txswt_state_clean(void)
{
    return dv8001_state_clean();
}

void txswt_test(void)
{
    txswt_print_state();
    printf("========to test req process\n");
    txswt_req_callback();
    txswt_print_state();
}

//for debug
void txswt_set_hit_chn(uint32_t hit_chn)
{
    s_txswt_hit_multi_chn = 0;
    s_txswt_hit_chn = hit_chn;
}

void txswt_set_hit_zone(uint32_t hit_zone)
{
    printf("tx set hit zone: %d\n", hit_zone);
    s_txswt_hit_multi_chn = 0;
    if(0 == hit_zone){
        s_txswt_hit_chn = 0;
        dv8001_chip_enable(DV8001_CHIP_ALL_ENABLE);
    }else if(hit_zone <= DV8001_CHAN_USE_COUNT){
        s_txswt_hit_chn = hit_zone;
        dv8001_chip_enable(DV8001_CHIP_1EN_2DIS);
    }else if(hit_zone <= (2 * DV8001_CHAN_USE_COUNT)){
        s_txswt_hit_chn = hit_zone - DV8001_CHAN_USE_COUNT;
        dv8001_chip_enable(DV8001_CHIP_1DIS_2EN);
    }
}

void txswt_set_hit_two_chn(uint32_t chn1, uint32_t chn2)
{
    if(chn1 && chn2){
        s_txswt_hit_chn = (chn1 < chn2 ? chn1 : chn2);
        s_txswt_hit_multi_chn = (1 << (chn1 - 1)) | (1 << (chn2 - 1));
    }else{
        s_txswt_hit_chn = 0;
        s_txswt_hit_multi_chn = 0;
    }
}

void txswt_set_hit_multi_chn(uint32_t chn_mask)
{
    int i;
    if(chn_mask){
        for(i = 0; i < 32; i++){
            if(chn_mask & (1 << i)){
                s_txswt_hit_chn = i + 1;
                break;
            }
        }
        s_txswt_hit_multi_chn = chn_mask;
    }else{
        s_txswt_hit_chn = 0;
        s_txswt_hit_multi_chn = 0;
    }
}

//for roi cali
//fixed_chn:通道号，为0时表示恢复轮询模式(非固定通道发光)

static void txswt_update_fixed_chn_mask(void)
{
    int i;
    uint32_t chn;
    uint32_t mask = 0;
    for(i = 0; i < FIXED_CHN_NUM; i++){
        chn = s_txswt_fixed_chn_list[i];
        if(chn){
            mask |= (1 << (chn - 1));
        }
    }
    s_txswt_fixed_mask = mask;
}

void txswt_set_fixed_chn(uint32_t fixed_chn)
{
    s_txswt_fixed_chn_list[0] = fixed_chn;
    txswt_update_fixed_chn_mask();
}

void txswt_set_fixed_chn2(uint32_t fixed_chn)
{
    s_txswt_fixed_chn_list[1] = fixed_chn;
    txswt_update_fixed_chn_mask();
}

void txswt_set_fixed_chn3(uint32_t fixed_chn)
{
    s_txswt_fixed_chn_list[2] = fixed_chn;
    txswt_update_fixed_chn_mask();
}

void txswt_set_fixed_chn4(uint32_t fixed_chn)
{
    s_txswt_fixed_chn_list[3] = fixed_chn;
    txswt_update_fixed_chn_mask();
}

void txswt_config_fixed_chn(uint32_t index, uint32_t fixed_chn)
{
    if(index < FIXED_CHN_NUM){
        s_txswt_fixed_chn_list[index] = fixed_chn;
        txswt_update_fixed_chn_mask();
    }
}

void txswt_set_fixed_zone(uint32_t fixed_zone)
{
    printf("tx set fixed zone: %d\n", fixed_zone);
    if(0 == fixed_zone){
        txswt_set_fixed_chn(0);
        dv8001_chip_enable(DV8001_CHIP_ALL_ENABLE);
    }else if(fixed_zone <= DV8001_CHAN_USE_COUNT){
        txswt_set_fixed_chn(fixed_zone);
        dv8001_chip_enable(DV8001_CHIP_1EN_2DIS);
    }else if(fixed_zone <= (2 * DV8001_CHAN_USE_COUNT)){
        txswt_set_fixed_chn(fixed_zone - DV8001_CHAN_USE_COUNT);
        dv8001_chip_enable(DV8001_CHIP_1DIS_2EN);
    }
}


void txswt_notify_pvdd_changed(uint32_t pvdd_vol)
{
    dv8001_notify_pvdd_changed(pvdd_vol);
}

int txswt_set_rx_expo_time1(uint32_t time_us)
{
    if((time_us < 10) || (time_us > 4000)){
        printf("\nrx expo time1 is invalid, should in [100, 4000] us\n");
        return -1;
    }
    s_rx_expo_time1_us = time_us;
    s_dv8001_reconfigure_times = TXSWT_DV8001_RECONFIG_TIMES_CALC(time_us);
    return 0;
}

int txswt_set_rx_expo_time2(uint32_t time_us)
{
    if((time_us < 10) || (time_us > 4000)){
        printf("\nrx expo time is invalid, should in [100, 4000] us\n");
        return -1;
    }
    s_rx_expo_time2_us = time_us;
    return 0;
}

//flag: 0=using time1, others=using time2
int txswt_switch_rx_expo_time(uint32_t flag)
{
    s_rx_expo_time_using_flag = flag;
    return 0;
}

void txswt_timer_enable(uint32_t enable)
{
    if(!enable) TXSWT_TIMER_STOP(0);
    s_txswt_timer_enable = enable;
}

void txswt_chn_short_protect_enable(uint32_t enable)
{
    dv8001_chn_short_protect_enable(enable);
}

void txswt_chn_reconfig_enable(uint32_t enable)
{
    s_txswt_chn_reconfig_enable = enable;
}


void txswt_chip_config(void)
{
    if(s_txswt_is_open){
        //bsp_disable_irq();
        txswt_pause();
        dv8001_config();
        txswt_resume();
        //bsp_enable_irq();
    }
}

void txswt_open(void)
{
    printf("txswt_open() in, is open %d\n", s_txswt_is_open);
    if(!s_txswt_is_open){
        dv8001_open();
        s_txswt_is_open = 1;
    }
}

int txswt_is_open(void)
{
    return s_txswt_is_open;
}

void txswt_close(void)
{
    printf("txswt_close() in, is open %d\n", s_txswt_is_open);
    if(s_txswt_is_open){
        s_txswt_is_open = 0;
        dv8001_close();
    }
}

//用于主程序临时访问shark，避免主程序与中断同时访问shark造成不可意料的结果
void txswt_pause(void)
{
    s_txswt_is_pause = 1;
}

//用于主程序临时操作shark， 需与txswt_pause()成对调用
void txswt_resume(void)
{
    s_txswt_is_pause = 0;
}

//获取最近1次TX控制时间
uint32_t txswt_ctrl_last_time_get(void)
{
    return s_txswt_ctrl_last_time;
}

void txswt_set_subframe_count(uint32_t count)
{
    if(count){
        if(count > DV8001_CHAN_USE_COUNT){
            count = DV8001_CHAN_USE_COUNT;
        }
        s_txswt_subframe_count = count;
        s_txswt_fov_mode = 1;
    }else{ //对称FOV计算方式
        s_txswt_fov_mode = 0;
        s_txswt_vroll_offset = s_txswt_vroll_offset % (DV8001_CHAN_USE_COUNT / 2);
        s_txswt_subframe_count = DV8001_CHAN_USE_COUNT - (s_txswt_vroll_offset * 2);
    }
}

uint32_t txswt_get_cur_subframe_index(void)
{
    return s_txswt_cur_subframe_index;
}


void txswt_reset_cur_subframe_index(void)
{
    s_txswt_cur_subframe_index = 0;
}

void txswt_set_vroll_offset(uint32_t offset)
{
    if(s_txswt_fov_mode == 0){
        //对称FOV计算方式：
        s_txswt_vroll_offset = offset % (DV8001_CHAN_USE_COUNT / 2);
        s_txswt_subframe_count = DV8001_CHAN_USE_COUNT - (s_txswt_vroll_offset * 2);
    }else{
        s_txswt_vroll_offset = offset % DV8001_CHAN_USE_COUNT;
    }

}

void txswt_set_callback(uint8_t type, txswt_action_t cb)
{
    switch(type){
        case TXSWT_FIRST_SUBFRAME_START:
            s_txswt_first_subframe_start_action = cb;
        break;
        case TXSWT_LAST_SUBFRAME_END:
            s_txswt_last_subframe_end_action = cb;
        break;
        default:
        break;
    }

}




