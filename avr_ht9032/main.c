#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/atomic.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef F_CPU
#define F_CPU 8000000
#endif

#include <util/delay.h>
#include "timer.h"

#define byte uint8_t

#define delayMicroseconds _delay_us
#define delay _delay_ms

#if defined (__AVR_ATtiny85__)
#define _PORT     PORTB
#define _DDR      DDRB
#define _PIN      PINB
#define RING_BIT _BV(PB1)
#define DATA_BIT _BV(PB2)
#define DATA_SHIFT_TO_D7 5
#define DEB_BIT  _BV(PB0)
#define UART_BIT _BV(PB4)
#endif

#if defined(__AVR_ATtiny84__)
#define _PORT     PORTA
#define _DDR      DDRA
#define _PIN      PINA
#define RING_BIT _BV(PA1)
#define DATA_BIT _BV(PA2)
#define DATA_SHIFT_TO_D7 5
#define DEB_BIT  _BV(PB3)
#define UART_BIT _BV(PB0)
#endif

#define DEB_OFF (~DEB_BIT)
#define DEB_ON  (DEB_BIT)

static FILE uart_stdout;

void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init () {
     MCUSR = 0;
     wdt_disable();
}

void resetNearSecondsOverflow() {
      uint16_t s = get_seconds_counter();
      if (s >= 990) {
#ifdef DEBUG
	   printf_P(PSTR("Resetting before overflow\n"));
#endif
	   wdt_enable(WDTO_1S);
	   while (1);
      }
}

/* Bit bang uart out for debugging */
void send_uart(byte v) {
     byte i = 0x80;
     _PORT &= ~UART_BIT;
     _delay_us(104); /* 104 us for 9600 baud */
     while (i) {
	  if (v & 1)
	       _PORT |= UART_BIT;
	  else
	       _PORT &= ~UART_BIT;
	  _delay_us(104);
	  i>>=1;
	  v>>=1;
     }
     _PORT |= UART_BIT;
     _delay_us(104);
}

static int uart_putchar(char c, FILE *f) {
     if (c == '\n')
	  uart_putchar('\r', f);
     send_uart(c);
     return 0;
}

