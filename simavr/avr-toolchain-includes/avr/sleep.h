/* Copyright (c) 2002, 2004 Theodore A. Roth
   Copyright (c) 2004, 2007, 2008 Eric B. Weddington
   Copyright (c) 2005, 2006, 2007 Joerg Wunsch
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE. */

/* $Id: sleep.h 2238 2011-05-09 16:44:33Z arcanum $ */

#ifndef _AVR_SLEEP_H_
#define _AVR_SLEEP_H_ 1

#include <avr/io.h>
#include <stdint.h>


/** \file */

/** \defgroup avr_sleep <avr/sleep.h>: Power Management and Sleep Modes

    \code #include <avr/sleep.h>\endcode

    Use of the \c SLEEP instruction can allow an application to reduce its
    power comsumption considerably. AVR devices can be put into different
    sleep modes. Refer to the datasheet for the details relating to the device
    you are using.

    There are several macros provided in this header file to actually
    put the device into sleep mode.  The simplest way is to optionally
    set the desired sleep mode using \c set_sleep_mode() (it usually
    defaults to idle mode where the CPU is put on sleep but all
    peripheral clocks are still running), and then call
    \c sleep_mode(). This macro automatically sets the sleep enable bit, goes 
    to sleep, and clears the sleep enable bit.
    
    Example:
    \code
    #include <avr/sleep.h>

    ...
      set_sleep_mode(<mode>);
      sleep_mode();
    \endcode
    
    Note that unless your purpose is to completely lock the CPU (until a 
    hardware reset), interrupts need to be enabled before going to sleep.

    As the \c sleep_mode() macro might cause race conditions in some
    situations, the individual steps of manipulating the sleep enable
    (SE) bit, and actually issuing the \c SLEEP instruction, are provided
    in the macros \c sleep_enable(), \c sleep_disable(), and
    \c sleep_cpu().  This also allows for test-and-sleep scenarios that
    take care of not missing the interrupt that will awake the device
    from sleep.

    Example:
    \code
    #include <avr/interrupt.h>
    #include <avr/sleep.h>

    ...
      set_sleep_mode(<mode>);
      cli();
      if (some_condition)
      {
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
      }
      sei();
    \endcode

    This sequence ensures an atomic test of \c some_condition with
    interrupts being disabled.  If the condition is met, sleep mode
    will be prepared, and the \c SLEEP instruction will be scheduled
    immediately after an \c SEI instruction.  As the intruction right
    after the \c SEI is guaranteed to be executed before an interrupt
    could trigger, it is sure the device will really be put to sleep.

    Some devices have the ability to disable the Brown Out Detector (BOD) before 
    going to sleep. This will also reduce power while sleeping. If the
    specific AVR device has this ability then an additional macro is defined:
    \c sleep_bod_disable(). This macro generates inlined assembly code
    that will correctly implement the timed sequence for disabling the BOD
    before sleeping. However, there is a limited number of cycles after the
    BOD has been disabled that the device can be put into sleep mode, otherwise
    the BOD will not truly be disabled. Recommended practice is to disable
    the BOD (\c sleep_bod_disable()), set the interrupts (\c sei()), and then
    put the device to sleep (\c sleep_cpu()), like so:

    \code
    #include <avr/interrupt.h>
    #include <avr/sleep.h>

    ...
      set_sleep_mode(<mode>);
      cli();
      if (some_condition)
      {
        sleep_enable();
        sleep_bod_disable();
        sei();
        sleep_cpu();
        sleep_disable();
      }
      sei();
    \endcode
*/


/* Define an internal sleep control register and an internal sleep enable bit mask. */
#if defined(SLEEP_CTRL)

    /* XMEGA devices */
    #define _SLEEP_CONTROL_REG  SLEEP_CTRL
    #define _SLEEP_ENABLE_MASK  SLEEP_SEN_bm

#elif defined(SMCR)

    #define _SLEEP_CONTROL_REG  SMCR
    #define _SLEEP_ENABLE_MASK  _BV(SE)

#elif defined(__AVR_AT94K__)

    #define _SLEEP_CONTROL_REG  MCUR
    #define _SLEEP_ENABLE_MASK  _BV(SE)

