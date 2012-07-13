/*
	atmega168_timer_64led.c
	
	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>

#include "atmega168_timer_64led.h"

// for linker, emulator, and programmer's sake
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega168");

PIN_DEFINE(SRESET, D, PD4, 1);
//PIN_DEFINE(SOE, D, PD6, 1);		// pwm AIN0/OC0A
PIN_DEFINE(SLATCH, D, PD7, 1);

PIN_DEFINE(BSTART, C, PC0, 1);	// PCI6
PIN_DEFINE(BSTOP, B, PB1, 1);	// PCI1
PIN_DEFINE(BRESET, B, PB0, 1);	// PCI0


#define	TICK_HZ					4

enum {
	TICK_SECOND			= TICK_HZ,
	TICK_250MS			= (TICK_SECOND / 4),
	TICK_500MS			= (TICK_SECOND / 2),
	TICK_750MS			= (TICK_500MS + TICK_250MS),
	TICK_MAX			= 0x7f,

	TICK_TIMER_DISABLED		= 0xff
};

volatile uint32_t tickCount;
struct tick_t {
	uint8_t	delay;
	void (*callback)(struct tick_t *);
};

enum EDelayIndex {
        delay_Second    = 0,
        delay_Update,
        delay_DisplayChange,
        delay_StopFade,

        timer_MAX
};

struct tick_t timer[timer_MAX];

#define tick_timer_fired(_t) (timer[(_t)].delay == 0)
#define tick_timer_reset(_t, _v) timer[(_t)].delay = (_v)

ISR(TIMER2_COMPA_vect)		// handler for Output Compare 1 overflow interrupt
{
	sei();
	tickCount++;

	// decrement delay lines
	for (char i = 0; i < timer_MAX; i++)
		if (timer[(int)i].delay && timer[(int)i].delay != TICK_TIMER_DISABLED) {
			timer[(int)i].delay--;
			if (timer[(int)i].delay == 0 && timer[(int)i].callback)
				timer[(int)i].callback(&timer[(int)i]);
		}
}


void tick_init()
{
	/*
		Timer 2 as RTC
	 */
	// needs to do that before changing the timer registers
	// ASYNC timer using a 32k crystal
	ASSR |= (1 << AS2);
    TCCR2A = (1 << WGM21);
    TCCR2B = (3 << CS20);
    OCR2A = 127;
    TIMSK2  |= (1 << OCIE2A);
}


enum EKEYS {
	KEY_RESET = 0,	// 0x01
	KEY_STOP,		// 0x02
	KEY_START,		// 0x04
	KEY_MAX
};

enum EState {
	state_ShowTime = 0,
	state_ShowHour,
	state_Sleep,
	
	state_IncrementTime = (1 << 7),
	
	state_MAX
} ;

uint8_t state = state_ShowTime | state_IncrementTime;
uint8_t	digits[4];

enum EDecimal {
	d_second = 0, d_10second, d_minute, d_10minute, d_hour, d_10hour, d_100hour, d_MAX
};
const uint8_t decimal_max[d_MAX] = { 10, 6, 10, 6, 10, 10, 10 };
uint8_t decimal[d_MAX] = {0,0,0,0,0,0,0};
uint8_t decimalChanged = 0;

uint8_t keyState;// = 0x7;
uint8_t keyEvent = 0;
uint8_t keyDebounce[KEY_MAX];
uint8_t lastKeyValue; // = 0; // use to detect which pin(s) triggered the interupt

#define PWM_MAX_DUTY_CYCLE 0xFF

#define STANDBY_DELAY	60

uint8_t pwmRunning = PWM_MAX_DUTY_CYCLE - (PWM_MAX_DUTY_CYCLE >> 4);
uint8_t pwmStopped = PWM_MAX_DUTY_CYCLE - (PWM_MAX_DUTY_CYCLE >> 6);
uint16_t stopTimerCount;

