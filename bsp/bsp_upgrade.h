/**
 * @file bsp_upgrade.h
 * @author rongchao
 * @brief 
 * @version 0.1
 * @date 2023-08-28
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#ifndef __BSP_UPGRADE_H__
#include <stdint.h>
#include "BAT32A239.h"
#include "clk.h"
#include "flash.h"
#include "sci.h"
#include "userdefine.h"
#include <stdio.h>

int bsp_upgrade_init(void);

int bsp_upgrade_trigger(void);


#endif //__BSP_UPGRADE_H__