void start_uart() {
     fdev_setup_stream(&uart_stdout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
     stdout = &uart_stdout;
}

void dump_hex(const char *msg, unsigned char *buf, int len) {
  int i;
  printf_P(PSTR("%s"), msg);
  for (i = 0; i<len; i++) {
    if ((i % 16) == 0) {
      printf_P(PSTR("\n%03x: "), i);
    }
    printf_P(PSTR(" %02x"), buf[i]);
  }
  printf_P(PSTR("\n"));
}

void setup ()
{
     setup_counter();

     _DDR = (_DDR & 0x00) | DEB_BIT | UART_BIT;
     _PORT &= DEB_OFF;
     _PORT |= UART_BIT;

     start_uart();
}

struct t_cnd_msg {
  byte tag;
  byte len;

  byte month;
  byte day;
  byte hour;
  byte minute;

  char number[16];

  byte cksum;
} cnd_msg;

int parse_cnd_msg(byte *buf, byte len, struct t_cnd_msg *msg) {
  byte *bp = buf;

  if (*bp != 0x4)
    goto Error;

  msg->tag = *bp;
  bp ++;

  msg->len = *bp;

  if (len < (msg->len + 3))
    goto Error;

  bp ++;

  msg->month = ((*bp-'0') * 10);
  bp ++;
  msg->month += (*bp-'0');
  bp ++;

  msg->day = ((*bp-'0') * 10);
  bp ++;
  msg->day += (*bp-'0');
  bp ++;

  msg->hour = ((*bp-'0') * 10);
  bp ++;
  msg->hour += (*bp-'0');
  bp ++;

  msg->minute = ((*bp-'0') * 10);
  bp ++;
  msg->minute += (*bp-'0');
  bp ++;

  memset(msg->number, 0, 16);
  memcpy(msg->number, bp, msg->len-8);
  bp += (msg->len-8);

  msg->cksum = *bp;

  return 1;

Error:
  return 0;
}

struct t_cnd_msg msg;
byte buf[32];

/* This function is called around the time the signal is in the (Mark)
 * stretch of '1's */
void getClip () {
  byte *bp = buf;
  byte len = 0;
  int stc;

  memset(buf, 0, 32);
  /* Wait for the start of data */
  while (_PIN & DATA_BIT);

  do {
    byte v = 0, aux = 0;

    /* Start bit */
    _PORT &= DEB_OFF;
    delayMicroseconds(750);

    for (byte i=8; i>0; i--) {
      _PORT &= DEB_OFF;
      delayMicroseconds(415);

      _PORT |= DEB_ON;

      /* Sample once, happenstance */
      aux = (_PIN & DATA_BIT);
      delayMicroseconds(210);

      /* Sample twice, coincidence */
      aux |= (_PIN & DATA_BIT);
      delayMicroseconds(201);

      /* Sample thrice, enemy action */
      /* aux |= (_PIN & DATA_BIT); */
      v = (v >> 1) | (aux << DATA_SHIFT_TO_D7);
    }

    *bp = v;
    bp ++;
    len ++;

    /* Stop bit */
    _PORT &= DEB_OFF;
    delayMicroseconds(414);
    delayMicroseconds(415);

    stc = 0;
    /* No more bits coming if FSK demod output stays high long enough */
    while ((_PIN & DATA_BIT) && (stc < 1200)) stc++;
    if (stc > 1200) {
	 printf_P(PSTR("stc = %d\r\n"), stc);
    }
  }
  while ((stc < 1200) && (len < 32));
  *bp = 0;

  /* Jump over some additional bits I found few milliseconds later,
   * have to check whats in them */
  delay(200);
  while (!(_PIN & DATA_BIT));

#ifdef DEBUG
  printf("Len = %d\r\n", len);
  dump_hex("Stuff", buf, len);
#endif

  if (parse_cnd_msg(buf, len, &msg)) {
    /* Frame (wrap it in ++ and --) and send the message over to the
     * other guy (ESP8266) */
    printf_P(PSTR("++0,%d,%d,%d,%d,%s--"), msg.month, msg.day, msg.hour, msg.minute, msg.number);

#ifdef DEBUG
    printf_P(PSTR("\nTimestamp = %02d - %02d : %02d:%02d hours\r\n"), msg.month, msg.day, msg.hour, msg.minute);
    printf_P(PSTR("Calling line number = %s\r\n"), msg.number);
    printf_P(PSTR("Tag = %02x, Checksum = %02x, Len = %02x\r\n"), msg.tag, msg.cksum, msg.len);
#endif
  }
  else {
#ifdef DEBUG
    printf_P(PSTR("Parse failed (Len = %d)\r\n"), len);
#endif
    printf_P(PSTR("++1,Parse failed--"));
  }
}

long last_call_first_ring = 0;
long last_call_last_ring = 0;

void waitForFirstRing() {
  byte isFirst = 0;

  printf_P(PSTR("\nWait.. isFirst = %d\n"), isFirst);
  while (!isFirst) {
    uint8_t wakeup_timer = 0;
    _PORT &= DEB_OFF;
     /* Ring start */
    while (!(_PIN & RING_BIT)) {
	 wakeup_timer ++;
	 if (wakeup_timer == 0)
	      resetNearSecondsOverflow();
    };

    _PORT |= DEB_ON;

    /* At some point between 15-20 uS, the _PIN check jumps over and messes with the 'states'
     * Using _delay_us, for porting to non-arduino AVR code. */
    _delay_ms(1);

    while (_PIN & RING_BIT); /* Ring end */
    _PORT &= DEB_OFF;

    _delay_ms(200);

    _PORT |= DEB_ON;
    if (_PIN & RING_BIT) {
      while (_PIN & RING_BIT);
      isFirst = 0;
      _PORT &= DEB_OFF;
      _delay_ms(1);
    }
    else {
      uint16_t s = get_seconds_counter();
      if (((s - last_call_first_ring) > 3) &&
	  ((s - last_call_last_ring) > 3)) {
        _delay_us(20);
        last_call_first_ring = s;
        _PORT &= DEB_OFF;
        isFirst = 1;
#ifdef DEBUG
	printf_P(PSTR("RING (First)\n"));
#endif
      }
      else {
        _delay_us(20);
        _PORT &= DEB_OFF;
#ifdef DEBUG
	printf_P(PSTR("RING\n"));
#endif
      }
      last_call_last_ring = s;
    }
  }
}

void loop ()
{
  waitForFirstRing();
  getClip();
  resetNearSecondsOverflow();
}

int main () {
     setup ();
     /* Rudimentary start up indicator : short flash of the LED */
     _PORT |= DEB_ON;
     _delay_ms(100);
     _PORT &= DEB_OFF;
     while (1) loop();
}
