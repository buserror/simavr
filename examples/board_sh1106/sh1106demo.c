/*
 ssh1106demo.c

 Copyright Luki <humbell@ethz.ch>
 Copyright 2011 Michel Pollet <buserror@gmail.com>
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

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

// System interfaces
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <pthread.h>
#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

// simavr interfaces
#include "sim_avr.h"
#include "sim_time.h"
#include "avr_ioport.h"
#include "avr_adc.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"

// Hardware interfaces
#include "button.h"
#include "sh1106_glut.h"

// Global objects
// simavr
avr_t * avr = NULL;

elf_firmware_t firmware = {{0}};
int loglevel = LOG_ERROR;
int gdb = 0;
int gdb_port = 1234;
int trace_pc = 0;
int vcd_enabled = 0;

pthread_t avr_thread;
int avr_thread_running;

// Hardware (see main)
sh1106_t sh1106;
button_t start_sw;
button_t button_sw;
uint32_t vx, vy;
avr_irq_t *adcbase_irq;
button_t encoder_pin;
int encoder_value;

// Graphic
int win_width, win_height;

// Event queue from graphic to avr threads
enum event_type {
    BUTTON_SELECT,
    BUTTON_START,
    VCD_DUMP,
};

struct event_queue_t {
    int index_read;  // updated by read thread
    int index_write; // updated by graphic thread
    struct {
        enum event_type  type;
        int         value;
    } items[8];
} event_queue = {.index_read=0, .index_write=0};

int queue_push(struct event_queue_t *queue, enum event_type type, int value) {
    int index_write_next = (queue->index_write + 1) % ARRAY_SIZE(queue->items);

    if (index_write_next == queue->index_read) {
        fprintf(stderr, "WARNING: Event queue full !\n");
        return 0;
    }
    queue->items[queue->index_write].type=type;
    queue->items[queue->index_write].value=value;

    queue->index_write = index_write_next;
    return 1;
}

int queue_pop(struct event_queue_t *queue, enum event_type *type, int *value) {

    if (queue->index_read == queue->index_write) { //queue empty
        return 0;
    }
    *type = queue->items[queue->index_read].type;
    *value = queue->items[queue->index_read].value;

    queue->index_read = (queue->index_read + 1) % ARRAY_SIZE(queue->items);
    return 1;
}

void vcd_trace_enable(int enable) {
    if (avr->vcd) {
        enable ? avr_vcd_start(avr->vcd) : avr_vcd_stop(avr->vcd);
        printf("VCD trace %s (%s)\n", avr->vcd->filename, enable ? "enabled":"disabled");
    }
}

static void *
avr_thread_start (void *arg)
{
	int state = cpu_Running;
    avr_cycle_count_t lastChange = avr->cycle;
    encoder_value = 0;
    avr_irq_t vcd_irq_pc;

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

	while (avr_thread_running && (state != cpu_Done) && (state != cpu_Crashed)) 
	{
        // Process events from graphic thread
        enum event_type src;
        int value;
        while (queue_pop(&event_queue, &src, &value)) {
            switch (src) {
                case BUTTON_SELECT:
                    avr_raise_irq(button_sw.irq + IRQ_BUTTON_OUT, value);
                    break;
                case BUTTON_START:
                    avr_raise_irq(start_sw.irq + IRQ_BUTTON_OUT, value);
                    break;
                case VCD_DUMP:
                    vcd_trace_enable(vcd_enabled);
                    break;
            }
        }

        // Emulate optical encoder input
        if ( avr_cycles_to_usec(avr, avr->cycle - lastChange)  > (1000000 / 32)) {
            lastChange = avr->cycle;
            encoder_value = !encoder_value;
            avr_raise_irq(encoder_pin.irq + IRQ_BUTTON_OUT, encoder_value ? 1:0);
        }

        // Trigger IRQ for a PC trace
        if (trace_pc && avr->vcd->output) {
            avr_raise_irq(&vcd_irq_pc, avr->pc);
        }

        // Run one AVR cycle
		state = avr_run(avr);
	}

	avr_terminate(avr);

	return NULL;
}

/* Callback for A-D conversion sampling. */
static void adcTriggerCB(struct avr_irq_t *irq, uint32_t value, void *param)
{
    union {
        avr_adc_mux_t request;
        uint32_t      v;
    }   u = { .v = value };

    switch (u.request.src) {
        case ADC_IRQ_ADC6:
            avr_raise_irq(adcbase_irq + 6, vx);
            break;
        case ADC_IRQ_ADC7:
            avr_raise_irq(adcbase_irq + 7, vy);
            break;
        default:
            fprintf(stderr, "Unexpected ADC_IRQ_OUT_TRIGGER request [0x%04x]\n", u.v);
    }
}

