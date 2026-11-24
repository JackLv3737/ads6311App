/**
 * @file bsp_basic.c
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdio.h>
#include "userdefine.h"
#include "BAT32A239.h"
#include "gpio.h"
#include "sci.h"
#include "clk.h"
#include "bsp_basic.h"
#include "board_config.h"
#include "system_config.h"
#include "cm_backtrace.h"
#include "ring_buf.h"
#include "bsp_adc.h"
#include "bsp_pwm.h"
#include "common.h"

void IRQ14_Handler(void) __attribute__((alias("stdio_uart_isr")));

#define UART_RING_BUF_SIZE  256
#define UART_RX_BUF_LOCK()  __disable_irq()
#define UART_RX_BUF_UNLOCK()  __enable_irq()

static uint32_t s_clock_per_us = 0;
static volatile uint32_t s_sys_tick = 0;
static uint8_t s_uart_rx_buf[UART_RING_BUF_SIZE];
static ring_buf_t s_uart_ring_buf_ctrl;
#ifdef BOARD_DEMO4
static uint32_t s_hw_variant_ver = HW_VARIANT_VER_UNKNOWN;
#else
static uint32_t s_hw_variant_ver = HW_VARIANT_VER0;
#endif
static uint32_t s_hw_feature = 0;

static uint8_t *s_hw_variant_name[HW_VARIANT_COUNT] = {
  "unknown",
  "v0",
  "v1",
  "v2",
  "v3",
};

#if defined ( __CC_ARM   )
#if (__ARMCC_VERSION < 400000)
#else
// Insist on keeping widthprec, to avoid X propagation by benign code in C-lib
#pragma import _printf_widthprec
#endif
#endif

//
// C library retargetting
//

#if defined ( __CC_ARM )
struct __FILE { int handle; /* Add whatever you need here */ };
#endif
FILE __stdout;
FILE __stdin;

void stdio_uart_isr(void) {
    volatile uint8_t err_type;
    INTC_ClearPendingIRQ(SR1_IRQn);
    err_type = (uint8_t)(SCI0->SSR03 & 0x0007U);
    SCI0->SIR03 = (uint16_t)err_type;
    if (err_type != 0U) {
        err_type = SCI0->RXD1;//aviod overflow
        return;
    }

    s_uart_rx_buf[s_uart_ring_buf_ctrl.in] = SCI0->RXD1;
    ring_buf_add(&s_uart_ring_buf_ctrl);

}

static int bsp_stdio_uart_init(uint32_t freq) {
    // Use UART1
    MD_STATUS status;
    ring_buf_reset(&s_uart_ring_buf_ctrl, UART_RING_BUF_SIZE);
    status = UART1_Init(freq, 19200);
    if (status == MD_ERROR) {
        while (1)
            ;  // The baud rate cannot get at the current system clock
               // frequency.
    }
    // Not Use interrupt
    INTC_DisableIRQ(ST1_IRQn);      /* disable INTST1 interrupt */
    //INTC_DisableIRQ(SR1_IRQn);      /* disable INTSR1 interrupt */
    INTC_ClearPendingIRQ(ST1_IRQn); /* clear INTST1 interrupt flag */
    //INTC_ClearPendingIRQ(SR1_IRQn); /* clear INTSR1 interrupt flag */

    return 0;
}

static inline void bsp_stdio_uart_send(uint8_t ch)
{
    SCI0->TXD1 = ch;
    while (SCI0->SSR02 & (_0040_SCI_UNDER_EXECUTE | _0020_SCI_VALID_STORED))
    {
        ;
    }
}

static inline uint8_t bsp_stdio_uart_receive(void)
{
    while ((SCI0->SSR03 & _0020_SCI_VALID_STORED) == 0)
    {
        ;
    }
    return SCI0->RXD1;
}

void _ttywrch(int ch)
{
  bsp_stdio_uart_send(ch);
  return;
}

int fputc(int ch, FILE *f)
{
  bsp_stdio_uart_send(ch);
  if(ch == '\n')
  {
    bsp_stdio_uart_send('\r');
  }
  return ch;
}

