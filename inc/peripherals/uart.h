#ifndef INC_PERIPHERALS_UART_H
#define INC_PERIPHERALS_UART_H

#include <peripherals/base.h>

#define R_UART          (MMIO_BASE + 0x01F02800)
#define UARTn_BASE(n)   (MMIO_BASE + 0x01C28000 + n * 0x400)

#define UART_RBR(n)     (UARTn_BASE(n) + 0x00)
#define UART_THR(n)     (UARTn_BASE(n) + 0x00)
#define UART_DLL(n)     (UARTn_BASE(n) + 0x00)
#define UART_DLH(n)     (UARTn_BASE(n) + 0x04)
#define UART_IER(n)     (UARTn_BASE(n) + 0x04)
#define UART_IIR(n)     (UARTn_BASE(n) + 0x08)
#define UART_FCR(n)     (UARTn_BASE(n) + 0x08)
#define UART_LCR(n)     (UARTn_BASE(n) + 0x0C)
#define UART_MCR(n)     (UARTn_BASE(n) + 0x10)
#define UART_LSR(n)     (UARTn_BASE(n) + 0x14)
#define UART_MSR(n)     (UARTn_BASE(n) + 0x18)
#define UART_SCH(n)     (UARTn_BASE(n) + 0x1C)
#define UART_USR(n)     (UARTn_BASE(n) + 0x7C)
#define UART_TFL(n)     (UARTn_BASE(n) + 0x80)
#define UART_RFL(n)     (UARTn_BASE(n) + 0x84)
#define UART_HALT(n)    (UARTn_BASE(n) + 0xA4)

/* IER (Interrupt Enable Register)のビット定義 */
#define UART_IER_PTIME  (1 << 7)    /* THRE Interrupt Mode */
#define UART_ELSI       (1 << 2)    /* Receiver Line Status Interrup */
#define UART_ETBEI      (1 << 1)    /* Transmit Holding Register Empty Interrupt */
#define UART_ERBFI      (1 << 0)    /* Received Data Available Interrup */

/* IIR (Interrupt Identity Register)のビット定義 */
#define UART_IIR_FEFLAG     (3 << 6)    /* FIFOs Enable Flag */
#define UART_IIR_IID_MASK   (4 << 0)    /* Mask of Interrupt ID */

/* FCR (FIFO Control Register)のビット定義 */
#define UART_FCR_RT         (0 << 6)    /* 1 character in the FIFO ; 1: 1/4, 2: 1/2, 3: full未満 */
#define UART_FCR_TFT        (0 << 4)    /* FIFO empty ; 1: 2 char in, 2: 1/4, 3: 1/2 */
#define UART_FCR_DMAM       (1 << 3)    /* DMA Mode 1 ; 0: Mode 0 */
#define UART_FCR_XFIFOR     (1 << 2)    /* Soft reset transmt FIFO */
#define UART_FCR_RFIFOR     (1 << 1)    /* Sofr reset receive FIFO */
#define UART_FCR_FIFOE      (1 << 0)    /* Enable FIFOs */

#define UART_LCR_DLAB       (1 << 7)    /* Select UART_DLL, DLH ; 0: Select RBR, THR, IER */
#define UART_LCR_BC         (1 << 6)    /* Cause a break condition to be tx to rx device */
#define UART_LCR_EPS        (1 << 4)    /* Select Even Parity ; 0: Odd parity */
#define UART_LCR_PEN        (0 << 3)    /* disable parity ; 1: enable  parity */
#define UART_LCR_STOP       (0 << 2)    /* 1 stop bit ; 1: 1.5 stop bit */
#define UART_LCR_DLS        (3 << 0)    /* 8 bits ; 0: 5 bits, 1: 6 bits, 2: 7 bits */

#define UART_LSR_FIFOERR    (1 << 7)    /* 1: at least one PE, FE, BI in the RX FIFO */
#define UART_LSR_TEMT       (1 << 6)    /* 1: TX FIFO and TX Shift Register are empty ; 0: not empty */
#define UART_LSR_THRE       (1 << 5)    /* 1: TX FIFO is empty ; 0: not empty*/
#define UART_LSR_BI         (1 << 4)    /* indicate the detection of a break sequence on the serial
input data.*/
#define UART_LSR_FE         (1 << 3)    /* 1: framing error ; 0: no error */
#define UART_LSR_PE         (1 << 2)    /* 1: parity error ; no error */
#define UART_LSR_OE         (1 << 1)    /* 1: overrun error ; no overrun */
#define UART_LSR_DR         (1 << 0)    /* 1: data ready ; 0: no data */

#define UART_USR_RFF        (1 << 4)    /* 1: Receive FIFO Full ; 0: not full */
#define UART_USR_RFNE       (1 << 3)    /* 1: Receive FIFO is not empty ; 0: empty */
#define UART_USR_TFE        (1 << 2)    /* 1: Transmit FIFO is empty ; 0: not empty */
#define UART_USR_TFNF       (1 << 1)    /* 1: Transmit FIFO is not full ; 0: full*/
#define UART_USR_BUSY       (1 << 0)    /* 1: UART is busy ; 0: idle or inactive */

#endif