/* Called on a key press */
void
keyUpCB (unsigned char key, int x, int y)
{
	switch (key)
	{
		case '\r':
            queue_push(&event_queue, BUTTON_SELECT, 1);
			break;
		case ' ':
            queue_push(&event_queue, BUTTON_START, 1);
			break;
	}
}

void
keyCB (unsigned char key, int x, int y)
{
	switch (key)
	{
		case 0x1b:
		case 'q':
            // Terminate the AVR thread ...
            avr_thread_running = 0;
            pthread_join(avr_thread, NULL); 
            // ... and exit
            exit(0);
			break;
		case 'v':
            if (avr->vcd) {
                vcd_enabled = ! vcd_enabled;
                queue_push(&event_queue, VCD_DUMP, vcd_enabled);
            }
			break;
		case '\r':
            queue_push(&event_queue, BUTTON_SELECT, 0);
			break;
		case ' ':
            queue_push(&event_queue, BUTTON_START, 0);
			break;
		default:
			break;
	}
}

void
specialkeyUpCB (int key, int x, int y)
{
	switch (key)
    {
		case GLUT_KEY_UP:
		case GLUT_KEY_DOWN:
            vx = 1650;
            break;
		case GLUT_KEY_LEFT:
		case GLUT_KEY_RIGHT:
            vy = 1650;
            break;
		default:
			break;
    }
}

void
specialkeyCB (int key, int x, int y)
{
	switch (key)
    {
		case GLUT_KEY_UP:
            vx = 0;
            break;
		case GLUT_KEY_DOWN:
            vx = 3300;
            break;
		case GLUT_KEY_LEFT:
            vy = 0;
            break;
		case GLUT_KEY_RIGHT:
            vy = 3300;
            break;
		default:
			break;
    }
}

/* Function called whenever redisplay needed */
void
displayCB (void)
{
	const uint8_t seg_remap_default = 1 - sh1106_get_flag (
	                &sh1106, SH1106_FLAG_SEGMENT_REMAP_0);
	const uint8_t seg_comscan_default = 1 - sh1106_get_flag (
	                &sh1106, SH1106_FLAG_COM_SCAN_NORMAL);

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set up projection matrix
	glMatrixMode (GL_PROJECTION);
	// Start with an identity matrix
	glLoadIdentity ();
	glOrtho (0, win_width, 0, win_height, 0, 10);
	// Apply vertical and horizontal display mirroring
	glScalef (seg_remap_default ? 1 : -1, seg_comscan_default ? -1 : 1, 1);
	glTranslatef (seg_remap_default ? 0 : -win_width, seg_comscan_default ? -win_height : 0, 0);

	// Select modelview matrix
	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	// Start with an identity matrix
	glLoadIdentity ();
	sh1106_gl_draw (&sh1106);
	glPopMatrix ();
	glutSwapBuffers ();
}

// gl timer. if the lcd is dirty, refresh display
void
timerCB (int i)
{
	// restart timer
	glutTimerFunc (1000 / 64, timerCB, 0);
	glutPostRedisplay ();
}

