/**
 * @file bsp_i2c.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_GPIO_H__
#include <stdint.h>
#include "board_config.h"

#ifndef GPIO_PIN_RESET
#define GPIO_PIN_RESET 0
#endif

#ifndef GPIO_PIN_SET
#define GPIO_PIN_SET 1
#endif

#define GPIO_CONFIG(m_port, m_pin)  {m_port, m_pin}

typedef void (*bsp_gpio_evt_cb_t)(void);

enum bsp_gpio_evt_flag_e
{
    BSP_GPIO_EVT_EDGE_RISE = 0,
    BSP_GPIO_EVT_EDGE_FALL,
    BSP_GPIO_EVT_EDGE_BOTH,
};

typedef struct gpio_unit_s
{
    gpio_port_t port;
    uint16_t pin;
}gpio_unit_t;

typedef int GPIO_PinState;


/**
 * @brief bsp gpio init
 * 
 * @return int 
 */
int bsp_gpio_init(void);

int bsp_gpio_read(gpio_port_t port, uint8_t pin);

int bsp_gpio_write(gpio_port_t port, uint8_t pin, uint8_t value);

int bsp_gpio_toggle(gpio_port_t port, uint8_t pin);

int bsp_gpio_register_event_callback(gpio_port_t port, uint16_t pin_num, bsp_gpio_evt_cb_t cb, uint8_t event_flag);

#define gpio_interrupt_set_cb(m_GPIOx, m_pin, m_cb) bsp_gpio_register_event_callback(m_GPIOx, m_pin, m_cb, 0)


//===========will remove below
#define gpio_read(m_GPIOx, m_pin)   bsp_gpio_read(m_GPIOx,m_pin)
#define gpio_write(m_GPIOx, m_pin, m_state)   bsp_gpio_write(m_GPIOx,m_pin, m_state)



#endif /*__BSP_GPIO_H__*/
