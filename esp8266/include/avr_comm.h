#ifndef _AVR_COMM_H
#define _AVR_COMM_H

typedef void (*avr_msg_callback)(char *msg, int len);

void avr_uart_override_rx_intr ();
void avr_reg_msg_callback(avr_msg_callback cb);
void avr_init_task ();

#endif
