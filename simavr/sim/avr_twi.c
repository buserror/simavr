/*
	avr_twi.c

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

#include <stdio.h>
#include "avr_twi.h"

/*
 * This block respectfully nicked straight out from the Atmel sample
 * code for AVR315. Typos and all.
 * There is no copyright notice on the original file.
 */
/****************************************************************************
  TWI State codes
****************************************************************************/
// General TWI Master status codes
#define TWI_START                  0x08  // START has been transmitted
#define TWI_REP_START              0x10  // Repeated START has been transmitted
#define TWI_ARB_LOST               0x38  // Arbitration lost

// TWI Master Transmitter status codes
#define TWI_MTX_ADR_ACK            0x18  // SLA+W has been transmitted and ACK received
#define TWI_MTX_ADR_NACK           0x20  // SLA+W has been transmitted and NACK received
#define TWI_MTX_DATA_ACK           0x28  // Data byte has been transmitted and ACK received
#define TWI_MTX_DATA_NACK          0x30  // Data byte has been transmitted and NACK received

// TWI Master Receiver status codes
#define TWI_MRX_ADR_ACK            0x40  // SLA+R has been transmitted and ACK received
#define TWI_MRX_ADR_NACK           0x48  // SLA+R has been transmitted and NACK received
#define TWI_MRX_DATA_ACK           0x50  // Data byte has been received and ACK transmitted
#define TWI_MRX_DATA_NACK          0x58  // Data byte has been received and NACK transmitted

// TWI Slave Transmitter status codes
#define TWI_STX_ADR_ACK            0xA8  // Own SLA+R has been received; ACK has been returned
#define TWI_STX_ADR_ACK_M_ARB_LOST 0xB0  // Arbitration lost in SLA+R/W as Master; own SLA+R has been received; ACK has been returned
#define TWI_STX_DATA_ACK           0xB8  // Data byte in TWDR has been transmitted; ACK has been received
#define TWI_STX_DATA_NACK          0xC0  // Data byte in TWDR has been transmitted; NOT ACK has been received
#define TWI_STX_DATA_ACK_LAST_BYTE 0xC8  // Last data byte in TWDR has been transmitted (TWEA = �0�); ACK has been received

// TWI Slave Receiver status codes
#define TWI_SRX_ADR_ACK            0x60  // Own SLA+W has been received ACK has been returned
#define TWI_SRX_ADR_ACK_M_ARB_LOST 0x68  // Arbitration lost in SLA+R/W as Master; own SLA+W has been received; ACK has been returned
#define TWI_SRX_GEN_ACK            0x70  // General call address has been received; ACK has been returned
#define TWI_SRX_GEN_ACK_M_ARB_LOST 0x78  // Arbitration lost in SLA+R/W as Master; General call address has been received; ACK has been returned
#define TWI_SRX_ADR_DATA_ACK       0x80  // Previously addressed with own SLA+W; data has been received; ACK has been returned
#define TWI_SRX_ADR_DATA_NACK      0x88  // Previously addressed with own SLA+W; data has been received; NOT ACK has been returned
#define TWI_SRX_GEN_DATA_ACK       0x90  // Previously addressed with general call; data has been received; ACK has been returned
#define TWI_SRX_GEN_DATA_NACK      0x98  // Previously addressed with general call; data has been received; NOT ACK has been returned
#define TWI_SRX_STOP_RESTART       0xA0  // A STOP condition or repeated START condition has been received while still addressed as Slave

// TWI Miscellaneous status codes
#define TWI_NO_STATE               0xF8  // No relevant state information available; TWINT = �0�
#define TWI_BUS_ERROR              0x00  // Bus error due to an illegal START or STOP condition

#define AVR_TWI_DEBUG 0

static inline void
_avr_twi_status_set(
		avr_twi_t * p,
		uint8_t v,
		int interrupt)
{
	avr_regbit_setto_raw(p->io.avr, p->twsr, v);
#if AVR_TWI_DEBUG
	printf("_avr_twi_status_set %02x\n", v);
#endif
	avr_raise_irq(p->io.irq + TWI_IRQ_STATUS, v);
	if (interrupt)
		avr_raise_interrupt(p->io.avr, &p->twi);
}

static inline uint8_t
_avr_twi_status_get(
		avr_twi_t * p)
{
	return avr_regbit_get_raw(p->io.avr, p->twsr);
}

static avr_cycle_count_t
avr_twi_set_state_timer(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;
	_avr_twi_status_set(p, p->next_twstate, 1);
	p->next_twstate = 0;
	return 0;
}