#else

    #define _SLEEP_CONTROL_REG  MCUCR
    #define _SLEEP_ENABLE_MASK  _BV(SE)

#endif


/* Define set_sleep_mode() and sleep mode values per device. */
#if defined(__AVR_ATmega161__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_PWR_DOWN     1
    #define SLEEP_MODE_PWR_SAVE     2

    #define set_sleep_mode(mode) \
    do { \
        MCUCR = ((MCUCR & ~_BV(SM1)) | ((mode) == SLEEP_MODE_PWR_DOWN || (mode) == SLEEP_MODE_PWR_SAVE ? _BV(SM1) : 0)); \
        EMCUCR = ((EMCUCR & ~_BV(SM0)) | ((mode) == SLEEP_MODE_PWR_SAVE ? _BV(SM0) : 0)); \
    } while(0)


#elif defined(__AVR_ATmega162__) \
|| defined(__AVR_ATmega8515__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_PWR_DOWN     1
    #define SLEEP_MODE_PWR_SAVE     2
    #define SLEEP_MODE_ADC          3
    #define SLEEP_MODE_STANDBY      4
    #define SLEEP_MODE_EXT_STANDBY  5

    #define set_sleep_mode(mode) \
    do { \
        MCUCR = ((MCUCR & ~_BV(SM1)) | ((mode) == SLEEP_MODE_IDLE ? 0 : _BV(SM1))); \
        MCUCSR = ((MCUCSR & ~_BV(SM2)) | ((mode) == SLEEP_MODE_STANDBY  || (mode) == SLEEP_MODE_EXT_STANDBY ? _BV(SM2) : 0)); \
        EMCUCR = ((EMCUCR & ~_BV(SM0)) | ((mode) == SLEEP_MODE_PWR_SAVE || (mode) == SLEEP_MODE_EXT_STANDBY ? _BV(SM0) : 0)); \
    } while(0)

#elif defined(__AVR_AT90S2313__) \
|| defined(__AVR_AT90S2323__) \
|| defined(__AVR_AT90S2333__) \
|| defined(__AVR_AT90S2343__) \
|| defined(__AVR_AT43USB320__) \
|| defined(__AVR_AT43USB355__) \
|| defined(__AVR_AT90S4414__) \
|| defined(__AVR_AT90S4433__) \
|| defined(__AVR_AT90S8515__) \
|| defined(__AVR_ATtiny22__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_PWR_DOWN     _BV(SM)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~_BV(SM)) | (mode)); \
    } while(0)

#elif defined(__AVR_ATtiny167__) \
|| defined(__AVR_ATtiny87__) \
|| defined(__AVR_ATtiny474__) \
|| defined(__AVR_ATtiny828__) \
|| defined(__AVR_ATtiny841__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1))) | (mode)); \
    } while(0)

#elif defined(__AVR_AT90S4434__) \
|| defined(__AVR_ATA5505__) \
|| defined(__AVR_ATA5272__) \
|| defined(__AVR_AT76C711__) \
|| defined(__AVR_AT90S8535__) \
|| defined(__AVR_ATmega103__) \
|| defined(__AVR_ATmega161__) \
|| defined(__AVR_ATmega163__) \
|| defined(__AVR_ATmega16HVB__) \
|| defined(__AVR_ATmega16HVBREVB__) \
|| defined(__AVR_ATmega32HVB__) \
|| defined(__AVR_ATmega32HVBREVB__) \
|| defined(__AVR_ATtiny13__) \
|| defined(__AVR_ATtiny13A__) \
|| defined(__AVR_ATtiny15__) \
|| defined(__AVR_ATtiny24__) \
|| defined(__AVR_ATtiny24A__) \
|| defined(__AVR_ATtiny44__) \
|| defined(__AVR_ATtiny44A__) \
|| defined(__AVR_ATtiny84__) \
|| defined(__AVR_ATtiny84A__) \
|| defined(__AVR_ATtiny25__) \
|| defined(__AVR_ATtiny45__) \
|| defined(__AVR_ATtiny48__) \
|| defined(__AVR_ATtiny85__) \
|| defined(__AVR_ATtiny88__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATmega48HVF__) \
|| defined(__AVR_ATmega26HVG__) \
|| defined(__AVR_ATmega16HVA__) \
|| defined(__AVR_ATmega8HVA__) 

    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
    #define SLEEP_MODE_PWR_OFF      _BV(SM2)


    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATmega406__)

    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
    #define SLEEP_MODE_PWR_OFF      _BV(SM2)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATtiny2313__) \
