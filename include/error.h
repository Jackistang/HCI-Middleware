#ifndef HM_ERROR_H
#define HM_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#define HM_SUCCESS          0   /* Success */
#define HM_NOT_OPEN         1   /* Device not open */
#define HM_NO_MEMORY        2   /* Memory is not enough */
#define HM_THREAD_ERROR     3   /* Thread error */
#define HM_UART_SEND_ERR    4   /* Uart send error */

#define HM_NOT_SUPPORT      10  /* HCI Middleware not support now */

#ifdef __cplusplus
}
#endif

#endif /* HM_ERROR_H */
