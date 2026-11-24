/**********************************************************************
 *
 *
 *
 *********************************************************************/
/**********************************************************************
 *             Title:   ring_buf.c
 *
 *       Description:
 *
 *********************************************************************/

/**********************************************************************
 * Include header files
 *********************************************************************/
/*                                    */
/*********************************************************************/

#include "ring_buf.h"

/*********************************************************************
 * File level pragmas
 *********************************************************************/

/**********************************************************************
 * Constant and Macro Definitions using #define
 *********************************************************************/

/**********************************************************************
 * Enumerations and Structures and Typedefs
 *********************************************************************/

/**********************************************************************
 * Global and Const Variable Defining Definitions / Initializations
 *********************************************************************/

/**********************************************************************
 * Static Variables and Const Variables With File Level Scope
 *********************************************************************/

/**********************************************************************
 * ROM Const Variables With File Level Scope
 *********************************************************************/

/**********************************************************************
 * Function Prototypes for Private Functions with File Level Scope
 *********************************************************************/

/**********************************************************************
 * Add User defined functions
 *********************************************************************/

/**********************************************************************
 * Function Definitions
 *********************************************************************/

/**********************************************************************
 *    Purpose: Reset / Initialize ring buffer to empty
 * Parameters: Pointer to ring buf control structure
 *    Returns: None
 *********************************************************************/
extern void ring_buf_reset(ring_buf_t *buf, uint16_t size) {
    buf->in = 0;
    buf->out = 0;
    buf->full = 0;
    buf->size = size;
}

/**********************************************************************
 *    Purpose: returns whether buffer is empty
 * Parameters: Pointer to ring buf control structure
 *    Returns: true if buffer is empty
 *********************************************************************/
extern uint8_t ring_buf_is_empty(ring_buf_t *buf) {
    return (!buf->full && (buf->in == buf->out));
}

/**********************************************************************
 *    Purpose: returns wether buffer is full
 * Parameters: Pointer to ring buf control structure
 *    Returns: true if buffer is full
 *********************************************************************/
extern uint8_t ring_buf_is_full(ring_buf_t *buf) { return (buf->full); }

/**********************************************************************
 *    Purpose: Logical adds element to ring buffer
 * Parameters: Pointer to ring buf control structure
 *    Returns: None
 *********************************************************************/
extern void ring_buf_add(ring_buf_t *buf) {
    if (!buf->full) {
        buf->in++;                 // increment in index
        if (buf->in >= buf->size)  // wrapping as necessary
        {
            buf->in = 0;
        }
        buf->full =
            (buf->in ==
             buf->out);  // if in pointer reaches out then buffer is full
    }
}



/**********************************************************************
 *    Purpose: Logical remove element  from ring buffer
 * Parameters: Pointer to ring buf control structure
 *    Returns: None
 *********************************************************************/
extern void ring_buf_remove(ring_buf_t *buf) {
    if (!ring_buf_is_empty(buf)) {
        buf->out++;                 // incrment out index
        if (buf->out >= buf->size)  // wrapping as necessary
        {
            buf->out = 0;
        }
        buf->full = 0;  // can not be full if we just removed one
    }
}


/**********************************************************************
 *
 * Modification Record
 *
 *********************************************************************
 *
 * Intial Version
 *
 *********************************************************************/
