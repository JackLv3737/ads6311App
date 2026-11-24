/***********************************************************************************************************************
* Copyright (C) . All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : adaps_nst112_driver.h
* Version      : 1.0.0
* Description  : head file for adaps_nst112_driver.c
* Creation Date: 2023/04/23
* Author       : wang.yi
***********************************************************************************************************************/
#ifndef ADAPS_NST112_H
#define ADAPS_NST112_H

#include <stdint.h>

typedef enum
{
    CHIPA = 0,
    CHIPB,
    CHIPC,
    CHIPD,
}NST112_CHIP;

void nst112_init(void);
float get_temperature_nst112(NST112_CHIP chip);
#endif
