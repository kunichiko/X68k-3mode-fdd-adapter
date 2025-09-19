#ifndef MINYASX_H
#define MINYASX_H

#include <stdbool.h>
#include <stdint.h>

#include "ch32fun.h"

typedef enum {
    FDD_RPM_300,
    FDD_RPM_360,
    FDD_RPM_NONE,
} fdd_rpm_t;

typedef struct drive_context {
    bool media_inserted;    //
    bool ready;             //
    fdd_rpm_t rpm_setting;  //
    fdd_rpm_t current_rpm;  //
} drive_context_t;

typedef struct minyasx_context {
    drive_context_t drive[2];  // ドライブA/B
} minyasx_context_t;
#endif