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

// 3mode driveのMODE_SELECT信号の論理反転フラグ
// 9scdrvはOPTION_SELECTとOPTION_SELECT_PAIRをアサートした時に、300RPMで1.44MBモードにするので
// MODE_SELECTもそれを合わせた論理を通常動作とするが、このフラグをアサートすると論理が逆になる
// まとめると、mode_select_invertの意味は以下のようになる。
// falseの時: MODE_SELECT=1(インアクティブ)の時に1.2MB, MODE_SELECT=0(アクティブ)の時に1.44MB
// trueの時 : MODE_SELECT=1(インアクティブ)の時に1.44MB, MODE_SELECT=0(アクティブ)の時に1.2MB
extern bool mode_select_invert;

volatile extern bool in_access;
volatile extern bool media_inserted;
volatile extern bool led_blink;
volatile extern bool eject_mask;

#endif
