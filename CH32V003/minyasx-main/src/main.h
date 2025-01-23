#ifndef __MAIN_H
#define __MAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define DEBUG 1

#ifdef DEBUG
#define debugprint(...) printf(__VA_ARGS__)
#else
#define debugprint(...)
#endif

typedef struct context {
    // 0x00: off, 0x01: GREE, 0x02: RED
    // 0x80: BLINK
    int led_access;

    // 0x00: off, 0x01: GREE, 0x02: RED
    // 0x80: BLINK
    int led_eject;

    uint32_t led_blink_counter;

} context_t;

extern bool mode_select_invert;

volatile extern bool in_access;
volatile extern bool media_inserted;
volatile extern bool led_blink;
volatile extern bool eject_mask;

#endif
