/*
	ayab.c

	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>

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
#include <stdio.h>
#include <libgen.h>
#include <string.h>

#include "sim_avr.h"
#include "sim_time.h"
#include "sim_core.h"
#include "avr_ioport.h"
#include "avr_adc.h"
#include "avr_twi.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"
#include "uart_pty.h"
#include "queue.h"

#include "ayab_display.h"

// Hardware interfaces
#include "button.h"
#include "machine.h"
#include "shield.h"

avr_t * avr = NULL;
elf_firmware_t firmware = {{0}};
int loglevel = LOG_ERROR;
int gdb = 0;
int gdb_port = 1234;
int trace_pc = 0;
int vcd_enabled = 0;

uart_pty_t uart_pty;

machine_t machine;
shield_t shield;
button_t encoder_v1, encoder_v2;
button_t encoder_beltPhase;
avr_irq_t *adcbase_irq;

extern avr_kind_t *avr_kind[];

static void
list_cores()
{
	printf( "Supported AVR cores:\n");
	for (int i = 0; avr_kind[i]; i++) {
		printf("       ");
		for (int ti = 0; ti < 4 && avr_kind[i]->names[ti]; ti++)
			printf("%s ", avr_kind[i]->names[ti]);
		printf("\n");
	}
	exit(1);
}

static void
display_usage(
	const char * app)
{
	printf("Usage: %s [...] <firmware>\n", app);
	printf(
	 "       [--help|-h|-?]      Display this usage message and exit\n"
	 "       [--list-cores]      List all supported AVR cores and exit\n"
	 "       [-v]                Raise verbosity level (can be passed more than once)\n"
	 "       [--freq|-f <freq>]  Sets the frequency for an .hex firmware\n"
	 "       [--mcu|-m <device>] Sets the MCU type for an .hex firmware\n"
	 "       [--gdb|-g [<port>]] Listen for gdb connection on <port> (default 1234)\n"
	 "       [--output|-o <file>] VCD file to save signal traces\n"
	 "       [--start-vcd|-s     Start VCD output from reset\n"
	 "       [--pc-trace|-p      Add PC to VCD traces\n"
     "       [--machine <machine>]   Select KH910/KH930/KH270 machine (default=KH910)\n"
     "       [--carriage <carriage>] Select K/L/G carriage (default=K)\n"
     "       [--beltphase <phase>]   Select Regular/Shifted (default=Regular)\n"
     "       [--startside <side>]    Select Left/Right side to start (default=Left)\n"
	 "       <firmware>          An ELF file (can include debugging syms)\n"
     "\n");
	exit(1);
}

void
parse_arguments(int argc, char *argv[])
{
	if (argc == 1)
		display_usage(basename(argv[0]));

	for (int pi = 1; pi < argc; pi++) {
		if (!strcmp(argv[pi], "--list-cores")) {
			list_cores();
		} else if (!strcmp(argv[pi], "-v")) {
			loglevel++;
		} else if (!strcmp(argv[pi], "-h") || !strcmp(argv[pi], "--help")) {
			display_usage(basename(argv[0]));
		} else if (!strcmp(argv[pi], "-f") || !strcmp(argv[pi], "--freq")) {
			if (pi < argc-1) {
				firmware.frequency = atoi(argv[++pi]);
			} else {
				display_usage(basename(argv[0]));
			}
		} else if (!strcmp(argv[pi], "-m") || !strcmp(argv[pi], "--mcu")) {
			if (pi < argc-1) {
				snprintf(firmware.mmcu, sizeof(firmware.mmcu), "%s", argv[++pi]);
			} else {
				display_usage(basename(argv[0]));
			}
		} else if (!strcmp(argv[pi], "-g") ||
				   !strcmp(argv[pi], "--gdb")) {
			gdb++;
			if (pi < (argc-2) && argv[pi+1][0] != '-' )
				gdb_port = atoi(argv[++pi]);
		} else if (!strcmp(argv[pi], "-o") ||
				   !strcmp(argv[pi], "--output")) {
			if (pi + 1 >= argc) {
				fprintf(stderr, "%s: missing mandatory argument for %s.\n", argv[0], argv[pi]);
				exit(1);
			}
			snprintf(firmware.tracename, sizeof(firmware.tracename), "%s", argv[++pi]);
		} else if (!strcmp(argv[pi], "-s") ||
				   !strcmp(argv[pi], "--start-vcd")) {
            vcd_enabled = 1;
		} else if (!strcmp(argv[pi], "-p") ||
				   !strcmp(argv[pi], "--pc-trace")) {
            trace_pc = 1;
		} else if (!strcmp(argv[pi], "--machine")) {
			if (pi < argc-1) {
                if (!strcmp(argv[++pi], "KH910")) {
                    machine.type = KH910;
                } else if (!strcmp(argv[pi], "KH930")) {
                    machine.type = KH930;
                } else if (!strcmp(argv[pi], "KH270")) {
                    machine.type = KH270;
                } else {
                    display_usage(basename(argv[0]));
                }
            } else {
				display_usage(basename(argv[0]));
            }
		} else if (!strcmp(argv[pi], "--carriage")) {
			if (pi < argc-1) {
                if (!strcmp(argv[++pi], "K")) {
                    machine.carriage.type = KNIT;
                } else if (!strcmp(argv[pi], "L")) {
                    machine.carriage.type = LACE;
                } else if (!strcmp(argv[pi], "G")) {
                    machine.carriage.type = GARTNER;
                } else {
                    display_usage(basename(argv[0]));
                }
            } else {
				display_usage(basename(argv[0]));
            }
		} else if (!strcmp(argv[pi], "--startside")) {
			if (pi < argc-1) {
                if (!strcmp(argv[++pi], "Left")) {
                    machine.start_side = LEFT;
                } else if (!strcmp(argv[pi], "Right")) {
                    machine.start_side = RIGHT;
                } else {
                    display_usage(basename(argv[0]));
                }
            } else {
				display_usage(basename(argv[0]));
            }
		} else if (!strcmp(argv[pi], "--beltphase")) {
			if (pi < argc-1) {
                if (!strcmp(argv[++pi], "Regular")) {
                    machine.belt_phase = REGULAR;
                } else if (!strcmp(argv[pi], "Shifted")) {
                    machine.belt_phase = SHIFTED;
                } else {
                    display_usage(basename(argv[0]));
                }
            } else {
				display_usage(basename(argv[0]));
            }
		} else if (argv[pi][0] != '-') {
            if (elf_read_firmware(argv[pi], &firmware) == -1) {
                fprintf(stderr, "%s: Unable to load firmware from file %s\n",
                        argv[0], argv[pi]);
                exit(1);
            }
            printf ("%s loaded (f=%d mmcu=%s)\n", argv[pi], (int) firmware.frequency, firmware.mmcu);
        }
    }
}

/* Callback for A-D conversion sampling. */
static void adcTriggerCB(struct avr_irq_t *irq, uint32_t value, void *param)
{
    union {
        avr_adc_mux_t request;
        uint32_t      v;
    }   u = { .v = value };

    switch (u.request.src) {
        case PIN_HALL_RIGHT:
            avr_raise_irq(adcbase_irq + 0, machine.hall_right);
            break;
        case PIN_HALL_LEFT:
            avr_raise_irq(adcbase_irq + 1, machine.hall_left);
            break;
        default:
            fprintf(stderr, "Unexpected ADC_IRQ_OUT_TRIGGER request [0x%04x]\n", u.v);
    }
}