int
initGL (int w, int h, float pix_size)
{
	win_width = w * pix_size;
	win_height = h * pix_size;

	// Double buffered, RGB disp mode.
	glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize (win_width, win_height);
	glutCreateWindow ("SH1106 132x64 OLED");

	// Set window's display callback
	glutDisplayFunc (displayCB);
	// Set window's key callback
	glutKeyboardFunc (keyCB);
	glutKeyboardUpFunc (keyUpCB);
    glutSpecialFunc (specialkeyCB);
    glutSpecialUpFunc(specialkeyUpCB);
    glutIgnoreKeyRepeat(1);

	glutTimerFunc (1000 / 24, timerCB, 0);

	sh1106_gl_init (pix_size, SH1106_GL_WHITE);

	return 1;
}

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
	 "       [-v]                Raise verbosity level\n"
	 "                           (can be passed more than once)\n"
	 "       [--freq|-f <freq>]  Sets the frequency for an .hex firmware\n"
	 "       [--mcu|-m <device>] Sets the MCU type for an .hex firmware\n"
	 "       [--gdb|-g [<port>]] Listen for gdb connection on <port> "
	 "(default 1234)\n"
	 "       [--output|-o <file>] VCD file to save signal traces\n"
	 "       [--start-vcd|-s     Start VCD output from reset\n"
	 "       [--pc-trace|-p      Add PC to VCD traces\n"
	 "       [--add-trace|-at <name=[portpin|irq|trace]@addr/mask or [sram8|sram16]@addr>]\n"
	 "                           Add signal to be included in VCD output\n"
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
		} else if (!strcmp(argv[pi], "-at") ||
				   !strcmp(argv[pi], "--add-trace")) {
			if (pi + 1 >= argc) {
				fprintf(stderr, "%s: missing mandatory argument for %s.\n", argv[0], argv[pi]);
				exit(1);
			}
			++pi;
			struct {
				char     kind[64];
				uint8_t  mask;
				uint16_t addr;
				char     name[64];
			} trace;
			const int n_args = sscanf(
				argv[pi],
				"%63[^=]=%63[^@]@0x%hx/0x%hhx",
				&trace.name[0],
				&trace.kind[0],
				&trace.addr,
				&trace.mask
			);
            switch (n_args) {
                case 4:
                    break;
                case 3:
                    if (!strcmp(trace.kind, "sram8") || !strcmp(trace.kind, "sram16")) {
                        break;
                    }
                   [[fallthrough]];
                default:
                    --pi;
                    fprintf(stderr, "%s: format for %s is name=kind@addr</mask>.\n", argv[0], argv[pi]);
                    exit(1);
            }

			if (!strcmp(trace.kind, "portpin")) {
				firmware.trace[firmware.tracecount].kind = AVR_MMCU_TAG_VCD_PORTPIN;
			} else if (!strcmp(trace.kind, "irq")) {
				firmware.trace[firmware.tracecount].kind = AVR_MMCU_TAG_VCD_IRQ;
			} else if (!strcmp(trace.kind, "trace")) {
				firmware.trace[firmware.tracecount].kind = AVR_MMCU_TAG_VCD_TRACE;
			} else if (!strcmp(trace.kind, "sram8")) {
                    firmware.trace[firmware.tracecount].kind = AVR_MMCU_TAG_VCD_SRAM_8;
			} else if (!strcmp(trace.kind, "sram16")) {
                    firmware.trace[firmware.tracecount].kind = AVR_MMCU_TAG_VCD_SRAM_16;
			} else {
				fprintf(
					stderr,
					"%s: unknown trace kind '%s', not one of 'portpin', 'irq', 'trace', 'sram8' or 'sram16'.\n",
					argv[0],
					trace.kind
				);
				exit(1);
			}
			firmware.trace[firmware.tracecount].mask = trace.mask;
			firmware.trace[firmware.tracecount].addr = trace.addr;
			strncpy(firmware.trace[firmware.tracecount].name, trace.name, sizeof(firmware.trace[firmware.tracecount].name));

			printf(
				"ARGS: Adding %s trace on address 0x%04x, mask 0x%02x ('%s')\n",
				  firmware.trace[firmware.tracecount].kind == AVR_MMCU_TAG_VCD_PORTPIN ? "portpin"
				: firmware.trace[firmware.tracecount].kind == AVR_MMCU_TAG_VCD_IRQ     ? "irq"
				: firmware.trace[firmware.tracecount].kind == AVR_MMCU_TAG_VCD_TRACE   ? "trace"
				: firmware.trace[firmware.tracecount].kind == AVR_MMCU_TAG_VCD_SRAM_8  ? "sram8"
				: firmware.trace[firmware.tracecount].kind == AVR_MMCU_TAG_VCD_SRAM_16 ? "sram16"
				: "unknown",
				firmware.trace[firmware.tracecount].addr,
				firmware.trace[firmware.tracecount].mask,
				firmware.trace[firmware.tracecount].name
			);

			++firmware.tracecount;
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

int
main (int argc, char *argv[])
{
	printf (
        "---------------------------------------------------------\n"
        "SH1106 OLED demo (navigation menu for a motor controller)\n"
        "- Use arrows to navigate menu\n"
        "- <enter> to select,\n"
        "- <space> to start,pause or stop (long press) motor\n"
        "- 'v' to start/stop VCD traces"
        "- 'q' or ESC to quit\n\n"
        "NOTE: Emulation may be slightly slower than realtime and\n"
        "      a keypress has to be long enough to cope with that\n"
        "----------------------------------------------------------\n"
    );

    parse_arguments(argc, argv);

	avr = avr_make_mcu_by_name(firmware.mmcu);
	if (!avr) {
		fprintf (stderr, "%s: AVR '%s' not known\n", argv[0], firmware.mmcu);
		exit (1);
	}

	avr_init (avr);
	avr->log = (loglevel > LOG_TRACE ? LOG_TRACE : loglevel);

	avr_load_firmware (avr, &firmware);
	if (firmware.flashbase) {
		printf("Attempt to load a bootloader at %04x\n", firmware.flashbase);
		avr->pc = firmware.flashbase;
	}

    avr->gdb_port = gdb_port;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

    avr_gdb_init(avr);

    // System Hardware Description
    /* SH1106 I2C 132x64 OLED Display
    */
	sh1106_init (avr, &sh1106, 132, 64);
	sh1106_connect_twi (&sh1106, NULL); //TODO: specify SCL/SDA pins in wiring ?

    /* Joystick Navigation
       - X & Y analog inputs connected to A1 (ADC6 on 32u4) & A0 (ADC7 on 32u4) respectively
       - Push button input connected to D4 (PD.4 on 32u4)
    */
    vx = vy = 1650; // 3V3 -> 1650mV = center
    adcbase_irq = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, 0);
    avr_irq_register_notify(adcbase_irq + ADC_IRQ_OUT_TRIGGER, adcTriggerCB, NULL);

    button_init(avr, &button_sw, "Button Joystick");
    avr_connect_irq(
        button_sw.irq + IRQ_BUTTON_OUT,
        avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), 4)
    );

    /* Start button with led
       - switch input connected to D11 (PB.7 on 32u4)
       - led output connected to D12 (PD.6 on 32u4)
    */
    button_init(avr, &start_sw, "Button Start");
    avr_connect_irq(
        start_sw.irq + IRQ_BUTTON_OUT,
        avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 7)
    );

    /* Optical detector input
       - Detector input connected to D0 (PD.2 on 32u4)
    */
    button_init(avr, &encoder_pin, "Encoder output");
    avr_connect_irq(
        encoder_pin.irq + IRQ_BUTTON_OUT,
        avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), 2)
    );

    /* Motor outputs
        - PWM output -> D6 (PD.7 on 32u4)
        - Enable outputs -> D5 & D7 (PC.6 & PE.6 on 32u4)
    */

    // Start AVR thread (Graphic subsystem uses the main thread)
    avr_thread_running = 1;
	pthread_create (&avr_thread, NULL, avr_thread_start, NULL);

	// Initialize & run Graphic/GLUT system
	glutInit (&argc, argv);
	initGL (sh1106.columns, sh1106.rows, 2.0);

	glutMainLoop ();
}