/*
 * This is supposed to trigger a timer whose duration is a multiple
 * of 'twi' clock cycles, which should be derived from the prescaler
 * (100khz, 400khz etc).
 * Right now it cheats and uses one twi cycle == one usec.
 */
static void
_avr_twi_delay_state(
		avr_twi_t * p,
		int twi_cycles,
		uint8_t state)
{
	p->next_twstate = state;
	// TODO: calculate clock rate, convert to cycles, and use that
	avr_cycle_timer_register_usec(
			p->io.avr, twi_cycles, avr_twi_set_state_timer, p);
}

static void
avr_twi_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;

	uint8_t twen = avr_regbit_get(avr, p->twen);
	uint8_t twsta = avr_regbit_get(avr, p->twsta);
	uint8_t twsto = avr_regbit_get(avr, p->twsto);
	uint8_t twint = avr_regbit_get(avr, p->twi.raised);

	avr_core_watch_write(avr, addr, v);
#if AVR_TWI_DEBUG
	printf("avr_twi_write %02x START:%d STOP:%d ACK:%d INT:%d TWSR:%02x (state %02x)\n", v,
			avr_regbit_get(avr, p->twsta),
			avr_regbit_get(avr, p->twsto),
			avr_regbit_get(avr, p->twea),
			avr_regbit_get(avr, p->twi.raised),
			avr_regbit_get_raw(p->io.avr, p->twsr), p->state);
#endif
	if (twen != avr_regbit_get(avr, p->twen)) {
		twen = !twen;
		if (!twen) { // if we were running, now now are not
			avr_regbit_clear(avr, p->twea);
			avr_regbit_clear(avr, p->twsta);
			avr_regbit_clear(avr, p->twsto);
			avr_clear_interrupt(avr, &p->twi);
			avr_core_watch_write(avr, p->r_twdr, 0xff);
			_avr_twi_status_set(p, TWI_NO_STATE, 0);
			p->state = 0;
			p->peer_addr = 0;
		}
		printf("TWEN: %d\n", twen);
	}
	if (!twen)
		return;

	uint8_t cleared = avr_regbit_get(avr, p->twi.raised);

	/*int cleared = */avr_clear_interrupt_if(avr, &p->twi, twint);
//	printf("cleared %d\n", cleared);

	if (!twsto && avr_regbit_get(avr, p->twsto)) {
		// generate a stop condition
#if AVR_TWI_DEBUG
		printf("<<<<< I2C stop\n");
#endif
		if (p->state) { // doing stuff
			if (p->state & TWI_COND_START) {
				avr_raise_irq(p->io.irq + TWI_IRQ_MOSI,
						avr_twi_irq_msg(TWI_COND_STOP, p->peer_addr, 1));
			}
		}
		p->state = 0;
	}
	if (!twsta && avr_regbit_get(avr, p->twsta)) {
#if AVR_TWI_DEBUG
		printf(">>>>> I2C %sstart\n", p->state & TWI_COND_START ? "RE" : "");
#endif
		// generate a start condition
		if (p->state & TWI_COND_START)
			_avr_twi_delay_state(p, 3, TWI_REP_START);
		else
			_avr_twi_delay_state(p, 3, TWI_START);
		p->peer_addr = 0;
		p->state = TWI_COND_START;
	}

	if (cleared &&
			!avr_regbit_get(avr, p->twsta) &&
			!avr_regbit_get(avr, p->twsto)) {
		// writing or reading a byte
		if (p->state & TWI_COND_ADDR) {
			int do_read = p->peer_addr & 1;
#if AVR_TWI_DEBUG
			if (do_read)
				printf("I2C READ byte from %02x\n", p->peer_addr);
			else
				printf("I2C WRITE byte %02x to %02x\n", avr->data[p->r_twdr], p->peer_addr);
#endif
			// a normal data byte
			uint8_t msgv = do_read ? TWI_COND_READ : TWI_COND_WRITE;

			if (avr_regbit_get(avr, p->twea))
				msgv |= TWI_COND_ACK;

			p->state &= ~TWI_COND_ACK;	// clear ACK bit

			// if the latch is ready... as set by writing/reading the TWDR
			if ((p->state & msgv)) {

				// we send an IRQ and we /expect/ a slave to reply
				// immediately via an IRQ to set the COND_ACK bit
				// otherwise it's assumed it's been nacked...
				avr_raise_irq(p->io.irq + TWI_IRQ_MOSI,
						avr_twi_irq_msg(msgv, p->peer_addr, avr->data[p->r_twdr]));

				if (do_read) { // read ?
					_avr_twi_delay_state(p, 9,
							msgv & TWI_COND_ACK ?
									TWI_MRX_DATA_ACK : TWI_MRX_DATA_NACK);
				} else {
					_avr_twi_delay_state(p, 9,
							p->state & TWI_COND_ACK ?
									TWI_MTX_DATA_ACK : TWI_MTX_DATA_NACK);
				}
			}
#if AVR_TWI_DEBUG
			else
				printf("I2C latch is not ready, do nothing\n");
#endif
		} else {
#if AVR_TWI_DEBUG
			printf("I2C Master address %02x\n", avr->data[p->r_twdr]);
#endif
			// send the address
			p->state |= TWI_COND_ADDR;
			p->peer_addr = avr->data[p->r_twdr];
			p->state &= ~TWI_COND_ACK;	// clear ACK bit

			// we send an IRQ and we /expect/ a slave to reply
			// immediately via an IRQ tp set the COND_ACK bit
			// otherwise it's assumed it's been nacked...
			avr_raise_irq(p->io.irq + TWI_IRQ_MOSI,
					avr_twi_irq_msg(TWI_COND_START, p->peer_addr, 0));

			if (p->peer_addr & 1) { // read ?
				p->state |= TWI_COND_READ;	// always allow read to start with
				_avr_twi_delay_state(p, 9,
						p->state & TWI_COND_ACK ?
								TWI_MRX_ADR_ACK : TWI_MRX_ADR_NACK);
			} else {
				_avr_twi_delay_state(p, 9,
						p->state & TWI_COND_ACK ?
								TWI_MTX_ADR_ACK : TWI_MTX_ADR_NACK);
			}
		}
		p->state &= ~TWI_COND_WRITE;
	}
}

