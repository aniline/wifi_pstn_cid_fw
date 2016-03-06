#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "user_interface.h"

#include "avr_comm.h"

#define AVR_PROC_PRIO        1
#define AVR_PROC_Q_LEN       1
#define UART0   0
#define UART1   1
#define MSG_BUF_SIZE 64

enum {
     START = 0,
     START_2,
     MSG,
     STOP,
} msg_state = START;

extern UartDevice UartDev;

ETSTimer frame_watchdog_timer;
uint32_t timer_10s = 0;
uint32_t last_message_time_10s = 0;

os_event_t    avr_procQueue[AVR_PROC_Q_LEN];
char 	      msg_buf[MSG_BUF_SIZE] = "";
char 	      msg_idx = 0;


avr_msg_callback msg_cb = NULL;

void avr_msg_callback_default (char *msg, int len) {
     os_printf("Msg = %s (%d)\r\n", msg, len);
}

ICACHE_FLASH_ATTR int uart0_rx_one_char() {
     int ret;
     if (UartDev.rcv_buff.pReadPos == UartDev.rcv_buff.pWritePos)
	  return -1;

     ret = *UartDev.rcv_buff.pReadPos;
     UartDev.rcv_buff.pReadPos++;

     if(UartDev.rcv_buff.pReadPos == (UartDev.rcv_buff.pRcvMsgBuff + RX_BUFF_SIZE)) {
	  UartDev.rcv_buff.pReadPos = UartDev.rcv_buff.pRcvMsgBuff;
     }

     return ret;
}

#define APPEND_CHR(x) { msg_buf[msg_idx] = (x); if (msg_idx < (MSG_BUF_SIZE-1)) { msg_idx ++; } }

static void ICACHE_FLASH_ATTR add_to_msg(char c) {
     if ((msg_state == MSG) || (msg_state == STOP)) {
	  if (msg_state == STOP) APPEND_CHR('-')
	  msg_state = MSG;
	  APPEND_CHR(c)
     } else {
	  msg_state = START;
     }
}

static void ICACHE_FLASH_ATTR avr_msg_handler(os_event_t *events) {
     int c;
     do {
	  last_message_time_10s = timer_10s;
	  c = uart0_rx_one_char();
#ifdef DEBUG
	  os_printf("Ping (%c) State = %d, %d\r\n", c != -1 ? c : '.',
		    msg_state, msg_idx);
#endif
	  /* Message : "++message--" */
	  switch (c) {
	  case '+':
	       if (msg_state == START)
		    msg_state = START_2;
	       else if (msg_state == START_2) {
		    msg_state = MSG;
		    msg_idx = 0;
		    os_memset(msg_buf, 0, MSG_BUF_SIZE);
	       } else {
		    add_to_msg(c);
	       }
	       break;
	  case '-':
	       if (msg_state == MSG)
		    msg_state = STOP;
	       else if (msg_state == STOP) {
		    msg_state = START;
		    msg_cb(msg_buf, msg_idx);
	       } else {
		    msg_state = MSG;
		    add_to_msg(c);
	       }
	       break;
	  case -1:
	       break;
	  default:
	       add_to_msg(c);
	  }
     } while (c != -1);
}

LOCAL void
uart0_rx_intr_override(void *para)
{
     /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
      * uart1 and uart0 respectively
      */
     RcvMsgBuff *pRxBuff = (RcvMsgBuff *)para;
     uint8 RcvChar;

     if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
	  return;
     }

     WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);

     while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
	  RcvChar = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

	  /* you can add your handle code below.*/

	  *(pRxBuff->pWritePos) = RcvChar;

	  // insert here for get one command line from uart
	  if (RcvChar == '\r') {
	       pRxBuff->BuffState = WRITE_OVER;
	  }

	  pRxBuff->pWritePos++;

	  if (pRxBuff->pWritePos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE)) {
	       // overflow ...we may need more error handle here.
	       pRxBuff->pWritePos = pRxBuff->pRcvMsgBuff ;
	  }
     }
     system_os_post(AVR_PROC_PRIO, 0, 0 );
}

void avr_uart_override_rx_intr () {
     ETS_UART_INTR_ATTACH(uart0_rx_intr_override,  &(UartDev.rcv_buff));
}

void avr_reg_msg_callback(avr_msg_callback cb) {
     if (cb)
	  msg_cb = cb;
     else
	  msg_cb = avr_msg_callback_default;
}

void frame_watchdog_timer_fn (void *arg) {
     if (timer_10s > last_message_time_10s) {
	  if ((timer_10s - last_message_time_10s) > 1) {
	       msg_state = START;
	       os_printf("Reset state to START (%d/%d)\r\n", timer_10s, last_message_time_10s);
	  }
     }
     timer_10s ++;
}

void avr_init_task () {
     os_timer_setfn(&frame_watchdog_timer, frame_watchdog_timer_fn, NULL);
     os_timer_arm(&frame_watchdog_timer, 10000, 1);

     msg_cb = avr_msg_callback_default;
     system_os_task(avr_msg_handler, AVR_PROC_PRIO, avr_procQueue, AVR_PROC_Q_LEN);
}