void pwmInit(void)
{
    /* 
       Start Timer 0 with no clock prescaler and phase correct 
       fast PWM mode. Output on PD6 (OC0A). 
    */
    TCCR0A =  (1<<COM0A1)|(0<<COM0A0)|(0<<COM0B1)|(0<<COM0B0)
             |(1<<WGM01) |(1<<WGM00);
    TCCR0B =  (0<<FOC0A) |(0<<FOC0B) |(0<<WGM02)
             |(0<<CS01)  |(1<<CS00);
//	TIMSK0 = (1 << OCIE0A);
	
    // Reset counter
    TCNT0 = 0;

    // Set duty cycle to 1/16th duty
//    OCR0A  = PWM_MAX_DUTY_CYCLE - (PWM_MAX_DUTY_CYCLE >> 4);
	OCR0A = pwmRunning;
}

static inline void pwmSet(uint8_t pwm)
{
	OCR0A = pwm;
}


void decimalInc()
{
	for (uint8_t in = 0; in < d_MAX; in++) {
		decimal[in]++;
		decimalChanged |= (1 << in);
		if (decimal[in] == decimal_max[in]) {
			decimal[in] = 0;
		} else
			break;
	}
}

/*
 			0x01 0x01
 		0x20		0x02
 		0x20		0x02
 			0x40 0x40
 		0x10		0x04
 		0x10		0x04
 			0x08 0x08
  */
const uint8_t	digits2led[10]= {
	0x3f, 0x06, 0x5b, 79, 102, 109, 125, 7, 0x7f, 111
};

struct {
	uint8_t b[16];	
	uint8_t in : 4;
	uint8_t out : 4;	
} spi;

void spi_init()
{
	spi.in = spi.out = 0;
		
	SPCR = 	(1 << SPIE) | (1 << SPE) | (0 << DORD) | (1 << MSTR) | 
			(0 << CPOL) | (0 << CPHA) | (0 << SPR0);
	SPSR =	(0 << SPI2X);
}

void spi_start()
{
	uint8_t b;
	if (spi.in != spi.out) {
		b = spi.b[spi.in++];
		SPDR = b;
	}		
}

/*
 * SPI FIFO and it's interupt function. The function just pushes the
 * 'next' byte into the SPI register, and if there are no more bytes, it
 * toggles the 'latch' PIN of the 74H595 to update all the LEDS in one go.
 *
 * There is a potential small race condition here where 2 sets of four bytes
 * are sent in sequence, but the probability is that 64 bits will be sent 
 * before the latch trigges instead of 32; and if they were /that/ close it
 * doesn'nt make a difference anyway.
 * One way to solve that would be to have a 'terminator' or 'flush' signal
 * queued along the byte stream.
 */

ISR(SPI_STC_vect)
{
	uint8_t b;
	if (spi.in != spi.out) {
		b = spi.b[spi.in++];
		SPDR = b;
	} else {
		// fifo is empty, tell the 74hc595 to latch the shift register
		SET_SLATCH();
		CLR_SLATCH();
	}
}

void startShowTime()
{
	state = (state & ~0xf) | state_ShowTime;
}

void startShowHours(uint8_t timeout /*= 4 * TICK_SECOND*/)
{
	if (timer[delay_DisplayChange].delay != TICK_TIMER_DISABLED)
		tick_timer_reset(delay_DisplayChange, timeout);
	state = (state & ~0xf) | state_ShowHour;
}

void updateTimer();
void sleepTimer();
void wakeTimer();
int startTimer();
int stopTimer();
void resetTimer();

void sleepTimer()
{
	state = state_Sleep;
	tick_timer_reset(delay_Second, 0);
	tick_timer_reset(delay_Update, 0);
	pwmSet(0xff);		// stop the LEDs completely
}