|| defined(__AVR_ATtiny2313A__) \
|| defined(__AVR_ATtiny4313__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_PWR_DOWN     (_BV(SM0) | _BV(SM1))
    #define SLEEP_MODE_STANDBY      _BV(SM1)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1))) | (mode)); \
    } while(0)

#elif defined(__AVR_AT94K__) \
|| defined(__AVR_ATmega64HVE__) \
|| defined(__AVR_ATmega64HVE2__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATtiny26__) \
|| defined(__AVR_ATtiny261__) \
|| defined(__AVR_ATtiny261A__) \
|| defined(__AVR_ATtiny461__) \
|| defined(__AVR_ATtiny461A__) \
|| defined(__AVR_ATtiny861__) \
|| defined(__AVR_ATtiny861A__) \
|| defined(__AVR_ATtiny43U__) \
|| defined(__AVR_ATtiny1634__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_STANDBY      (_BV(SM0) | _BV(SM1))

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1))) | (mode)); \
    } while(0)

#elif defined(__AVR_AT90PWM216__) \
|| defined(__AVR_AT90PWM316__) \
|| defined(__AVR_AT90PWM161__) \
|| defined(__AVR_AT90PWM81__) \
|| defined(__AVR_AT90PWM1__) \
|| defined(__AVR_AT90PWM2__) \
|| defined(__AVR_AT90PWM2B__) \
|| defined(__AVR_AT90PWM3__) \
|| defined(__AVR_AT90PWM3B__) \
|| defined(__AVR_ATmega32M1__) \
|| defined(__AVR_ATmega16M1__) \
|| defined(__AVR_ATmega64M1__) 

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_AT90USB1286__) \
|| defined(__AVR_AT90USB1287__) \
|| defined(__AVR_AT90USB646__) \
|| defined(__AVR_AT90USB647__) \
|| defined(__AVR_ATmega128__) \
|| defined(__AVR_ATmega128A__) \
|| defined(__AVR_ATmega1280__) \
|| defined(__AVR_ATmega1281__) \
|| defined(__AVR_ATmega1284__) \
|| defined(__AVR_ATmega1284P__) \
|| defined(__AVR_ATmega128RFA1__) \
|| defined(__AVR_ATmega128RFA2__) \
|| defined(__AVR_ATmega128RFR2__) \
|| defined(__AVR_ATmega1284RFR2__) \
|| defined(__AVR_ATmega16__) \
|| defined(__AVR_ATmega16A__) \
|| defined(__AVR_ATmega162__) \
|| defined(__AVR_ATmega164A__) \
|| defined(__AVR_ATmega164P__) \
|| defined(__AVR_ATmega164PA__) \
|| defined(__AVR_ATmega168A__) \
|| defined(__AVR_ATmega168P__) \
|| defined(__AVR_ATmega168PA__) \
|| defined(__AVR_ATmega16HVA2__) \
|| defined(__AVR_ATmega16U4__) \
|| defined(__AVR_ATmega2560__) \
|| defined(__AVR_ATmega2561__) \
|| defined(__AVR_ATmega256RFA2__) \
|| defined(__AVR_ATmega256RFR2__) \
|| defined(__AVR_ATmega2564RFR2__) \
|| defined(__AVR_ATmega32__) \
|| defined(__AVR_ATmega32A__) \
|| defined(__AVR_ATmega323__) \
|| defined(__AVR_ATmega324A__) \
|| defined(__AVR_ATmega324P__) \
|| defined(__AVR_ATmega324PA__) \
|| defined(__AVR_ATmega328__) \
|| defined(__AVR_ATmega328P__) \
|| defined(__AVR_ATmega32C1__) \
|| defined(__AVR_ATmega32U4__) \
|| defined(__AVR_ATmega32U6__) \
|| defined(__AVR_ATmega48A__) \
|| defined(__AVR_ATmega48PA__) \
|| defined(__AVR_ATmega48P__) \
|| defined(__AVR_ATmega64__) \
|| defined(__AVR_ATmega64A__) \
|| defined(__AVR_ATmega640__) \
|| defined(__AVR_ATmega644__) \
|| defined(__AVR_ATmega644A__) \
|| defined(__AVR_ATmega644P__) \
|| defined(__AVR_ATmega644PA__) \
|| defined(__AVR_ATmega64C1__) \
|| defined(__AVR_ATmega64RFA2__) \
|| defined(__AVR_ATmega64RFR2__) \
|| defined(__AVR_ATmega644RFR2__) \
|| defined(__AVR_ATmega8515__) \
|| defined(__AVR_ATmega8535__) \
|| defined(__AVR_ATmega88A__) \
|| defined(__AVR_ATmega88P__) \
|| defined(__AVR_ATmega88PA__) 

    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
    #define SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))
    #define SLEEP_MODE_EXT_STANDBY  (_BV(SM0) | _BV(SM1) | _BV(SM2))


    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATmega8A__) \
