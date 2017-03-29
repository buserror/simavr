/*
 * hcmd_adc.c
 *
 *  Created on: 15 Oct 2015
 *      Author: michel
 */

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <ctype.h>
#include <wordexp.h>

#include "history_avr.h"
#include "avr_adc.h"

static uint16_t adc_mask = 0;
static uint16_t	adc_val[16];
struct avr_irq_t * adc0_irq = NULL;

static void
adc_update_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	union {
		avr_adc_mux_t mux;
		uint32_t v;
	} m = { .v = value };
//	/printf("read mux %d\r\n", m.mux.src);
	if (!adc0_irq)
		adc0_irq = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC0);
	if (adc_mask & (1 << m.mux.src)) {
		int noise = 0; //(random() % 3) - 1;
		int adc = (int)adc_val[m.mux.src] + noise;
	//	printf("ADC %d %d:%d\n", m.mux.src, adc, noise);
		avr_raise_irq(adc0_irq + m.mux.src, noise < 0 ? 0 : adc);
	}
}

/*
 * called when the AVR change any of the pins we listen to
 */
static void
pin_changed_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
 //   pin_state = (pin_state & ~(1 << irq->irq)) | (value << irq->irq);
	AVR_LOG(avr, LOG_OUTPUT, "PORT B%d changed to %d\n", irq->irq,  value);
}


static int
_cmd_adc(
		wordexp_t * l)
{
	char *cmd = l->we_wordv[0];
	char * it = l->we_wordv[1];
	int index = 0;
	if (!sscanf(it, "%d", &index) || index > 15) {
		fprintf(stderr, "%s: invalid adc number '%s'\r\n", cmd, it);
	} else {
		char * val = l->we_wordv[2];
		if (val) {
			int v = 0;
			if (!sscanf(val, "%d", &v) || v < 0 || v > 5500) {
				fprintf(stderr, "%s: invalid adc value '%s'\r\n", cmd,
						val);
			} else {
				adc_val[index] = v;
				adc_mask |= (1 << index);
			}
		}
		fprintf(stderr, "ADC %d = %d (0x%04x)\r\n", index,
				adc_val[index], adc_val[index]);
	}
	return 0;
}


const history_cmd_t cmd_adc = {
	.names[0] = "adc",
	.usage = "<adc number> [<value in mV>]: Get/set ADC value",
	.help = "Get and sets ADC input IRQ value",
	.parameter_map = (1 << 1) | (1 << 2),
	.execute = _cmd_adc,
};
HISTORY_CMD_REGISTER(cmd_adc);

