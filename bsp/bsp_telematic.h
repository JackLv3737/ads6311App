/**
 * @file bsp_telematic.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __BSP_TELEMATIC_H__
#define __BSP_TELEMATIC_H__
#include <stdint.h>

#define BSP_TELM_TX_LEN_MAX 3

typedef enum bsp_telm_evt_e{
    BSP_TELM_EVT_TX_DONE = 0,
    BSP_TELM_EVT_RX_DONE,
    BSP_TELM_EVT_MASS_TX_DONE,
    BSP_TELM_EVT_MASS_RX_DONE,
    BSP_TELM_EVT_TX_TIMEOUT,
    BSP_TELM_EVT_RX_TIMEOUT,
    BSP_TELM_EVT_MASS_TX_TIMEOUT,
    BSP_TELM_EVT_MASS_RX_TIMEOUT
}bsp_telm_evt_t;

//@return the length of tx msg
typedef int (*bsp_telm_msg_cb_t)(uint8_t *rx_msg, uint16_t rx_len, uint8_t *tx_msg);
typedef void (*bsp_telm_evt_cb_t)(bsp_telm_evt_t evt);

typedef struct bsp_telm_init_param_s{
    bsp_telm_msg_cb_t msg_cb;
    bsp_telm_evt_cb_t evt_cb;
}bsp_telm_init_param_t;

int bsp_telm_init(bsp_telm_init_param_t *param);

int bsp_telm_mass_revc(uint8_t *buf, uint16_t rx_len);
int bsp_telm_mass_send(uint8_t *buf, uint16_t tx_len);


void bsp_telm_state_check(void);

void bsp_telm_state_print(void);

uint32_t bsp_telm_msg_count(void);


#endif /*__BSP_TELEMATIC_H__*/
