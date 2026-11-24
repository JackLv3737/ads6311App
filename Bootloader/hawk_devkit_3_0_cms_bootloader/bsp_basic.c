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
#include <string.h>
#include "bsp_basic.h"

static uint32_t s_clock_per_us = 0;
static volatile uint32_t s_sys_tick = 0;

uint8_t u8Uart1RxBuf[USART_REC_LEN];
uint32_t u32UartRxCnt = 0;
uint8_t u8RxBuf[RXBUFFERSIZE];


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

static void bsp_uart_init(void) {
  write_bootloader_state(EN_BOOTLOADER_STATE_BSP_UART_INIT_START);
  MD_STATUS status;
  // TODO JUST FOR DEBUG
  // status = UART1_Init(freq, 19200);
  status = UART1_Init(CLK_GetCoreFreq(), 19200);

  if (status == MD_ERROR) {
    while (1)
      ; // The baud rate cannot get at the current system clock frequency.
  }
  // UART1_Start();
  UART1_Receive(u8Uart1RxBuf, USART_REC_LEN);
}

static int bsp_clk_init(void)
{
    int clock_hz;
    // bsp_basic_clock_config();

    // 使用外部8M晶振， PLL到64M
    CLK_MainOsc_Setting(OSC_OSCILLATOR, OSC_UNDER_10M);
    CLK_PLL_Setting(PLL_SR_fMX, PLL_DIV_2, PLL_MUL_16);
    CLK_PLL_Start();
    CLK_Fclk_Select(MAINCLK_FPLL);
    while ((CGC->MCKC & CGC_MCKC_CKSTR_Msk) == 0)
      ;

    SystemCoreClockUpdate();
    clock_hz = CLK_GetCoreFreq();
    SysTick_Config(clock_hz / 1000);
    s_clock_per_us = clock_hz / 1000000;
    s_sys_tick = 0;
    // bsp_stdio_uart_init(clock_hz);

    // cm_backtrace_init("HawkTx", hardware_ver, software_ver);

    return 0;
}

static int bsp_telm_msg_callback(uint8_t *rx_msg, uint16_t rx_len, uint8_t *tx_msg)
{
    int ret;
    int16_t val;
		uint16_t addr;
    if(rx_msg){
				addr = (rx_msg[0] & 0X7F) << 8 | ((uint16_t)rx_msg[1]);
        if(rx_msg[0] & 0x80){ //read reg
            if(!tx_msg){
                return -1;
            }
						
            val = reg_val_get(addr);
            // printf("%X", rx_msg[1]);
            tx_msg[0] = rx_msg[1]; //dummy
            tx_msg[1] = val >> 8;
            tx_msg[2] = val & 0xFF;
            return 3;//less than BSP_TELM_TX_LEN_MAX
        }else{
            reg_control_set(addr, (rx_msg[2] << 8) | rx_msg[3]);
            return 0;
        }
    }
    return -1;
}

static void bsp_spi_init(void) {
  bsp_telm_init_param_t telm_param;
  memset(&telm_param, 0, sizeof(bsp_telm_init_param_t));
  telm_param.msg_cb = bsp_telm_msg_callback;
  bsp_telm_init(&telm_param);
}

int bsp_init(void) {
  write_bootloader_state(EN_BOOTLOADER_STATE_BSP_INIT_START);
  bsp_clk_init();
  bsp_uart_init();
  bsp_spi_init();
#ifdef HW_TELM_USING_IIC

#define SV11_DV11_EN_PORT   1
#define SV11_DV11_EN_PIN    7
#define AV11_EN_PORT        1
#define AV11_EN_PIN         2
#define IOV18_EN_PORT       7
#define IOV18_EN_PIN        3

#define GPIO_SET_OUTPUT_HIGH(m_port, m_pin) do{\
  /* xx output high level */\
  (*((uint8_t *)(&(PORT->P0) + m_port))) |= (1 << m_pin);\
  /* xx is used as output */ \
  (*((uint8_t *)(&(PORT->PM0) + m_port))) &= (~(1 << m_pin));\
}while(0)

  //目前硬件需将IOV18_EN脚拉高才能I2C通讯;
  //稳妥起见，1.8V上电之前也把1.1V上电;
  /* xx output high level */
  GPIO_SET_OUTPUT_HIGH(SV11_DV11_EN_PORT, SV11_DV11_EN_PIN);
  GPIO_SET_OUTPUT_HIGH(AV11_EN_PORT, AV11_EN_PIN);
  GPIO_SET_OUTPUT_HIGH(IOV18_EN_PORT, IOV18_EN_PIN);
  printf("###warn: IOV18_EN pin(P73)、SV11_DV11_EN pin(P17)、AV11_EN pin(P12) is set high\n");
#endif
  return 0;
}

static inline void bsp_debug_uart_send(uint8_t ch) {
  SCI0->TXD1 = ch;
  while (SCI0->SSR02 & (_0040_SCI_UNDER_EXECUTE | _0020_SCI_VALID_STORED)) {
    ;
  }
}

static inline uint8_t bsp_debug_uart_receive(void) {
  while ((SCI0->SSR03 & _0020_SCI_VALID_STORED) == 0) {
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
  uint8_t ch = bsp_debug_uart_receive();
#if 1
  if(ch == 0x0D) // 回车键
  {
    bsp_debug_uart_send('\r'); // echo
    bsp_debug_uart_send('\n'); // echo
  }
  else {
    bsp_debug_uart_send(ch); // echo
  }
#endif
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

void bsp_enable_irq(void)
{
    __enable_irq();
    
}

void bsp_disable_irq(void)
{
    __disable_irq();

}
