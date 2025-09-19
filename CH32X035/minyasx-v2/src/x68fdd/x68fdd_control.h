#ifndef X68FDD_CONTROL_H
#define X68FDD_CONTROL_H

#include <stdint.h>

#include "ch32fun.h"

void x68fdd_init(void);
void x68fdd_poll(uint32_t systick_ms);

#endif  // X68FDD_CONTROL_H