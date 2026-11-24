#ifndef  Ring_Buf_H_FILE
#define  Ring_Buf_H_FILE

/**********************************************************************
 *
 *
 *********************************************************************/
/**********************************************************************
 *             Title:   ring_buf.h
 *
 *       Description:   
 *
 *********************************************************************/

/**********************************************************************
 * Include files                                                       
 *********************************************************************/
#include <stdint.h>

/**********************************************************************
 * Global Constant and Macro Definitions using #define                        
 *********************************************************************/

/**********************************************************************
 * Global Enumerations and Structures and Typedefs                          
 *********************************************************************/
typedef struct ring_buf_s
{
   uint16_t    in;            /* index of next array element to add to */
   uint16_t    out;           /* index of oldest element to remove */
   uint16_t    size;          /* number of elements in ring */
   uint8_t  full;          /* Flag to indicate between full and empty buffer */
} ring_buf_t;

/**********************************************************************
 * Global Variable extern Declarations                               
 *********************************************************************/

/**********************************************************************
 * Global Function Prototypes                                               
 *********************************************************************/
 
extern void ring_buf_reset(ring_buf_t *buf, uint16_t size);
/**********************************************************************
 *    Purpose: Reset / Initialize ring buffer to empty
 * Parameters: Pointer to ring buf control structure
 *    Returns: None
 *********************************************************************/

extern uint8_t ring_buf_is_empty(ring_buf_t *buf);
/**********************************************************************
 *    Purpose: returns whether buffer is empty
 * Parameters: Pointer to ring buf control structure
 *    Returns: TRUE if buffer is empty
 *********************************************************************/

extern uint8_t ring_buf_is_full(ring_buf_t *buf);
/**********************************************************************
 *    Purpose: returns wether buffer is full
 * Parameters: Pointer to ring buf control structure
 *    Returns: TRUE if buffer is full
 *********************************************************************/

extern void ring_buf_add(ring_buf_t *buf);
/**********************************************************************
 *    Purpose: Logical adds element to ring buffer 
 * Parameters: Pointer to ring buf control structure
 *    Returns: None
 *********************************************************************/

extern void ring_buf_remove(ring_buf_t *buf);
/**********************************************************************
 *    Purpose: Logical remove element  from ring buffer 
 * Parameters: Pointer to ring buf control structure
 *    Returns: None
 *********************************************************************/

/**********************************************************************
 *
 * Modification Record
 *
 **********************************************************************
 * Initial revision.
 * 
 *********************************************************************/
#endif