int fgetc( FILE *f)
{
  int ch;
IN:
  while(ring_buf_is_empty(&s_uart_ring_buf_ctrl)){
    bsp_delay_ms(1);
  }
  UART_RX_BUF_LOCK();
  if(ring_buf_is_empty(&s_uart_ring_buf_ctrl)){
    UART_RX_BUF_UNLOCK();
    goto IN;
  }
  ch = s_uart_rx_buf[s_uart_ring_buf_ctrl.out];
  ring_buf_remove(&s_uart_ring_buf_ctrl);
  UART_RX_BUF_UNLOCK();

  return ch;
}

int ferror(FILE *f) {
  return EOF;
}

void _sys_exit(int return_code) {
	if(return_code) 
	{
		*((volatile uint32_t*)0x40028020) = 0x00000BAD;  /* simulation NG */
	}
	else
	{
		*((volatile uint32_t*)0x40028020) = 0x00000001;  /* simulation OK */
	}
  while (1);    /* endless loop */
}

void SysTick_Handler(void)
{
	s_sys_tick++;
}

static void bsp_basic_clock_config(void)
{
    //使用外部8M晶振， PLL到64M
    CLK_MainOsc_Setting(OSC_OSCILLATOR, OSC_UNDER_10M);
    CLK_PLL_Setting(PLL_SR_fMX, PLL_DIV_2, PLL_MUL_16);
	  CLK_PLL_Start();
    CLK_Fclk_Select(MAINCLK_FPLL);
	  while((CGC->MCKC & CGC_MCKC_CKSTR_Msk) == 0);
}

static void bsp_justify_hw_variant(void)
{
  #define SAMPLE_CNT 5
	uint32_t adc_val;
  uint32_t vol_val;
  uint32_t vol_val_rec[SAMPLE_CNT] = {0};
  uint32_t rec_index = 0;
  uint32_t try_count = 0;
  int i;
  printf("[%d]start to justify hw variant...\n", bsp_tick_get());
  bsp_pwm_init();
  bsp_pwm_set_duty(PWM_VOP_CHANNEL, PWM_VOP_ADJ_INIT_DUTY);
  PORT_Init(VOP_EN_PORT, VOP_EN_PIN, OUTPUT);
  PORT_SetBit(VOP_EN_PORT, VOP_EN_PIN);
  bsp_adc_init(BIT(ADC_CHANNEL_VOP));
  //实测硬件VOP从0V到最低绝对电压的耗时约30ms
  bsp_delay_ms(40);

  memset(vol_val_rec, 0, sizeof(vol_val_rec));
  while(try_count++ < 25){
    adc_val = adc_get_result_average(ADC_CHANNEL_VOP, 5);
    if(VOP_ADC_VAL_MIN < adc_val){
      vol_val = VOP_VOL_CALC(adc_val) * 1000;
    }else{
      vol_val = 0;
    }
    vol_val_rec[rec_index++] = vol_val;
    rec_index %= SAMPLE_CNT;
    if(vol_val != 0){
      for(i = 1; i < SAMPLE_CNT; i++){
        if(ABS(vol_val_rec[0], vol_val_rec[i]) > VOP_ERR_WARN_LIMIT){
          break;
        }
      }
      if(i >= SAMPLE_CNT){
        if(vol_val > VOP_CHECK_HW_VER_THRESHOLD){
          s_hw_variant_ver = HW_VARIANT_VER0;
          break;
        }else if(vol_val > (VOP_CHECK_HW_VER_THRESHOLD / 2)){
          s_hw_variant_ver = HW_VARIANT_VER1;
          break;
        }
      }  
    }
    bsp_delay_ms(5);
  }

  printf("[%d]@@@justified hw variant: %s, vop %d mv, adc %d, \n", bsp_tick_get(), s_hw_variant_name[s_hw_variant_ver], vol_val, adc_val);

  PORT_ClrBit(VOP_EN_PORT, VOP_EN_PIN);

  // bsp_delay_ms(10);
  
	// adc_val = adc_get_result_average(ADC_CHANNEL_VOP, 5);
  // if(VOP_ADC_VAL_MIN < adc_val){
  //   vol_val = VOP_VOL_CALC(adc_val) * 1000;
  // }else{
  //   vol_val = 0;
  // }
  // printf("=====vop %d mv, adc %d\n", vol_val, adc_val);

}

