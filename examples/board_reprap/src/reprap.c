/*
	simduino.c

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

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <pthread.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"

#include "reprap_gl.h"

#include "button.h"
#include "reprap.h"
#include "arduidiot_pins.h"

#define __AVR_ATmega644__
#include "marlin/pins.h"

#include <stdbool.h>
#define PROGMEM
#include "marlin/Configuration.h"

/*
 * these are the sources of heat and cold to register to the heatpots
 */
enum {
	TALLY_AMBIANT = 1,
	TALLY_HOTEND_PWM,
	TALLY_HOTBED,
	TALLY_HOTEND_FAN,
};

reprap_t reprap;

avr_t * avr = NULL;

// gnu hackery to make sure the parameter is expanded
#define _TERMISTOR_TABLE(num) \
		temptable_##num
#define TERMISTOR_TABLE(num) \
		_TERMISTOR_TABLE(num)

/*
 * called when the AVR change any of the pins on port B
 * so lets update our buffer
 */
static void
hotbed_change_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
//	printf("%s %d\n", __func__, value);
//	pin_state = (pin_state & ~(1 << irq->irq)) | (value << irq->irq);
	heatpot_tally(
			&reprap.hotbed,
			TALLY_HOTEND_PWM,
			value ? 1.0f : 0 );
}
static void
hotend_change_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
//	printf("%s %d\n", __func__, value);
//	pin_state = (pin_state & ~(1 << irq->irq)) | (value << irq->irq);
	heatpot_tally(
			&reprap.hotend,
			TALLY_HOTBED,
			value ? 1.0f : 0 );
}
static void
hotend_fan_change_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	printf("%s %d\n", __func__, value);
//	pin_state = (pin_state & ~(1 << irq->irq)) | (value << irq->irq);
	heatpot_tally(
			&reprap.hotend,
			TALLY_HOTEND_FAN,
			value ? -0.05 : 0 );
}



