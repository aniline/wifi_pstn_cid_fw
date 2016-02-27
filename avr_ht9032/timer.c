#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/io.h>

volatile uint8_t __ctr = 0;
volatile uint16_t __ms = 0;
volatile uint16_t __s = 0;

#if defined(__AVR_ATtiny85__)
ISR (TIMER0_COMPA_vect) {
#elif defined(__AVR_ATtiny84__)
ISR (TIM0_COMPA_vect) {
#else
ISR (TIMER0_COMPA_vect) {
#endif
     __ms ++;
     if (__ms == 1000) {
	  __ms = 0;
	  __s ++;
	  if (__s == 1000)
	       __s = 0;
     }

     TCNT0 = 0;

#if defined(__AVR_ATtiny85__)
     TIFR |= 0x02;
#elif defined(__AVR_ATtiny84__)
     TIFR0 |= 0x02;
#else
     TIFR0 |= 0x02;
#endif
}

uint16_t get_seconds_counter() {
     uint16_t _s;
     ATOMIC_BLOCK(ATOMIC_FORCEON)
     {
	  _s = __s;
     }
     return _s;
}

uint16_t get_ms_counter() {
     uint16_t _ms;
     ATOMIC_BLOCK(ATOMIC_FORCEON)
     {
	  _ms = __ms;
     }
     return _ms;
}

long millis() {
     long mil;
     uint16_t _ms, _s;
     ATOMIC_BLOCK(ATOMIC_FORCEON)
     {
	  _ms = __ms;
	  _s = __s;
     }
     mil = ((long)_s) * 1000 + ((long) _ms);
     return mil;
}

void setup_counter () {
     cli();

     TCCR0A = 0x00; /* No outputs on compare match (COM0nx), no modes of
		     * outputs (WGMx) */
#if defined(__AVR_ATtiny85__)
     TCCR0B = 0x02; /* FOCXn = 0, WGM2 = 0, Prescaler 2 = io_clk/8, 5
		     * = io_clk / 1024 */
     OCR0A = 100;  /* Compare value */
     TCNT0 = 0x00; /* Counter */
     TIMSK = 0x10; /* OCIE0A = 1, TOIE0 =0  */
     TIFR = 0x10; /* OCF0A = 1, TOV0 = 0 */
#elif defined(__AVR_ATtiny84__)
     TCCR0B = 0x03; /* FOCXn = 0, WGM2 = 0, Prescaler 2 = io_clk/64, 5
		     * = io_clk / 1024 */
     OCR0A = 125;  /* Compare value : 125 * (1/(8Mhz/64)) = 1ms */
     TCNT0 = 0x00; /* Counter */
     TIMSK0 = 0x02; /* OCIE0A = 1, TOIE0 =0  */
     TIFR0 = 0x02;  /* OCF0A = 1, TOV0 = 0 */
#elif defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
     TCCR0B = 0x02; /* FOCXn = 0, WGM2 = 0, Prescaler 2 = io_clk/8, 5
		     * = io_clk / 1024 */
     OCR0A = 100;  /* Compare value */
     TCNT0 = 0x00; /* Counter */
     TIMSK0 = 0x02; /* OCIE0A = 1, TOIE0 =0  */
     TIFR0 = 0x02; /* OCF0A = 1, TOV0 = 0 */
#endif

     sei();
}