void vcd_trace_enable(int enable) {
    if (avr->vcd) {
        enable ? avr_vcd_start(avr->vcd) : avr_vcd_stop(avr->vcd);
        printf("VCD trace %s (%s)\n", avr->vcd->filename, enable ? "enabled":"disabled");
    }
}

static void * avr_run_thread(void * param)
{
	int state = cpu_Running;
    avr_cycle_count_t lastChange = avr->cycle;
    int *run = (int *)param;
    avr_irq_t vcd_irq_pc;
    
#if AVR_STACK_WATCH
    uint16_t SP_min = (avr->data[R_SPH]<<8) +  avr->data[R_SPL];
#endif

    // Initialize VCD for PC-only traces
    if (!avr->vcd && trace_pc) {
        avr->vcd = malloc(sizeof(*avr->vcd));
        avr_vcd_init(avr,
            firmware.tracename[0] ? firmware.tracename: "simavr.vcd",
            avr->vcd,
            100000 /* usec */
        );
    }

    // Add Program Counter (PC) to vcd file
    if (trace_pc) {
        avr_vcd_add_signal(avr->vcd, &vcd_irq_pc, 16, "PC");
    }

    vcd_trace_enable(vcd_enabled);

    // Phase overlaps 16 needles/solenoids
    unsigned encoder_phase=0;
    // {v1, v2} encoding over 4 phases
    unsigned phase_map[4] = {0, 1, 3, 2};

    encoder_phase = (machine.carriage.position +
        (machine.belt_phase == REGULAR) ? 4 : 12) % 16;

    char needles[machine.num_needles];
    memset(needles, ' ', machine.num_needles);

	while (*run && (state != cpu_Done) && (state != cpu_Crashed))
    {
        // Limit event/interrupt rate towards avr
        if (avr_cycles_to_usec(avr, avr->cycle - lastChange)  > 10000) {
            lastChange = avr->cycle;

            enum event_type event;
            int value;
            if (queue_pop(&event_queue, &event, &value))
            {
                unsigned new_phase = encoder_phase;
                switch (event)
                {
                    case CARRIAGE_LEFT:
                        new_phase = (encoder_phase-1)%16;
                        if ((new_phase%4) == 0) {
                            if (machine.carriage.position > -24) {
                                machine.carriage.position--;
                            } else {
                                machine.carriage.position = -24;
                                new_phase = encoder_phase;
                            }
                        }
                        break;
                    case CARRIAGE_RIGHT:
                        new_phase = (encoder_phase+1)%16;
                        if ((new_phase%4) == 0) {
                            if (machine.carriage.position < (machine.num_needles + 24)) {
                                machine.carriage.position++;
                            } else {
                                machine.carriage.position = (machine.num_needles + 24);
                                new_phase = encoder_phase;
                            }
                        }
                        break;
                    case VCD_DUMP:
                        vcd_enabled = ! vcd_enabled;
                        vcd_trace_enable(vcd_enabled);
                        continue;
                        break;
                    default:
                        fprintf(stderr, "Unexpect event from graphic thread\n");
                        break;
                }

                machine.hall_left = 1650;
                machine.hall_right = 1650;
                uint16_t solenoid_states = (shield.mcp23008[1].reg[MCP23008_REG_OLAT] << 8) + shield.mcp23008[0].reg[MCP23008_REG_OLAT]; 
                int solenoid_update = 0;
                int selected_needle;
                int select_offset = 0;
                switch (machine.carriage.type) {
                    case KNIT:
                        // Handle hall sensors
                        if (machine.carriage.position == 0) {
                            machine.hall_left = 2200; //TBC North
                        } else if (machine.carriage.position == (machine.num_needles - 1)) {
                            machine.hall_right = 2200; //TBC North
                            if(machine.type == KH910) { // Shield error
                                machine.hall_right = 0; // Digital low
                            }
                        }
                        // Handle solenoids
                        select_offset = 24;
                        if (event == CARRIAGE_RIGHT) {
                            select_offset = -24;
                        }
                        break;
                    case LACE:
                        if (machine.carriage.position == 0) {
                            machine.hall_left = 100; //TBC South
                        } else if (machine.carriage.position == (machine.num_needles - 1)) {
                            machine.hall_right = 100; //TBC South
                            if(machine.type == KH910) { // Shield error
                                machine.hall_right = 1650; // HighZ
                            }
                        }
                        // Handle solenoids
                        select_offset = 12;
                        if (event == CARRIAGE_RIGHT) {
                            select_offset = -12;
                        }
                        break;
                    case GARTNER:
                        switch (machine.carriage.position) {
                            case -13:
                            case  13:
                                machine.hall_left = 100; //TBC South
                                break;
                            case -11:
                            case  11:
                                machine.hall_left = 2200; //TBC North
                                break;
                            case 186:
                            case 212:
                                machine.hall_right = 100; //TBC South
                                if(machine.type == KH910) { // Shield error
                                    machine.hall_right = 1650; // HighZ
                                }
                                break;
                            case 188:
                            case 210:
                                machine.hall_right = 2200; //TBC North
                                if(machine.type == KH910) { // Shield error
                                    machine.hall_right = 0; // Digital low
                                }
                                break;
                            default:
                                break;
                        }
                        // Handle solenoids
                        select_offset = 0;
                        break;
                    case KNIT270:
                        switch (machine.carriage.position) {
                            case -6:
                                machine.hall_left = 2200; //TBC North
                                break;
                            case  0:
                                machine.hall_left = 100; //TBC South
                                break;
                            case 111:
                                machine.hall_right = 2200; //TBC North
                                break;
                            case 117:
                                machine.hall_right = 100; //TBC South
                                break;
                            default:
                                break;
                        }
                        // Handle solenoids
                        select_offset = 12;
                        if (event == CARRIAGE_RIGHT) {
                            select_offset = -12;
                        }
                        break;
                    default:
                        fprintf(stderr, "Unexpected carriage type (%d)\n", machine.carriage.type);
                        break;
                }

                selected_needle = machine.carriage.position + select_offset;
                if ((selected_needle < machine.num_needles) && (selected_needle >= 0)) {
                    int solenoid_offset =  (machine.type == KH270) ? 3 : 0;
                    needles[selected_needle] = solenoid_states & (1<< (solenoid_offset + selected_needle % machine.num_solenoids)) ? '.' : '|';
                    solenoid_update = 1;
                }

                encoder_phase = new_phase;
                avr_raise_irq(encoder_v2.irq + IRQ_BUTTON_OUT, (phase_map[encoder_phase % 4] & 1) ? 1 : 0);
                avr_raise_irq(encoder_v1.irq + IRQ_BUTTON_OUT, (phase_map[encoder_phase % 4] & 2) ? 1 : 0);
                avr_raise_irq(encoder_beltPhase.irq + IRQ_BUTTON_OUT, (encoder_phase & 8) ? 1 : 0);

                if ((encoder_phase % 4) == 0) {
                    int half_num_needles = machine.num_needles / 2;

                    char info_buffer[machine.num_needles];
                    memset(info_buffer, ' ', machine.num_needles);
                    if ((selected_needle >=0) && (selected_needle < machine.num_needles)) {
                        info_buffer[selected_needle]           = 'x';
                    }
                    if ((machine.carriage.position >=0) && (machine.carriage.position < machine.num_needles)) {
                        info_buffer[machine.carriage.position] = '^';
                    }

                    fprintf(stderr, "Solenoids =[");
                    for (int i=15; i>=0;i--) {
                        fprintf(stderr, "%c", solenoid_states & (1<<i) ? '.' : '|');
                    }
                    fprintf(stderr, "], Carriage = %3d, Sensors = (%4d, %4d)\n", machine.carriage.position, machine.hall_left, machine.hall_right);

                    if (solenoid_update != 0) {
                        fprintf(stderr, "<- %.*s\n", half_num_needles, needles);
                        fprintf(stderr, "   %.*s\n", half_num_needles, info_buffer);

                        fprintf(stderr, "-> %.*s\n", half_num_needles, needles+half_num_needles);
                        fprintf(stderr, "   %.*s\n", half_num_needles, info_buffer+half_num_needles);
                    }
                }
            }
        }

        // Trigger IRQ for a PC trace
        if (trace_pc && avr->vcd->output) {
            avr_raise_irq(&vcd_irq_pc, avr->pc);
        }

#if AVR_STACK_WATCH
        uint16_t SP = (avr->data[R_SPH]<<8) +  avr->data[R_SPL];
        if (SP < SP_min) {
            SP_min = SP;
            fprintf(stderr, "-- New SP minima (SP = 0x%04x) --\n", SP);
            DUMP_STACK();
        }
#endif
        // Run one AVR cycle
		state = avr_run(avr);
    }

	return NULL;
}

