#ifndef PREFERENCES_CONTROL_H
#define PREFERENCES_CONTROL_H

#include <stdbool.h>

#include "minyasx.h"

void preferences_init(minyasx_context_t* ctx);
void preferences_poll(minyasx_context_t* ctx, uint32_t systick_ms);

void preferences_load_defaults(minyasx_context_t* ctx);
void preferences_save(minyasx_context_t* ctx);

#endif  // PREFERENCES_CONTROL_H