#ifndef PCFDD_CONTROL_H
#define PCFDD_CONTROL_H

#include <stdint.h>

void pcfdd_init(void);
void pcfdd_poll(uint32_t systick_ms);

#endif