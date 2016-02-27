#ifndef _TIMER_H
#define _TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

     void setup_counter();
     long millis();
     uint16_t get_ms_counter();
     uint16_t get_seconds_counter();

#ifdef __cplusplus
}
#endif

#endif