char avr_flash_path[1024];
int avr_flash_fd = 0;

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init( avr_t * avr)
{
	// open the file
	avr_flash_fd = open(avr_flash_path, O_RDWR|O_CREAT, 0644);
	if (avr_flash_fd < 0) {
		perror(avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(avr_flash_fd, avr->flashend + 1);
	ssize_t r = read(avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit( avr_t* avr)
{
	puts(__func__);
	lseek(avr_flash_fd, SEEK_SET, 0);
	ssize_t r = write(avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(avr_flash_path);
	}
	close(avr_flash_fd);
	uart_pty_stop(&reprap.uart_pty);
}

#define MEGA644_GPIOR0 0x3e

static void
reprap_relief_callback(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
//	printf("%s write %x\n", __func__, addr);
	static uint16_t tick = 0;
	if (!(tick++ & 0xf))
		usleep(100);
}

static void *
avr_run_thread(
		void * ignore)
{
	while (1) {
		avr_run(avr);
	}
	return NULL;
}

void
reprap_init(
		avr_t * avr,
		reprap_p r)
{
	r->avr = avr;
	uart_pty_init(avr, &r->uart_pty);
	uart_pty_connect(&r->uart_pty, '0');

	thermistor_init(avr, &r->therm_hotend, 0,
			(short*)TERMISTOR_TABLE(TEMP_SENSOR_0),
			sizeof(TERMISTOR_TABLE(TEMP_SENSOR_0)) / sizeof(short) / 2,
			OVERSAMPLENR, 25.0f);
	thermistor_init(avr, &r->therm_hotbed, 2,
			(short*)TERMISTOR_TABLE(TEMP_SENSOR_BED),
			sizeof(TERMISTOR_TABLE(TEMP_SENSOR_BED)) / sizeof(short) / 2,
			OVERSAMPLENR, 30.0f);
	thermistor_init(avr, &r->therm_spare, 1,
			(short*)temptable_5, sizeof(temptable_5) / sizeof(short) / 2,
			OVERSAMPLENR, 10.0f);

	heatpot_init(avr, &r->hotend, "hotend", 28.0f);
	heatpot_init(avr, &r->hotbed, "hotbed", 25.0f);

	heatpot_tally(&r->hotend, TALLY_AMBIANT, -0.5f);
	heatpot_tally(&r->hotbed, TALLY_AMBIANT, -0.3f);

	/* connect heatpot temp output to thermistors */
	avr_connect_irq(r->hotend.irq + IRQ_HEATPOT_TEMP_OUT,
			r->therm_hotend.irq + IRQ_TERM_TEMP_VALUE_IN);
	avr_connect_irq(r->hotbed.irq + IRQ_HEATPOT_TEMP_OUT,
			r->therm_hotbed.irq + IRQ_TERM_TEMP_VALUE_IN);

	avr_irq_register_notify(
			get_ardu_irq(avr, HEATER_0_PIN, arduidiot_644),
			hotend_change_hook, NULL);
	avr_irq_register_notify(
			get_ardu_irq(avr, FAN_PIN, arduidiot_644),
			hotend_fan_change_hook, NULL);
	avr_irq_register_notify(
			get_ardu_irq(avr, HEATER_BED_PIN, arduidiot_644),
			hotbed_change_hook, NULL);

	//avr_irq_register_notify()
	float axis_pp_per_mm[4] = DEFAULT_AXIS_STEPS_PER_UNIT;	// from Marlin!
	{
		avr_irq_t * e = get_ardu_irq(avr, X_ENABLE_PIN, arduidiot_644);
		avr_irq_t * s = get_ardu_irq(avr, X_STEP_PIN, arduidiot_644);
		avr_irq_t * d = get_ardu_irq(avr, X_DIR_PIN, arduidiot_644);
		avr_irq_t * m = get_ardu_irq(avr, X_MIN_PIN, arduidiot_644);

		stepper_init(avr, &r->step_x, "X", axis_pp_per_mm[0], 100, 200, 0);
		stepper_connect(&r->step_x, s, d, e, m, stepper_endstop_inverted);
	}
	{
		avr_irq_t * e = get_ardu_irq(avr, Y_ENABLE_PIN, arduidiot_644);
		avr_irq_t * s = get_ardu_irq(avr, Y_STEP_PIN, arduidiot_644);
		avr_irq_t * d = get_ardu_irq(avr, Y_DIR_PIN, arduidiot_644);
		avr_irq_t * m = get_ardu_irq(avr, Y_MIN_PIN, arduidiot_644);

		stepper_init(avr, &r->step_y, "Y", axis_pp_per_mm[1], 100, 200, 0);
		stepper_connect(&r->step_y, s, d, e, m, stepper_endstop_inverted);
	}
	{
		avr_irq_t * e = get_ardu_irq(avr, Z_ENABLE_PIN, arduidiot_644);
		avr_irq_t * s = get_ardu_irq(avr, Z_STEP_PIN, arduidiot_644);
		avr_irq_t * d = get_ardu_irq(avr, Z_DIR_PIN, arduidiot_644);
		avr_irq_t * m = get_ardu_irq(avr, Z_MIN_PIN, arduidiot_644);

		stepper_init(avr, &r->step_z, "Z", axis_pp_per_mm[2], 20, 130, 0);
		stepper_connect(&r->step_z, s, d, e, m, stepper_endstop_inverted);
	}
	{
		avr_irq_t * e = get_ardu_irq(avr, E0_ENABLE_PIN, arduidiot_644);
		avr_irq_t * s = get_ardu_irq(avr, E0_STEP_PIN, arduidiot_644);
		avr_irq_t * d = get_ardu_irq(avr, E0_DIR_PIN, arduidiot_644);

		stepper_init(avr, &r->step_e, "E", axis_pp_per_mm[3], 0, 0, 0);
		stepper_connect(&r->step_e, s, d, e, NULL, 0);
	}

}

int main(int argc, char *argv[])
{
	char path[256];
	strcpy(path, argv[0]);
	strcpy(path, dirname(path));
	strcpy(path, dirname(path));
	printf("Stripped base directory to '%s'\n", path);
	chdir(path);

	int debug = 0;

	for (int i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-d"))
			debug++;
	avr = avr_make_mcu_by_name("atmega644");
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core\n", argv[0]);
		exit(1);
	}
//	snprintf(avr_flash_path, sizeof(avr_flash_path), "%s/%s", pwd, "simduino_flash.bin");
	strcpy(avr_flash_path,  "reprap_flash.bin");
	// register our own functions
	avr->special_init = avr_special_init;
	avr->special_deinit = avr_special_deinit;
	avr_init(avr);
	avr->frequency = 20000000;
	avr->aref = avr->avcc = avr->vcc = 5 * 1000;	// needed for ADC

	elf_firmware_t f;
	const char * fname = "/opt/reprap/tvrrug/Marlin/Marlin/applet/Marlin.elf";
	// try to load an ELF file, before trying the .hex
	if (elf_read_firmware(fname, &f) == 0) {
		printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);
		avr_load_firmware(avr, &f);
	} else {
		char path[1024];
		uint32_t base, size;
//		snprintf(path, sizeof(path), "%s/%s", pwd, "ATmegaBOOT_168_atmega328.ihex");
		strcpy(path, "marlin/Marlin.hex");
//		strcpy(path, "marlin/bootloader-644-20MHz.hex");
		uint8_t * boot = read_ihex_file(path, &size, &base);
		if (!boot) {
			fprintf(stderr, "%s: Unable to load %s\n", argv[0], path);
			exit(1);
		}
		printf("Firmware %04x(%04x in AVR talk): %d bytes (%d words)\n", base, base/2, size, size/2);
		memcpy(avr->flash + base, boot, size);
		free(boot);
		avr->pc = base;
		avr->codeend = avr->flashend;
	}
	//avr->trace = 1;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (debug) {
		printf("AVR is stopped, waiting on gdb on port %d. Use 'target remote :%d' in avr-gdb\n",
				avr->gdb_port, avr->gdb_port);
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	// Marlin doesn't loop, sleep, so we don't know when it's idle
	// I changed Marlin to do a spurious write to the GPIOR0 register so we can trap it
	avr_register_io_write(avr, MEGA644_GPIOR0, reprap_relief_callback, NULL);

	reprap_init(avr, &reprap);

	gl_init(argc, argv);
	pthread_t run;
	pthread_create(&run, NULL, avr_run_thread, NULL);

	gl_runloop();

}
