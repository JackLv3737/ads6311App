/***********************************************************************************************************************
* File Name    : usmart.h
* Version      : 1.0.0
* Description  : head file for usart function
* Creation Date: 2023/05/18
* Author       : wang.yi
***********************************************************************************************************************/

#ifndef __USMART_H
#define __USMART_H

#include "usmart_port.h"


#define USMART_OK               0       /* no err */
#define USMART_FUNCERR          1       /* func err */
#define USMART_PARMERR          2       /* params err */
#define USMART_PARMOVER         3       /* params exceed*/
#define USMART_NOFUNCFIND       4       /* not find compatible func */

#define SP_TYPE_DEC             0       /* display with dec format */
#define SP_TYPE_HEX             1       /* display with hex format */


/* func list */
struct _m_usmart_nametab
{
    void *func;             /* func ptr */
    const char *name;       /* func name */
};

/* usmart control manager */
struct _m_usmart_dev
{
    struct _m_usmart_nametab *funs;     /* func ptr */

    void (*init)(uint16_t tclk);        
    uint8_t (*cmd_rec)(char *str);   /* regonize func name & params */
    void (*exe)(void);                  
    void (*scan)(void);                 
    uint8_t fnum;                       /* func nums*/
    uint8_t pnum;                       /* params nums */
    uint8_t id;                         /* func id */
    uint8_t sptype;                     /* display format, 0->dec; 1->hex */
    uint16_t parmtype;                  /* param type*/
    uint8_t  plentbl[MAX_PARM];         /* buffer to store length of eath param */
    uint8_t  parm[PARM_LEN];            /* params */
    uint8_t runtimeflag;                /* 0,not stat func run time;1 stat func run time. note: this func is only really enable when USMART_ENTIMX_SCAN is enable*/
    uint32_t runtime;                   /* run time,uint:0.1ms */
};

extern struct _m_usmart_dev usmart_dev;            


void usmart_init(uint16_t tclk);        
uint8_t usmart_cmd_rec(char*str);    	
void usmart_exe(void);                 
void usmart_scan(void);               
uint32_t read_addr(uint32_t addr);     
void write_addr(uint32_t addr,uint32_t val);

#endif






























