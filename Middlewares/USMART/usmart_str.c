/***********************************************************************************************************************
* File Name    : usmart_str.c
* Version      : 1.0.0
* Description  : This file is for umsart str
* Creation Date: 2023/05/18
* Author       : wang.yi
***********************************************************************************************************************/

#include "usmart.h"
#include "usmart_str.h"

/**
 * @brief       compare str1&str2
 * @param       str1: str1 ptr
 * @param       str2: str2 ptr
 * @retval      0, equal; 1,not equal;
 */
uint8_t usmart_strcmp(char *str1, char *str2)
{
    while (1)
    {
        if (*str1 != *str2)return 1; 

        if (*str1 == '\0')break;    

        str1++;
        str2++;
    }

    return 0;
}

/**
 * @brief       src copy to dst
 * @param       src: src ptr
 * @param       dst: dst ptr
 * @retval      none
 */
void usmart_strcopy(char *src, char *dst)
{
    while (1)
    {
        *dst = *src;            

        if (*src == '\0')break; 

        src++;
        dst++;
    }
}

/**
 * @brief       get length of str(bytes)
 * @param       str: str ptr
 * @retval      length
 */
uint8_t usmart_strlen(char *str)
{
    uint8_t len = 0;

    while (1)
    {
        if (*str == '\0')break; 

        len++;
        str++;
    }

    return len;
}

/**
 * @brief       square func, m^n
 * @param       m: base
 * @param       n: index
 * @retval      result
 */
uint32_t usmart_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)result *= m;

    return result;
}

/**
 * @brief       translate str to num
 *   @note
 *              support hex convert, but hex num has to be upper format and start with 0X.
 *              support minus
 * @param       str: str ptr
 * @param       res: result store addr
 * @retval      return value:
 *   @arg       0, succuss
 *   @arg       1, num format err
 *   @arg       2, hex format num is 0 digit
 *   @arg       3, start format err
 *   @arg       4, dec format num is 0 digit
 */
uint8_t usmart_str2num(char *str, uint32_t *res)
{
    uint32_t t;
    int tnum;
    uint8_t bnum = 0;   /* digit of the num*/
    char *p;
    uint8_t hexdec = 10;/* default is dec format */
    uint8_t flag = 0;   /* 0, no symble;1,positive num;2,negtive num */
    p = str;
    *res = 0;   /* clear */

    while (1)
    {
        /* params detection*/
        if ((*p <= '9' && *p >= '0') || ((*str == '-' || *str == '+') && bnum == 0) || (*p <= 'F' && *p >= 'A') || (*p == 'X' && bnum == 1))
        {
            if (*p >= 'A')hexdec = 16;  /*when there is a letter in str, is hex format */

            if (*str == '-')
            {
                flag = 2;   /* dismiss symbol */
                str += 1;
            }
            else if (*str == '+')
            {
                flag = 1;   /* dismiss symbol */
                str += 1;
            }
            else
            {
                bnum++; /* digit cnt */
            }
        }
        else if (*p == '\0')
        {
            break;      
        }
        else
        {
            return 1;   /* not all dec or hex format */
        }

        p++;
    }

    p = str;            /* reload str start addr */

    if (hexdec == 16)   /* hex format */
    {
        if (bnum < 3)return 2;  /* digit less than 3*/

        if (*p == '0' && (*(p + 1) == 'X'))   /*must start with 0X */
        {
            p += 2;     /* ptr to the data start addr */
            bnum -= 2;  /* digit minus the start digits */
        }
        else
        {
            return 3;   /* head format err */
        }
    }
    else if (bnum == 0)
    {
        return 4;       /* digis is 0 */
    }

    while (1)
    {
        if (bnum)bnum--;

        if (*p <= '9' && *p >= '0')t = *p - '0';    /* get digit */
        else t = *p - 'A' + 10; /* get value A~F */

        *res += t * usmart_pow(hexdec, bnum);
        p++;

        if (*p == '\0')break;   
    }

    if (flag == 2)      /* negtive num */
    {
        tnum = -*res;
        *res = tnum;
    }

    return 0;  
}

/**
 * @brief       get cmd name
 * @param       str     : source str
 * @param       cmdname : cmd name
 * @param       nlen    : ָcmd length
 * @param       maxlen  : max len(to limit cmd's length is less than maxlen)
 * @retval      0,succuss;others, fail
 */
uint8_t usmart_get_cmdname(char *str, char *cmdname, uint8_t *nlen, uint8_t maxlen)
{
    *nlen = 0;

    while (*str != ' ' && *str != '\0')   /* get ending char */
    {
        *cmdname = *str;
        str++;
        cmdname++;
        (*nlen)++;  /*stat cmd length */

        if (*nlen >= maxlen)return 1;   /* err cmd */
    }

    *cmdname = '\0';/* add ending char*/
    return 0;      
}