/*
 * Write data to the latch, tell the system we have something
 * to send next
 */
static void
avr_twi_write_data(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;

	avr_core_watch_write(avr, addr, v);
	// tell system we have something in the write latch
	p->state |= TWI_COND_WRITE;
}

/*
 * Read data from the latch, tell the system can receive a new byte
 */
static uint8_t
avr_twi_read_data(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;

	// tell system we can receive another byte
	p->state |= TWI_COND_READ;
	return avr->data[p->r_twdr];
}

/*
 * prevent code from rewriting out status bits, since we actually use them!
 */
static void
avr_twi_write_status(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;
	uint8_t sr = avr_regbit_get(avr, p->twsr);
	uint8_t c = avr_regbit_get(avr, p->twps);

	avr_core_watch_write(avr, addr, v);
	avr_regbit_setto(avr, p->twsr, sr);	// force restore

	if (c != avr_regbit_get(avr, p->twps)) {
		// prescaler bits changed...
	}
}

static void
avr_twi_irq_input(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;
	avr_t * avr = p->io.avr;

	// check to see if we are enabled
	if (!avr_regbit_get(avr, p->twen))
		return;
	avr_twi_msg_irq_t msg;
	msg.u.v = value;

	// receiving an acknowledge bit
	if (msg.u.twi.msg & TWI_COND_ACK) {
#if AVR_TWI_DEBUG
		printf("I2C received ACK:%d\n", msg.u.twi.data & 1);
#endif
		if (msg.u.twi.data & 1)
			p->state |= TWI_COND_ACK;
		else
			p->state &= ~TWI_COND_ACK;
	}
	// receive a data byte from a slave
	if (msg.u.twi.msg & TWI_COND_READ) {
#if AVR_TWI_DEBUG
		printf("I2C received %02x\n", msg.u.twi.data);
#endif
		avr->data[p->r_twdr] = msg.u.twi.data;
	}
}

void avr_twi_reset(struct avr_io_t *io)
{
	avr_twi_t * p = (avr_twi_t *)io;
	avr_irq_register_notify(p->io.irq + TWI_IRQ_MISO, avr_twi_irq_input, p);
}

static const char * irq_names[TWI_IRQ_COUNT] = {
	[TWI_IRQ_MISO] = "8<mosi",
	[TWI_IRQ_MOSI] = "32>miso",
	[TWI_IRQ_STATUS] = "8>status",
};

static	avr_io_t	_io = {
	.kind = "twi",
	.reset = avr_twi_reset,
	.irq_names = irq_names,
};

void avr_twi_init(avr_t * avr, avr_twi_t * p)
{
	p->io = _io;
	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->twi);

	//printf("%s TWI%c init\n", __FUNCTION__, p->name);

	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_TWI_GETIRQ(p->name), TWI_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->twen.reg, avr_twi_write, p);
	avr_register_io_write(avr, p->r_twdr, avr_twi_write_data, p);
	avr_register_io_read(avr, p->r_twdr, avr_twi_read_data, p);
	avr_register_io_write(avr, p->twsr.reg, avr_twi_write_status, p);
}

