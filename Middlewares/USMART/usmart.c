/***********************************************************************************************************************
* File Name    : usmart.c
* Version      : 1.0.0
* Description  : This file is for usart function
* Creation Date: 2023/05/18
* Author       : wang.yi
***********************************************************************************************************************/

#include "usmart.h"
#include "usmart_str.h"
#include "usmart_port.h"

/* system cmd */
char *sys_cmd_tab[] =
{
    "?",
    "help",
    "list",
    "id",
    "hex",
    "dec",
    "runtime",
};

/**
 * @brief       sys cmd decode
 * @param       str : ptr to cmd
 * @retval      0, succuss; others, err code.
 */
uint8_t usmart_sys_cmd_exe(char *str)
{
    uint8_t i;
    char sfname[MAX_FNAME_LEN];                  /* store func name */
    uint8_t pnum;
    uint8_t rval;
    uint32_t res;
    res = usmart_get_cmdname(str, sfname, &i, MAX_FNAME_LEN);   /* get cmd & length */

    if (res)return USMART_FUNCERR;                  /* err cmd */

    str += i;

    for (i = 0; i < sizeof(sys_cmd_tab) / 4; i++)   /* lookup table */
    {
        if (usmart_strcmp(sfname, sys_cmd_tab[i]) == 0)break;
    }

    switch (i)
    {
        case 0:
        case 1: /* help */
            USMART_PRINTF("\r\n");
#if USMART_USE_HELP
            USMART_PRINTF("USMART contains 7 system cmds:\r\n");
            USMART_PRINTF("?:      get help info\r\n");
            USMART_PRINTF("help:   get help info\r\n");
            USMART_PRINTF("list:   support func list\r\n\n");
            USMART_PRINTF("id:     support id list\r\n\n");
            USMART_PRINTF("hex:    param display with hex format, follow with space+num to excute converting\r\n\n");
            USMART_PRINTF("dec:    param display with decimal format, follow with space+num to excute converting\r\n\n");
            USMART_PRINTF("runtime:1,enable func running timer;0, disable func running timer\r\n\n");
#else
            USMART_PRINTF("ָerr cmd\r\n");
#endif
            break;

        case 2: /* list user commands */
            USMART_PRINTF("\r\n");
            USMART_PRINTF("-------------------------func list--------------------------- \r\n");

            for (i = 0; i < usmart_dev.fnum; i++)USMART_PRINTF("%s\r\n", usmart_dev.funs[i].name);

            USMART_PRINTF("\r\n");
            break;

        case 3: /* function id */
            USMART_PRINTF("\r\n");
            USMART_PRINTF("-------------------------func ID --------------------------- \r\n");

            for (i = 0; i < usmart_dev.fnum; i++)
            {
                usmart_get_fname((char *)usmart_dev.funs[i].name, sfname, &pnum, &rval); /* get local func name */
                USMART_PRINTF("%s id is:\r\n0X%08X\r\n", sfname, (unsigned int)usmart_dev.funs[i].func);  /* display ID */
            }

            USMART_PRINTF("\r\n");
            break;

        case 4: /* hexָ cmd */
            USMART_PRINTF("\r\n");
            usmart_get_aparm(str, sfname, &i);

            if (i == 0) /* normal params */
            {
                i = usmart_str2num(sfname, &res);       /* record this param */

                if (i == 0) /* number system convert */
                {
                    USMART_PRINTF("HEX:0X%X\r\n", res); /* convert to hex */
                }
                else if (i != 4)return USMART_PARMERR;  /* param err */
                else        /* params display setting */
                {
                    USMART_PRINTF("hex format display\r\n");
                    usmart_dev.sptype = SP_TYPE_HEX;
                }

            }
            else return USMART_PARMERR; /* param err */

            USMART_PRINTF("\r\n");
            break;

        case 5: /* dec cmd */
            USMART_PRINTF("\r\n");
            usmart_get_aparm(str, sfname, &i);

            if (i == 0)     /* param normal */
            {
                i = usmart_str2num(sfname, &res);       /* recode this param */

                if (i == 0) /* number system convert*/
                {
                    USMART_PRINTF("DEC:%lu\r\n", (unsigned long)res);  /* convert to dec */
                }
                else if (i != 4)
                {
                    return USMART_PARMERR;  /* param err. */
                }
                else        /* display setting */
                {
                    USMART_PRINTF("dec format display\r\n");
                    usmart_dev.sptype = SP_TYPE_DEC;
                }

            }
            else 
            {
                return USMART_PARMERR;  /* param err */
            }
                
            USMART_PRINTF("\r\n");
            break;

        case 6: /* runtimeָ cmd, config whether to display func running time */
            USMART_PRINTF("\r\n");
            usmart_get_aparm(str, sfname, &i);

            if (i == 0) /* param normal */
            {
                i = usmart_str2num(sfname, &res);   /* record this param */

                if (i == 0) /* read assign addr */
                {
                    if (USMART_ENTIMX_SCAN == 0)
                    {
                        USMART_PRINTF("\r\nError! \r\nTo EN RunTime function,Please set USMART_ENTIMX_SCAN = 1 first!\r\n");/* err display */
                    }
                    else
                    {
                        usmart_dev.runtimeflag = res;

                        if (usmart_dev.runtimeflag)
                        {
                            USMART_PRINTF("Run Time Calculation ON\r\n");
                        }
                        else 
                        {
                            USMART_PRINTF("Run Time Calculation OFF\r\n");
                        }
                    }
                }
                else 
                {
                    return USMART_PARMERR;  /* no params or params err */
                }
            }
            else 
            {
                return USMART_PARMERR;      /* params err */
            }
            
            USMART_PRINTF("\r\n");
            break;

        default:/* err cmd */
            return USMART_FUNCERR;
    }

    return 0;
}

