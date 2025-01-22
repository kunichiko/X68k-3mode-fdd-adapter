#ifndef __MAIN_H
#define __MAIN_H

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

#endif