void wakeTimer()
{
	stopTimerCount = 0;
	if (state == state_Sleep) {
		startShowTime();
		tick_timer_reset(delay_Second, TICK_SECOND);
		tick_timer_reset(delay_Update, 1);
		pwmSet(pwmRunning);
		updateTimer();
	} else
		pwmSet(pwmRunning);
}

int startTimer()
{
	if (state & state_IncrementTime)
		return 0;
	wakeTimer();
	tick_timer_reset(delay_Second, TICK_SECOND);
	tick_timer_reset(delay_Update, 1);
	state |= state_IncrementTime;
	return 1;
}

int stopTimer()
{
	wakeTimer();
	if (!(state & state_IncrementTime))
		return 0;
	state &= ~state_IncrementTime;
	stopTimerCount = 0;
	tick_timer_reset(delay_StopFade, 10 * TICK_SECOND);
	return 1;
}

void resetTimer()
{
	wakeTimer();
	startShowTime();
	tick_timer_reset(delay_Second, TICK_SECOND);
	tick_timer_reset(delay_Update, 1);
	for (uint8_t bi = 0; bi < d_MAX; bi++)
		decimal[bi] = 0;
}

void updateTimer()
{
	if (OCR0A == 0xff)	// if the display is off, don't update anything
		return;
	if (!(state & state_IncrementTime) || timer[delay_Second].delay <= 2) {
		switch (state & ~state_IncrementTime) {
			case state_ShowTime:
				digits[1] |= 0x80;
				digits[2] |= 0x80;
				break;
			case state_ShowHour:
				if (state & state_IncrementTime)
					digits[0] |= 0x80;
				break;
		}
	}	     
	cli();
	// interupts are stopped here, so there is no race condition
	int restart = spi.out == spi.in;
	for (uint8_t bi = 0; bi < 4; bi++)
		spi.b[spi.out++] = digits[bi];
	if (restart)
		spi_start();
	sei();
}

void updateTimerDisplay()
{
	do {
		switch (state & ~state_IncrementTime) {
			case state_ShowTime: {
				if (decimalChanged & (1 << d_hour)) {
					startShowHours(4 * TICK_SECOND);
					break;
				}
				decimalChanged = 0;
				for (uint8_t bi = 0; bi < 4; bi++)
					digits[bi] = digits2led[decimal[bi]];

				if (!(state & state_IncrementTime)) {
					digits[1] |= 0x80;
					digits[2] |= 0x80;					
				}
			}	break;
			case state_ShowHour: {
				if (tick_timer_fired(delay_DisplayChange)) {
					decimalChanged = 1;
					startShowTime();
					break;
				}
				decimalChanged = 0;
				for (uint8_t bi = 0; bi < 4; bi++)
					digits[bi] = 0;
				
				if (decimal[d_100hour]) {
					digits[3] = digits2led[decimal[d_100hour]];
					digits[2] = digits2led[decimal[d_10hour]];
					digits[1] = digits2led[decimal[d_hour]];
				} else if (decimal[d_10hour]) {
					digits[3] = digits2led[decimal[d_10hour]];
					digits[2] = digits2led[decimal[d_hour]];					
				} else {
					digits[2] = digits2led[decimal[d_hour]];					
				}
				digits[0] = 116; // 'h'
			}	break;
	//		case state_Sleep: {
				/* nothing to do */
	//		}	break;
		}
	} while (decimalChanged);
}

/*
 * Called every seconds to update the visuals
 */
void second_timer_callback(struct tick_t *t)
{
	t->delay = TICK_SECOND;
	
	if (state & state_IncrementTime) {
		pwmSet(pwmRunning);
		decimalInc();
	} else {
		if (tick_timer_fired(delay_StopFade)) {
			stopTimerCount++;
			if (stopTimerCount >= STANDBY_DELAY) {
				if (OCR0A < 0xff) {
					if ((stopTimerCount & 0xf) == 0) // very gradualy fade out to zero (notch every 8 secs)
						OCR0A++;
				} else
					sleepTimer(); // this will stop the one second timer
			} else {
				if (OCR0A != pwmStopped) {
					if (OCR0A > pwmStopped)
						OCR0A--;
					else
						OCR0A++;
				}
			}
		}			
	}
	updateTimerDisplay();
}