int bsp_basic_init(const char *hardware_ver, const char *software_ver)
{
    int clock_hz;
    bsp_basic_clock_config();
    SystemCoreClockUpdate();
    clock_hz = CLK_GetCoreFreq();
    SysTick_Config(clock_hz / 1000);
    s_clock_per_us = clock_hz / 1000000;
    s_sys_tick = 0;
    bsp_stdio_uart_init(clock_hz);
    cm_backtrace_init("HawkTx", hardware_ver, software_ver);

#ifdef BOARD_DEMO4
#ifdef HW_FIX_VARIANT_VER1
    s_hw_variant_ver = HW_VARIANT_VER1;
#else
    bsp_justify_hw_variant();
#endif
#endif

    switch(s_hw_variant_ver){
      case HW_VARIANT_VER0:
        s_hw_feature = HW_FEATURE_PVDD_CURRENT;
      break;
      case HW_VARIANT_VER1:
        s_hw_feature = HW_FEATURE_PVDD_SWIO;
      break;
      default:
      break;
    }

    return 0;
}

void bsp_delay_us(uint32_t us)
{
    uint32_t old_value;
    uint32_t now_value;
    uint32_t delay_clock = us * s_clock_per_us;
    uint32_t count_clock = 0;
    old_value = SysTick->VAL;
    while(count_clock < delay_clock){
      now_value = SysTick->VAL;
      if(now_value != old_value){
        if(now_value < old_value){
          count_clock += old_value - now_value; /* SYSTICK is a minus counter */
        }else{
          count_clock += SysTick->LOAD - now_value + old_value; 
        }
        old_value = now_value;
      }
    }
}

void bsp_delay_ms(uint32_t ms)
{
    uint32_t cur_tick = s_sys_tick;
    while(s_sys_tick - cur_tick < ms);
}

uint32_t bsp_tick_get(void)
{
    return s_sys_tick;
}

int bsp_stdio_getchar_unblock(void)
{
  int ch;
  if(ring_buf_is_empty(&s_uart_ring_buf_ctrl)){
    return -1;
  }
  UART_RX_BUF_LOCK();
  if(!ring_buf_is_empty(&s_uart_ring_buf_ctrl)){
    ch = s_uart_rx_buf[s_uart_ring_buf_ctrl.out];
    ring_buf_remove(&s_uart_ring_buf_ctrl);
  }else{
    ch = -1;
  }
  UART_RX_BUF_UNLOCK();
  return ch;
}

int bsp_stdio_getchar_timeout(uint32_t timeout_ms)
{
  int ch;
  uint32_t tick_in;

  if(timeout_ms == 0){
    return bsp_stdio_getchar_unblock();
  }

  tick_in = bsp_tick_get();
  while((bsp_tick_get() - tick_in) < timeout_ms){
    if(!ring_buf_is_empty(&s_uart_ring_buf_ctrl)){
      UART_RX_BUF_LOCK();
      if(!ring_buf_is_empty(&s_uart_ring_buf_ctrl)){
        ch = s_uart_rx_buf[s_uart_ring_buf_ctrl.out];
        ring_buf_remove(&s_uart_ring_buf_ctrl);
        UART_RX_BUF_UNLOCK();
        return ch;
      }
      UART_RX_BUF_UNLOCK();
    }
    bsp_delay_ms(1);
  }
  return -1;
}

void bsp_reboot(void)
{
    NVIC_SystemReset();
}

void bsp_enable_irq(void)
{
    __enable_irq();
    
}

void bsp_disable_irq(void)
{
    __disable_irq();

}

uint32_t bsp_hw_variant_ver(void)
{
  return s_hw_variant_ver;
}

uint8_t *bsp_hw_variant_name(void)
{
  return s_hw_variant_name[s_hw_variant_ver];
}

uint32_t bsp_hw_check_feature(uint32_t feature)
{
  return s_hw_feature & feature;
}

