#include <stdint.h>
#include <printf.h>
#include <sys/bus.h>

void initialise_and_start_timer(uintptr_t);

unsigned long timer_get_ms(void);

void ms_delay(int);