/**
 * @brief       init USMART
 * @param       tclk: timer freq(unit:Mhz)
 * @retval      none
 */
void usmart_init(uint16_t tclk)
{
#if USMART_ENTIMX_SCAN == 1
    usmart_timx_init(1000, tclk * 100 - 1);
#endif
    usmart_dev.sptype = 1;  /* display params with hex format */
}

/**
 * @brief      get func name,id and params from str
 * @param      str: input str
 * @retval     0,succuss; others, err code.
 */
uint8_t usmart_cmd_rec(char *str)
{
    uint8_t sta, i, rval;   /* ״̬ */
    uint8_t rpnum, spnum;
    char rfname[MAX_FNAME_LEN];  /* buffer to store received func name*/
    char sfname[MAX_FNAME_LEN];  /* buffer to store local func name */
    sta = usmart_get_fname(str, rfname, &rpnum, &rval); /* get received func name and num of params */

    if (sta)return sta; /* err */

    for (i = 0; i < usmart_dev.fnum; i++)
    {
        sta = usmart_get_fname((char *)usmart_dev.funs[i].name, sfname, &spnum, &rval); /*get local func name and num of params */

        if (sta)return sta; /* err */

        if (usmart_strcmp(sfname, rfname) == 0) /* name compatible */
        {
            if (spnum > rpnum)return USMART_PARMERR;/* params err(input params less than func params) */

            usmart_dev.id = i;  /* record func id */
            break;  
        }
    }

    if (i == usmart_dev.fnum)return USMART_NOFUNCFIND;  /* have not found compatible func */

    sta = usmart_get_fparam(str, &i);   /*get func params*/

    if (sta)return sta;     /* return err code*/

    usmart_dev.pnum = i;    /* recode num of params */
    return USMART_OK;
}

/**
 * @brief       USMARTִ excute func
 *   @note
 *              this func is to excute the func receiving from uart.
 *              max supporting 10 params.
 *              this func would print excuting infos. such as "func(param1,param2...paramn)=return value"
 *              when the func return value is void, the print return value would be a meaningless num.
 * @param       none
 * @retval      none
 */