/**
 * @brief       get next char(when there are many spaces, spaces would be ommited and first char would be get)
 * @param       str : str ptr
 * @retval      next char
 */
uint8_t usmart_search_nextc(char *str)
{
    str++;

    while (*str == ' ' && str != 0)str++;

    return *str;
}

/**
 * @brief       get func name in str
 * @param       str   : src str
 * @param       fname : get func name ptr
 * @param       pnum  : get params num
 * @param       rval  : need return valueֵ or not(0,no need;1,need)
 * @retval      0,succuss; others, err code
 */
uint8_t usmart_get_fname(char *str, char *fname, uint8_t *pnum, uint8_t *rval)
{
    uint8_t res;
    uint8_t fover = 0;  /* bracket depth */
    char *strtemp;
    uint8_t offset = 0;
    uint8_t parmnum = 0;
    uint8_t temp = 1;
    char fpname[6];  /* void+X+'/0' */
    uint8_t fplcnt = 0; /* fisrt param length counter */
    uint8_t pcnt = 0;   /* param counter */
    uint8_t nchar;
    /* need return val or not */
    strtemp = str;

    while (*strtemp != '\0')    
    {
        if (*strtemp != ' ' && (pcnt & 0X7F) < 5)   /* record max 5 chars */
        {
            if (pcnt == 0)pcnt |= 0X80; /* set MSB, start to receive return val type */

            if (((pcnt & 0x7f) == 4) && (*strtemp != '*'))break;    /* last char has to be '*' */

            fpname[pcnt & 0x7f] = *strtemp; /* record return val type */
            pcnt++;
        }
        else if (pcnt == 0X85)
        {
            break;
        }

        strtemp++;
    }

    if (pcnt)   
    {
        fpname[pcnt & 0x7f] = '\0'; 

        if (usmart_strcmp(fpname, "void") == 0)
        {
            *rval = 0;  /* need no return val*/
        }
        else
        {
            *rval = 1;  /* need return val */
        }

        pcnt = 0;
    }

    res = 0;
    strtemp = str;

    while (*strtemp != '(' && *strtemp != '\0')   /* get the func start position */
    {
        strtemp++;
        res++;

        if (*strtemp == ' ' || *strtemp == '*')
        {
            nchar = usmart_search_nextc(strtemp);   /* get next char */

            if (nchar != '(' && nchar != '*')offset = res;  /* skip space and '*' */
        }
    }

    strtemp = str;

    if (offset)strtemp += offset + 1;   /* skip to func started addr */

    res = 0;
    nchar = 0;  /* flag in str or not.0,not in str;1,in str */

    while (1)
    {
        if (*strtemp == 0)
        {
            res = USMART_FUNCERR;   /* func err */
            break;
        }
        else if (*strtemp == '(' && nchar == 0)
        {
            fover++;    /* bracket depth */
        }
        else if (*strtemp == ')' && nchar == 0)
        {
            if (fover)
            {
                fover--;
            }
            else
            {
                res = USMART_FUNCERR;  /* not receive '(' */
            }

            if (fover == 0)break;       /* end of str */
        }
        else if (*strtemp == '"')
        {
            nchar = !nchar;
        }

        if (fover == 0)   /* func name not receive complete */
        {
            if (*strtemp != ' ')    /* space not included in func name */
            {
                *fname = *strtemp;  /* get func name*/
                fname++;
            }
        }
        else     /* get total func name */
        {
            if (*strtemp == ',')
            {
                temp = 1;           /* enable add 1 param */
                pcnt++;
            }
            else if (*strtemp != ' ' && *strtemp != '(')
            {
                if (pcnt == 0 && fplcnt < 5)    /* to avoid of void param, */
                {
                    fpname[fplcnt] = *strtemp;  /*recode param feature */
                    fplcnt++;
                }

                temp++;     /* get significant param(not space) */
            }

            if (fover == 1 && temp == 2)
            {
                temp++;     /* avoid to duplicate increment */
                parmnum++;  /* param num increment */
            }
        }

        strtemp++;
    }

    if (parmnum == 1)       /* only 1 param */
    {
        fpname[fplcnt] = '\0';  

        if (usmart_strcmp(fpname, "void") == 0)parmnum = 0; /* param is void,stand for no param */
    }

    *pnum = parmnum;/* record param num*/
    *fname = '\0';  
    return res;     
}
/**
 * @brief       get 1 param form str
 * @param       str   : source str ptr
 * @param       fparm : param str ptr
 * @param       ptype : param type
 *   @arg       0   num
 *   @arg       1   str
 *   @arg       0XFF param err
 * @retval
 *   @arg       0,   no more param
 *   @arg       other, next param's offset
 */