|| defined(__AVR_ATmega8__) \
|| defined(__AVR_ATmega6450A__) \
|| defined(__AVR_ATmega6450P__) \
|| defined(__AVR_ATmega645A__) \
|| defined(__AVR_ATmega645P__) \
|| defined(__AVR_ATmega3250A__) \
|| defined(__AVR_ATmega3250PA__) \
|| defined(__AVR_ATmega325A__) \
|| defined(__AVR_ATmega325PA__) \
|| defined(__AVR_ATmega165A__) \
|| defined(__AVR_ATmega165P__) \
|| defined(__AVR_ATmega165PA__) \
|| defined(__AVR_ATmega169A__) \
|| defined(__AVR_ATmega169P__) \
|| defined(__AVR_ATmega169PA__) \
|| defined(__AVR_ATmega329A__) \
|| defined(__AVR_ATmega329PA__) \
|| defined(__AVR_ATmega3290A__) \
|| defined(__AVR_ATmega3290PA__) \
|| defined(__AVR_ATmega649A__) \
|| defined(__AVR_ATmega649P__) \
|| defined(__AVR_ATmega6490A__) \
|| defined(__AVR_ATmega6490P__) \
|| defined(__AVR_ATmega165__) \
|| defined(__AVR_ATmega169__) \
|| defined(__AVR_ATmega48__) \
|| defined(__AVR_ATmega88__) \
|| defined(__AVR_ATmega168__) \
|| defined(__AVR_ATmega325P__) \
|| defined(__AVR_ATmega3250P__) \
|| defined(__AVR_ATmega325__) \
|| defined(__AVR_ATmega3250__) \
|| defined(__AVR_ATmega645__) \
|| defined(__AVR_ATmega6450__) \
|| defined(__AVR_ATmega329__) \
|| defined(__AVR_ATmega329P__) \
|| defined(__AVR_ATmega3290__) \
|| defined(__AVR_ATmega3290P__) \
|| defined(__AVR_ATmega649__) \
|| defined(__AVR_ATmega6490__) \
|| defined(__AVR_AT90CAN128__) \
|| defined(__AVR_AT90CAN32__) \
|| defined(__AVR_AT90CAN64__) 

    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
    #define SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))


    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATMXT112SL__) \