void usmart_exe(void)
{
    uint8_t id, i;
    uint32_t res;
    uint32_t temp[MAX_PARM];        /* params coverting, support str */
    char sfname[MAX_FNAME_LEN];  /* buffer to store local func name */
    uint8_t pnum, rval;
    id = usmart_dev.id;

    if (id >= usmart_dev.fnum)return;   /* no excute */

    usmart_get_fname((char *)usmart_dev.funs[id].name, sfname, &pnum, &rval);    /*get local func name and num of params */
    USMART_PRINTF("\r\n%s(", sfname);   /*print func name to be excuted */

    for (i = 0; i < pnum; i++)      /* output params */
    {
        if (usmart_dev.parmtype & (1 << i)) /* param is str */
        {
            USMART_PRINTF("%c", '"');
            USMART_PRINTF("%s", usmart_dev.parm + usmart_get_parmpos(i));
            USMART_PRINTF("%c", '"');
            temp[i] = (uint32_t) & (usmart_dev.parm[usmart_get_parmpos(i)]);
        }
        else    /* param is num */
        {
            temp[i] = *(uint32_t *)(usmart_dev.parm + usmart_get_parmpos(i));

            if (usmart_dev.sptype == SP_TYPE_DEC)
            {
                USMART_PRINTF("%ld", (long)temp[i]);  /* display with dec format */
            }
            else 
            {
                USMART_PRINTF("0X%X", temp[i]); /* display with hex formatʾ */
            }
        }

        if (i != pnum - 1)USMART_PRINTF(",");
    }

    USMART_PRINTF(")");
#if USMART_ENTIMX_SCAN==1
    usmart_timx_reset_time();   /* timer reset and start to count */
#endif

    switch (usmart_dev.pnum)
    {
        case 0: /* no params(func(void)) */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)();
            break;

        case 1: /*1 param */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0]);
            break;

        case 2: /* 2 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1]);
            break;

        case 3: /* 3 params  */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2]);
            break;

        case 4: /* 4 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3]);
            break;

        case 5: /* 5 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3], temp[4]);
            break;

        case 6: /* 6 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3], temp[4], \
                    temp[5]);
            break;

        case 7: /* 7 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3], temp[4], \
                    temp[5], temp[6]);
            break;

        case 8: /* 8 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3], temp[4], \
                    temp[5], temp[6], temp[7]);
            break;

        case 9: /* 9 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3], temp[4], \
                    temp[5], temp[6], temp[7], temp[8]);
            break;

        case 10:/* 10 params */
            res = (*(uint32_t(*)())usmart_dev.funs[id].func)(temp[0], temp[1], temp[2], temp[3], temp[4], \
                    temp[5], temp[6], temp[7], temp[8], temp[9]);
            break;
    }

#if USMART_ENTIMX_SCAN==1
    usmart_timx_get_time(); /* get func run time */
#endif

    if (rval == 1)  /* return value */
    {
        if (usmart_dev.sptype == SP_TYPE_DEC)USMART_PRINTF("=%lu;\r\n", (unsigned long)res);   /* output return value in dec format */
        else USMART_PRINTF("=0X%X;\r\n", res);  /* output return value in hex format */
    }
    else USMART_PRINTF(";\r\n");    /* no return value */

    if (usmart_dev.runtimeflag)     /* display run time */
    {
        USMART_PRINTF("Function Run Time:%d.%1dms\r\n", usmart_dev.runtime / 10, usmart_dev.runtime % 10);  /* print func run time*/
    }
}

/**
 * @brief       USMART scan 
 * @note        control each USMART by this func. this func should be called by periodic time.
 *              ͨthis func could be called in int, so that it could be managed by itself
 * @param       none
 * @retval      none
 */
void usmart_scan(void)
{
    uint8_t sta, len;
    char *pbuf = 0;

    pbuf = usmart_get_input_string();   /* get data flow */
    if (pbuf == 0) return ; /* data is null */
     
    sta = usmart_dev.cmd_rec(pbuf);     /* get info of each func */

    if (sta == 0)
    {
        usmart_dev.exe();  /* excute func */
    }
    else
    {
        len = usmart_sys_cmd_exe(pbuf);

        if (len != USMART_FUNCERR)sta = len;

        if (sta)
        {
            switch (sta)
            {
                case USMART_FUNCERR:
                    USMART_PRINTF("func err!\r\n");
                    break;

                case USMART_PARMERR:
                    USMART_PRINTF("params err!\r\n");
                    break;

                case USMART_PARMOVER:
                    USMART_PRINTF("params exceed!\r\n");
                    break;

                case USMART_NOFUNCFIND:
                    USMART_PRINTF("have not found compatible func!\r\n");
                    break;
            }
        }
    } 
 
}

#if USMART_USE_WRFUNS == 1  /* enable r/w */

/**
 * @brief       read assigned addr
 * @param       addr
 * @retval      value of the addr
 */ 
uint32_t read_addr(uint32_t addr)
{ 
    return *(uint32_t *)addr;
}

/**
 * @brief       write assigned addr
 * @param       addr,val
 * @retval      none
 */ 
void write_addr(uint32_t addr, uint32_t val)
{
    *(uint32_t *)addr = val;
}

#endif





















