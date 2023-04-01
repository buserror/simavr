#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests.h"
#include "avr_ioport.h"

#define M_TCNT0 0x52
#define S_TCNT0 0x52

static char m_buf[64], *mp = m_buf, s_buf[64], *sp = s_buf;

static const char expect_s[] = "Hello there, 84";
static const char expect_m[] = "Hello to u 2313";

/* Callbacks for received data. */

static void m_out(struct avr_t * avr, avr_io_addr_t addr,
				  uint8_t v, void * param)
{
	*mp++ = v;
}


static void s_out(struct avr_t * avr, avr_io_addr_t addr,
				  uint8_t v, void * param)
{
	*sp++ = v;
}

int main(int argc, char **argv) {
	avr_t *avr_m, *avr_s;

	tests_init(argc, argv);
	avr_m = tests_init_avr("attiny2313a_usi.axf");
	avr_s = tests_init_avr("attiny84_usi.axf");

	/* Connect the two microcontrollers. */

	avr_connect_irq(avr_io_getirq(avr_m, AVR_IOCTL_IOPORT_GETIRQ('B'), 6),
					avr_io_getirq(avr_s, AVR_IOCTL_IOPORT_GETIRQ('A'), 6));
	avr_connect_irq(avr_io_getirq(avr_m, AVR_IOCTL_IOPORT_GETIRQ('B'), 7),
					avr_io_getirq(avr_s, AVR_IOCTL_IOPORT_GETIRQ('A'), 4));
	avr_connect_irq(avr_io_getirq(avr_s, AVR_IOCTL_IOPORT_GETIRQ('A'), 5),
					avr_io_getirq(avr_m, AVR_IOCTL_IOPORT_GETIRQ('B'), 5));


	/* Request callbacks for data received. */

	avr_register_io_write(avr_m, M_TCNT0, m_out, avr_m);
	avr_register_io_write(avr_s, S_TCNT0, s_out, avr_s);

	/* Run the simulation. */

	for (;;) {
		int state;

		state = avr_run(avr_s);
		if (state == cpu_Done) {
			break;
		} else if (state == cpu_Crashed) {
			fail("ATtiny84 failed: %d\n", state);
			break;
		}

		state = avr_run(avr_m);
		if (state == cpu_Done) {
			break;
		} else if (state == cpu_Crashed) {
			fail("ATtiny2313a failed: %d\n", state);
			break;
		}
	}

	if (strncmp(s_buf, expect_s, strlen(expect_s))) {
		fail("Slave expected \"%s\" but got \"%s\".\n",
			 expect_s, s_buf);
	}
	if (strncmp(m_buf, expect_m, strlen(expect_m))) {
		fail("Master expected \"%s\" but got \"%s\".\n",
			 expect_m, m_buf);
	}
	tests_success();
	return 0;
}