/*
 * called when the AVR change pins on port D
 */
void port_d_changed_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    switch (irq->irq) {
        case PIN_LED_A: // LED_A
            shield.led[0] = shield.led[0] == 0 ? 1 : 0;
            break;
        case PIN_LED_B: // LED_B
            shield.led[1] = shield.led[1] == 0 ? 1 : 0;
            break;
    }
}

int main(int argc, char *argv[])
{
    machine.type = KH910;
    machine.num_needles = 200;
    machine.num_solenoids = 16;
    machine.carriage.type = KNIT;
    machine.belt_phase = REGULAR;

	printf (
        "---------------------------------------------------------\n"
        "AYAB shield emulation \n"
        "- Use left/right arrows to move the carriage\n"
        "- 'v' to start/stop VCD traces"
        "- 'q' or ESC to quit\n"
        "----------------------------------------------------------\n"
    );

    parse_arguments(argc, argv);

    if (machine.type == KH270) {
        machine.num_needles = 112;
        machine.num_solenoids = 12;
        machine.carriage.type = KNIT270;
    }

    if (machine.start_side == LEFT) {
        machine.carriage.position = -24;
    } else {
        machine.carriage.position = machine.num_needles + 24;
    }

	avr = avr_make_mcu_by_name(firmware.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], firmware.mmcu);
		exit(1);
	}

	avr_init(avr);
	avr->log = (loglevel > LOG_TRACE ? LOG_TRACE : loglevel);

	avr_load_firmware (avr, &firmware);
	if (firmware.flashbase) {
		printf("Attempt to load a bootloader at %04x\n", firmware.flashbase);
		avr->pc = firmware.flashbase;
	}

	// Enable gdb
    avr->gdb_port = gdb_port;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

    // Connect uart 0 to a virtual pty
	uart_pty_init(avr, &uart_pty);
	uart_pty_connect(&uart_pty, '0');

    // System Hardware Description
    // mcp23008 at 0x20 & 0x21
    for(int i=0; i<2; i++) {
        i2c_mcp23008_init(avr, &shield.mcp23008[i], (0x20 + i)<<1, 0x01);
        i2c_mcp23008_attach(avr, &shield.mcp23008[i], AVR_IOCTL_TWI_GETIRQ(0));
        shield.mcp23008[i].verbose = loglevel > LOG_WARNING ? 1 : 0;
    }
    // LED_A
    shield.led[0] = 0;
    avr_irq_register_notify(
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), PIN_LED_A),
        port_d_changed_hook, 
        NULL);
    // LED_B
    shield.led[0] = 1;
    avr_irq_register_notify(
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), PIN_LED_B),
        port_d_changed_hook, 
        NULL);
    // Encoder v1 & v2
    button_init(avr, &encoder_v1, "Encoder v1");
    avr_connect_irq(
        encoder_v1.irq + IRQ_BUTTON_OUT,
        avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), PIN_V1)
    );
    button_init(avr, &encoder_v2, "Encoder v2");
    avr_connect_irq(
        encoder_v2.irq + IRQ_BUTTON_OUT,
        avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), PIN_V2)
    );
    // Belt Phase
    button_init(avr, &encoder_beltPhase, "Encoder v2");
    avr_connect_irq(
        encoder_beltPhase.irq + IRQ_BUTTON_OUT,
        avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), PIN_BP)
    );
    // ADC (hall sensors)
    adcbase_irq = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, 0);
    avr_irq_register_notify(adcbase_irq + ADC_IRQ_OUT_TRIGGER, adcTriggerCB, NULL);

    // Beeper not implemented (simavr lacks PWM support)

    // Start display 
    printf( "\nsimavr launching\n");

    ayab_display(argc, argv, avr_run_thread, &machine, &shield);

    uart_pty_stop(&uart_pty);
    printf( "\nsimavr done:\n");
}