uint8_t usmart_get_aparm(char *str, char *fparm, uint8_t *ptype)
{
    uint8_t i = 0;
    uint8_t enout = 0;
    uint8_t type = 0;   /* default is num */
    uint8_t string = 0; /* mark str is reading or not */

    while (1)
    {
        if (*str == ',' && string == 0)enout = 1;   /* delay to return, for finding next param started addr */

        if ((*str == ')' || *str == '\0') && string == 0)break; 

        if (type == 0)
        {
            /* nums detection */
            if ((*str >= '0' && *str <= '9') || *str == '-' || *str == '+' || (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F') || *str == 'X' || *str == 'x')
            {
                if (enout)break;    /* get next param already */

                if (*str >= 'a')
                {
                    *fparm = *str - 0X20;   /* lower convert to upper */
                }
                else
                {
                    *fparm = *str; /* lower char & num need not converting */
                }

                fparm++;
            }
            else if (*str == '"')     /* get start str ptr */
            {
                if (enout)break;    /* get ',' then get '"',means reaching the ending of str */

                type = 1;
                string = 1;         /* string is reading */
            }
            else if (*str != ' ' && *str != ',')     /* illegle char, param err*/
            {
                type = 0XFF;
                break;
            }
        }
        else     /* string type */
        {
            if (*str == '"')string = 0;

            if (enout)break;    /* get next param */

            if (string)         /* string is reading */
            {
                if (*str == '\\')   /* dealing with escape character */
                {
                    str++;      /* ptr to char after escapse char, copy directly */
                    i++;
                }

                *fparm = *str;  /* lower char or num need not converting */
                fparm++;
            }
        }

        i++;    /* offset increment */
        str++;
    }

    *fparm = '\0';  
    *ptype = type;  
    return i;       /* return param length */
}

/**
 * @brief       get assigned param started addr
 * @param       num sequnce of params  range: 0~9.
 * @retval      assigned param's started addr
 */
uint8_t usmart_get_parmpos(uint8_t num)
{
    uint8_t temp = 0;
    uint8_t i;

    for (i = 0; i < num; i++)
    {
        temp += usmart_dev.plentbl[i];
    }

    return temp;
}

/**
 * @brief       get func param form str
 * @param       str  : source str ptr
 * @param       parn : num of param. 0,no param,void type
 * @retval      0,succuss; others, err code
 */
uint8_t usmart_get_fparam(char *str, uint8_t *parn)
{
    uint8_t i, type;
    uint32_t res;
    uint8_t n = 0;
    uint8_t len;
    char tstr[PARM_LEN + 1]; /* buffer for param len */

    for (i = 0; i < MAX_PARM; i++)
    {
        usmart_dev.plentbl[i] = 0;  /* clear param length list */
    }

    while (*str != '(')   /* move to param started addr */
    {
        str++;

        if (*str == '\0')return USMART_FUNCERR; /* ending char*/
    }

    str++;  /* move to first byte after"(" */

    while (1)
    {
        i = usmart_get_aparm(str, tstr, &type); /* get first param */
        str += i;   /* offset */

        switch (type)
        {
            case 0: /* num */
                if (tstr[0] != '\0')    /* received data is not null */
                {
                    i = usmart_str2num(tstr, &res); /* record this param */

                    if (i)return USMART_PARMERR;    /* param err */

                    *(uint32_t *)(usmart_dev.parm + usmart_get_parmpos(n)) = res;   /* record converting result */
                    usmart_dev.parmtype &= ~(1 << n);   /* mark num */
                    usmart_dev.plentbl[n] = 4;  /* param's length is 4 */
                    n++;    /* param increment */

                    if (n > MAX_PARM)return USMART_PARMOVER;    /* param too many */
                }

                break;

            case 1:/* str */
                len = usmart_strlen(tstr) + 1;  /* including ending char '\0' */
                usmart_strcopy(tstr, (char *)&usmart_dev.parm[usmart_get_parmpos(n)]);  /*copy tstr to usmart_dev.parm[n] */
                usmart_dev.parmtype |= 1 << n;  /* mark str */
                usmart_dev.plentbl[n] = len;    /* param's length is len */
                n++;

                if (n > MAX_PARM)return USMART_PARMOVER;    /* param too many */

                break;

            case 0XFF:/* err */
                return USMART_PARMERR;  /* param err */
        }

        if (*str == ')' || *str == '\0')break;  
    }

    *parn = n;  /* record num of params */
    return USMART_OK;   
}