|| defined(__AVR_ATMXT224__) \
|| defined(__AVR_ATMXT224E__) \
|| defined(__AVR_ATMXT336S__) \
|| defined(__AVR_ATMXT540S__) \
|| defined(__AVR_ATMXT540SREVA__) \
|| defined(__AVR_ATxmega16A4__) \
|| defined(__AVR_ATxmega16A4U__) \
|| defined(__AVR_ATxmega16C4__) \
|| defined(__AVR_ATxmega16D4__) \
|| defined(__AVR_ATxmega32A4__) \
|| defined(__AVR_ATxmega32A4U__) \
|| defined(__AVR_ATxmega32C4__) \
|| defined(__AVR_ATxmega32D4__) \
|| defined(__AVR_ATxmega8E5__) \
|| defined(__AVR_ATxmega16E5__) \
|| defined(__AVR_ATxmega32E5__) \
|| defined(__AVR_ATxmega64A1__) \
|| defined(__AVR_ATxmega64A1U__) \
|| defined(__AVR_ATxmega64A3__) \
|| defined(__AVR_ATxmega64A3U__) \
|| defined(__AVR_ATxmega64A4U__) \
|| defined(__AVR_ATxmega64B1__) \
|| defined(__AVR_ATxmega64B3__) \
|| defined(__AVR_ATxmega64C3__) \
|| defined(__AVR_ATxmega64D3__) \
|| defined(__AVR_ATxmega64D4__) \
|| defined(__AVR_ATxmega128A1__) \
|| defined(__AVR_ATxmega128A1U__) \
|| defined(__AVR_ATxmega128A3__) \
|| defined(__AVR_ATxmega128A3U__) \
|| defined(__AVR_ATxmega128A4U__) \
|| defined(__AVR_ATxmega128B1__) \
|| defined(__AVR_ATxmega128B3__) \
|| defined(__AVR_ATxmega128C3__) \
|| defined(__AVR_ATxmega128D3__) \
|| defined(__AVR_ATxmega128D4__) \
|| defined(__AVR_ATxmega192A3__) \
|| defined(__AVR_ATxmega192A3U__) \
|| defined(__AVR_ATxmega192C3__) \
|| defined(__AVR_ATxmega192D3__) \
|| defined(__AVR_ATxmega256A3__) \
|| defined(__AVR_ATxmega256A3U__) \
|| defined(__AVR_ATxmega256C3__) \
|| defined(__AVR_ATxmega256D3__) \
|| defined(__AVR_ATxmega256A3B__) \
|| defined(__AVR_ATxmega256A3BU__) \
|| defined(__AVR_ATxmega384C3__) \
|| defined(__AVR_ATxmega384D3__)

    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_PWR_DOWN     (SLEEP_SMODE1_bm)
    #define SLEEP_MODE_PWR_SAVE     (SLEEP_SMODE1_bm | SLEEP_SMODE0_bm)
    #define SLEEP_MODE_STANDBY      (SLEEP_SMODE2_bm | SLEEP_SMODE1_bm)
    #define SLEEP_MODE_EXT_STANDBY  (SLEEP_SMODE2_bm | SLEEP_SMODE1_bm | SLEEP_SMODE0_bm)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(SLEEP_SMODE2_bm | SLEEP_SMODE1_bm | SLEEP_SMODE0_bm)) | (mode)); \
    } while(0)

#elif defined(__AVR_ATxmega32X1__)
    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_PWR_SAVE     (SLEEP_SMODE0_bm)
    #define SLEEP_MODE_PWR_DOWN     (SLEEP_SMODE1_bm)
    #define SLEEP_MODE_PWR_OFF      (SLEEP_SMODE0_bm | SLEEP_SMODE1_bm)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(SLEEP_SMODE1_bm | SLEEP_SMODE0_bm)) | (mode)); \
    } while(0)

#elif defined(__AVR_ATMXTS200__)
    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_PWR_SAVE     (SLEEP_SMODE0_bm)
    #define SLEEP_MODE_PWR_DOWN     (SLEEP_SMODE1_bm)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(SLEEP_SMODE1_bm | SLEEP_SMODE0_bm)) | (mode)); \
    } while(0)

