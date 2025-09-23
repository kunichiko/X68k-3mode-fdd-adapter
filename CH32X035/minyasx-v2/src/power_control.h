#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H
#include "ch32fun.h"
#include "ina3221/ina3221_control.h"

void power_control_init(minyasx_context_t* ctx);
void power_control_poll(minyasx_context_t* ctx, uint32_t systick_ms);

#endif  // POWER_CONTROL_H