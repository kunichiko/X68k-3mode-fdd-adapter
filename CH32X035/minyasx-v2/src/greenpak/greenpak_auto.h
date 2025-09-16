#ifndef GREENPAK_AUTO_H
#define GREENPAK_AUTO_H

#include <stdint.h>

void greenpak_force_program_verify(uint8_t addr, uint8_t unit);

void greenpak_autoprogram_verify();

#endif  // GREENPAK_AUTO_H