#elif defined(__AVR_AT90SCR100__) \
|| defined(__AVR_ATmega8U2__) \
|| defined(__AVR_ATmega16U2__) \
|| defined(__AVR_ATmega32U2__) \
|| defined(__AVR_AT90USB162__) \
|| defined(__AVR_AT90USB82__) 

    #define SLEEP_MODE_IDLE         (0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
    #define SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))
    #define SLEEP_MODE_EXT_STANDBY  (_BV(SM0) | _BV(SM1) | _BV(SM2))

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATA6285__) \
|| defined(__AVR_ATA6286__) \
|| defined(__AVR_ATA6289__)

    #define SLEEP_MODE_IDLE                     (0)
    #define SLEEP_MODE_SENSOR_NOISE_REDUCTION   (_BV(SM0))
    #define SLEEP_MODE_PWR_DOWN                 (_BV(SM1))

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined (__AVR_ATA5790__) \
|| defined (__AVR_ATA5790N__) \
|| defined (__AVR_ATA5795__) \
|| defined (__AVR_ATA5831__)

    #define SLEEP_MODE_IDLE           (0)
    #define SLEEP_MODE_EXT_PWR_SAVE   (_BV(SM0))
    #define SLEEP_MODE_PWR_DOWN       (_BV(SM1))
    #define SLEEP_MODE_PWR_SAVE       (_BV(SM1) | _BV(SM0))     
    
    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#elif defined(__AVR_ATtiny4__) \
|| defined(__AVR_ATtiny5__) \
|| defined(__AVR_ATtiny9__) \
|| defined(__AVR_ATtiny10__) \
|| defined(__AVR_ATtiny20__) \
|| defined(__AVR_ATtiny40__)

    #define SLEEP_MODE_IDLE         0
    #define SLEEP_MODE_ADC          _BV(SM0)
    #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
    #define SLEEP_MODE_STANDBY      _BV(SM2)

    #define set_sleep_mode(mode) \
    do { \
        _SLEEP_CONTROL_REG = ((_SLEEP_CONTROL_REG & ~(_BV(SM0) | _BV(SM1) | _BV(SM2))) | (mode)); \
    } while(0)

#else

    #error "No SLEEP mode defined for this device."

#endif



/** \ingroup avr_sleep

    Put the device in sleep mode. How the device is brought out of sleep mode
    depends on the specific mode selected with the set_sleep_mode() function.
    See the data sheet for your device for more details. */


#if defined(__DOXYGEN__)

/** \ingroup avr_sleep

    Set the SE (sleep enable) bit.
*/
extern void sleep_enable (void);

#else

#define sleep_enable()             \
do {                               \
  _SLEEP_CONTROL_REG |= (uint8_t)_SLEEP_ENABLE_MASK;   \
} while(0)

#endif


#if defined(__DOXYGEN__)

/** \ingroup avr_sleep

    Clear the SE (sleep enable) bit.
*/
extern void sleep_disable (void);

#else

#define sleep_disable()            \
do {                               \
  _SLEEP_CONTROL_REG &= (uint8_t)(~_SLEEP_ENABLE_MASK);  \
} while(0)

#endif


/** \ingroup avr_sleep

    Put the device into sleep mode.  The SE bit must be set
    beforehand, and it is recommended to clear it afterwards.
*/
#if defined(__DOXYGEN__)

extern void sleep_cpu (void);

#else

#define sleep_cpu()                              \
do {                                             \
  __asm__ __volatile__ ( "sleep" "\n\t" :: );    \
} while(0)

#endif


#if defined(__DOXYGEN__)

extern void sleep_mode (void);

#else

#define sleep_mode() \
do {                 \
    sleep_enable();  \
    sleep_cpu();     \
    sleep_disable(); \
} while (0)

#endif


#if defined(__DOXYGEN__)

extern void sleep_bod_disable (void);

#else

#if defined(BODS) && defined(BODSE)

#define sleep_bod_disable() \
do { \
  uint8_t tempreg; \
  __asm__ __volatile__("in %[tempreg], %[mcucr]" "\n\t" \
                       "ori %[tempreg], %[bods_bodse]" "\n\t" \
                       "out %[mcucr], %[tempreg]" "\n\t" \
                       "andi %[tempreg], %[not_bodse]" "\n\t" \
                       "out %[mcucr], %[tempreg]" \
                       : [tempreg] "=&d" (tempreg) \
                       : [mcucr] "I" _SFR_IO_ADDR(MCUCR), \
                         [bods_bodse] "i" (_BV(BODS) | _BV(BODSE)), \
                         [not_bodse] "i" (~_BV(BODSE))); \
} while (0)

#endif

#endif


/*@}*/

#endif /* _AVR_SLEEP_H_ */
