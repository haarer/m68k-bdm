#ifndef BDM_TIMING_H
#define BDM_TIMING_H

#include <stdint.h>

void     bdm_timing_init(void);
void     bdm_delay_half_period(void);
void     bdm_delay_full_period(void);
void     bdm_delay_us(uint16_t us);

#endif