/*
 * Called every tick, to push the four bytes of the counter into the shift registers
 */
void update_timer_callback(struct tick_t *t)
{
	t->delay = 1;
	updateTimer();
}

static void updateKeyValues()
{
	uint8_t keyValue = (PINB & 3) | ((PINC & 1) << 2);
	
	for (uint8_t ki = 0; ki < KEY_MAX; ki++)
		if ((keyValue & (1 << ki)) != (lastKeyValue & (1 << ki)))
			keyDebounce[ki] = 0;
		
	lastKeyValue = keyValue;
}

// pin change interupt
ISR(PCINT0_vect)		{ updateKeyValues(); }
ISR(PCINT1_vect)		{ updateKeyValues(); }

int main(void)
{
	PORTD = 0;
	DDRD = 0xff;

	// set power reduction register, disable everything we don't need
	PRR = (1 << PRTWI) | (1 << PRTIM1) | (1 << PRUSART0) | (1 << PRADC);

	DDRB = ~3; PORTB = 3; // pullups on PB0/PB1
	DDRC = ~1; PORTC = 1; // pullups on PC0
	PCMSK0 = (1 << PCINT1) | (1 << PCINT0);	// enable interupt for these pins
	PCMSK1 = (1 << PCINT8);					// enable interupt for these pins
	PCICR = (1 << PCIE0) | (1 << PCIE1);	// PCIE0 enable pin interupt PCINT7..0.

	tick_init();

	startShowHours(4 * TICK_SECOND);
	
	timer[delay_Second].callback = second_timer_callback;
	timer[delay_Update].callback = update_timer_callback;
	second_timer_callback(&timer[delay_Second]);	// get started
	update_timer_callback(&timer[delay_Update]);	// get started

    startTimer();
    updateKeyValues();
    keyState = lastKeyValue;
    
	SET_SRESET();

	spi_init();
	pwmInit();
		
    sei();
	
    for (;;) {    /* main event loop */
    	/* If our internal ideal of which keys are down is different from the one that has been
			updated via the interrupts, we start counting. If the 'different' key(s) stays the same for
			50ms, we declare it an 'event' and update the internal key state
		 */
    	if (keyState != lastKeyValue) {
    		for (uint8_t ki = 0; ki < KEY_MAX; ki++)
    			if ((keyState & (1 << ki)) != (lastKeyValue & (1 << ki))) {
    				if (keyDebounce[ki] < 50) {
        				keyDebounce[ki]++;
        				if (keyDebounce[ki] == 50) {
        					keyEvent |= (1 << ki);
        					keyState = 	(keyState & ~(1 << ki)) | (lastKeyValue & (1 << ki)); 
        				}
    				}
    			}
    		/*
    		 * if a Key changed state, let's check it out
    		 */
    		if (keyEvent) {
    			if ((keyEvent & (1 << KEY_START)) &&  (keyState & (1 << KEY_START)) == 0) {
    				if (!startTimer())
    					startShowHours(4 * TICK_SECOND);
    			}
    			if ((keyEvent & (1 << KEY_STOP)) && (keyState & (1 << KEY_STOP)) == 0) {
    				if (!stopTimer())
    					startShowHours(4 * TICK_SECOND);
    			}
    			if ((keyEvent & (1 << KEY_RESET)) && (keyState & (1 << KEY_RESET)) == 0) {
    				resetTimer();
    			}
    			keyEvent = 0;
    			updateTimerDisplay();
    			updateTimer();
    		}
    		delay_ms(1);
    	} else {
    		sleep_mode();
    	}
    }
    return 0;
}
