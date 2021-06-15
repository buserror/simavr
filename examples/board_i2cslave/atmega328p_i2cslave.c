/*
        atmega48_i2ctest.c

        Copyright 2021 Sebastian Koschmieder <sep@seplog.org>

        This file is part of simavr.

        simavr is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        simavr is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/twi.h>

// for linker, emulator, and programmer's sake
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega328p");

#include <stdio.h>

void __attribute__((section(".init8"), naked)) initTWI(void) {
  TWAR = (0x42 << 1);
  TWAMR = 0x7F << 1;
  TWCR = (1 << TWEA) | (1 << TWEN);
}

int twi_getState(void) {
  while ((TWCR & (1 << TWINT)) == 0)
    ;
  return TWSR & 0xF8;
}

static inline void twi_ack(void) { TWCR |= (1 << TWINT) | (1 << TWEA); }
static inline void twi_nack(void) { TWCR |= (1 << TWINT); }

int main() {
  if (twi_getState() != TW_SR_SLA_ACK) {
    abort();
  }

  twi_ack();
  if (twi_getState() != TW_SR_DATA_ACK) {
    abort();
  }

  if (TWDR != 'A') {
    abort();
  }

  twi_ack();
  if (twi_getState() != TW_SR_DATA_ACK) {
    abort();
  }

  if (TWDR != 'B') {
    abort();
  }

  twi_nack();

  cli();
  sleep_mode();
}
